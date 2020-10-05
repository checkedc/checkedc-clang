//=--ConstraintBuilder.cpp----------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of visitor methods for the FunctionVisitor class. These
// visitors create constraints based on the AST of the program.
//===----------------------------------------------------------------------===//

#include <clang/ASTMatchers/ASTMatchers.h>
#include "clang/CConv/ConstraintBuilder.h"
#include "clang/CConv/ConstraintResolver.h"
#include "clang/CConv/ArrayBoundsInferenceConsumer.h"
#include "clang/CConv/CCGlobalOptions.h"
#include "clang/CConv/TypeVariableAnalysis.h"

using namespace llvm;
using namespace clang;

// Used to keep track of in-line struct defs
unsigned int lastRecordLocation = -1;

void processRecordDecl(RecordDecl *Declaration, ProgramInfo &Info,
                       ASTContext *Context, ConstraintResolver CB,
                       bool IsInFunction) {
  if (RecordDecl *Definition = Declaration->getDefinition()) {
    // store current record's location to cross-ref later in a VarDecl
    lastRecordLocation = Definition->getBeginLoc().getRawEncoding();
    FullSourceLoc FL = Context->getFullLoc(Definition->getBeginLoc());
    if (FL.isValid()) {
      SourceManager &SM = Context->getSourceManager();
      FileID FID = FL.getFileID();
      const FileEntry *FE = SM.getFileEntryForID(FID);

      //detect whether this RecordDecl is part of an inline struct
      bool IsInLineStruct = false;
      Decl *D = Declaration->getNextDeclInContext();
      if (VarDecl *VD = dyn_cast_or_null<VarDecl>(D)) {
        auto VarTy = VD->getType();
        unsigned int BeginLoc = VD->getBeginLoc().getRawEncoding();
        unsigned int EndLoc = VD->getEndLoc().getRawEncoding();
        IsInLineStruct = !(VarTy->isPointerType() || VarTy->isArrayType()) &&
                         !VD->hasInit() &&
                         lastRecordLocation >= BeginLoc &&
                         lastRecordLocation <= EndLoc;
      }
      if (FE && FE->isValid()) {
        // We only want to re-write a record if it contains
        // any pointer types, to include array types.
        for (const auto &F : Definition->fields()) {
          auto FieldTy = F->getType();
          // If the RecordDecl is a union or in a system header
          // and this field is a pointer, we need to mark it wild;
          bool FieldInUnionOrSysHeader =
              (FL.isInSystemHeader() || Definition->isUnion());
          // mark field wild if the above is true and the field is a pointer
          if ((FieldTy->isPointerType() || FieldTy->isArrayType()) &&
              (FieldInUnionOrSysHeader || IsInLineStruct)) {
            std::string Rsn = "External struct field or union encountered";
            CVarOption CV = Info.getVariable(F, Context);
            CB.constraintCVarToWild(CV, Rsn);
          }
        }
      }
    }
  }
}

// This class visits functions and adds constraints to the
// Constraints instance assigned to it.
// Each VisitXXX method is responsible for looking inside statements
// and imposing constraints on variables it uses
class FunctionVisitor : public RecursiveASTVisitor<FunctionVisitor> {
public:
  explicit FunctionVisitor(ASTContext *C,
                           ProgramInfo &I,
                           FunctionDecl *FD,
                           TypeVarInfo &TVI)
      : Context(C), Info(I), Function(FD), CB(Info, Context), TVInfo(TVI) {}

  // T x = e
  bool VisitDeclStmt(DeclStmt *S) {
    // Introduce variables as needed.
    for (const auto &D : S->decls()) {
      if(RecordDecl *RD = dyn_cast<RecordDecl>(D)) {
        processRecordDecl(RD, Info, Context, CB, true);
      }
      if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
        if (VD->isLocalVarDecl()) {
          FullSourceLoc FL = Context->getFullLoc(VD->getBeginLoc());
          SourceRange SR = VD->getSourceRange();
          if (SR.isValid() && FL.isValid() &&
              (VD->getType()->isPointerType() ||
               VD->getType()->isArrayType())) {
            if (lastRecordLocation == VD->getBeginLoc().getRawEncoding()) {
              CVarOption CV = Info.getVariable(VD, Context);
              CB.constraintCVarToWild(CV, "Inline struct encountered.");
            }
          }
        }
      }
    }

    // Process inits even for non-pointers because structs and union values
    // can contain pointers
    for (const auto &D : S->decls()) {
      if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
        Expr *InitE = VD->getInit();
        CB.constrainLocalAssign(S, VD, InitE);
      }
    }

    return true;
  }

  // (T)e
  bool VisitCStyleCastExpr(CStyleCastExpr *C) {
    // Is cast compatible with LHS type?
    QualType SrcT = C->getSubExpr()->getType();
    QualType DstT = C->getType();
    if (!isCastSafe(DstT, SrcT) && !Info.hasPersistentConstraints(C, Context)) {
      auto CVs = CB.getExprConstraintVars(C->getSubExpr());
      std::string Rsn = "Cast from " + SrcT.getAsString() +  " to " +
                        DstT.getAsString();
      CB.constraintAllCVarsToWild(CVs, Rsn, C);
    }
    return true;
  }

  // x += e
  bool VisitCompoundAssignOperator(CompoundAssignOperator *O) {
    switch(O->getOpcode()) {
    case BO_AddAssign:
    case BO_SubAssign:
      arithBinop(O, true);
      break;
    // rest shouldn't happen on pointers, so we ignore
    default:
      break;
    }
    return true;
  }

  // x = e
  bool VisitBinAssign(BinaryOperator *O) {
    Expr *LHS = O->getLHS();
    Expr *RHS = O->getRHS();
    CB.constrainLocalAssign(O, LHS, RHS, Same_to_Same);
    return true;
  }

  // e(e1,e2,...)
  bool VisitCallExpr(CallExpr *E) {
    Decl *D = E->getCalleeDecl();
    PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(E, *Context);
    auto &CS = Info.getConstraints();
    CVarSet FVCons;
    std::string FuncName = "";
    FunctionDecl *TFD = nullptr;

    // figure out who we are calling
    if (D == nullptr) {
      // If the callee declaration could not be found, then we're doing some
      // sort of indirect call through an array or conditional.
      Expr *CalledExpr = E->getCallee();
      FVCons = CB.getExprConstraintVars(CalledExpr);
      // When multiple function variables are used in the same expression, they
      // must have the same type.
      if (FVCons.size() > 1) {
        PersistentSourceLoc PL =
            PersistentSourceLoc::mkPSL(CalledExpr, *Context);
        constrainConsVarGeq(FVCons, FVCons, Info.getConstraints(), &PL,
                            Same_to_Same, false, &Info);
      }
    } else if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
      FuncName = FD->getNameAsString();
      TFD = FD;
      CVarOption CV = Info.getVariable(FD, Context);
      if (CV.hasValue())
        FVCons.insert(&CV.getValue());
    } else if (DeclaratorDecl *DD = dyn_cast<DeclaratorDecl>(D)) {
      FuncName = DD->getNameAsString();
      CVarOption CV = Info.getVariable(DD, Context);
      if (CV.hasValue())
        FVCons.insert(&CV.getValue());
    }

    // Collect type parameters for this function call that are
    // consistently instantiated as single type in this function call.
    std::set<unsigned int> consistentTypeParams;
    if (TFD != nullptr)
      TVInfo.getConsistentTypeParams(E, consistentTypeParams);

    // Now do the call: Constrain arguments to parameters (but ignore returns)
    if (FVCons.empty()) {
      // Don't know who we are calling; make args WILD
      constraintAllArgumentsToWild(E);
    } else if (!ConstraintResolver::canFunctionBeSkipped(FuncName)) {
      // FIXME: realloc comparison is still required. See issue #176.
      // If we are calling realloc, ignore it, so as not to constrain the first arg
      // Else, for each function we are calling ...
      for (auto *TmpC : FVCons) {
        if (PVConstraint *PVC = dyn_cast<PVConstraint>(TmpC)) {
          TmpC = PVC->getFV();
          assert(TmpC != nullptr && "Function pointer with null FVConstraint.");
        }
        // and for each arg to the function ...
        if (FVConstraint *TargetFV = dyn_cast<FVConstraint>(TmpC)) {
          unsigned i = 0;
          bool callUntyped =
            TFD ?
              TFD->getType()->isFunctionNoProtoType() &&
              E->getNumArgs() != 0 && TargetFV->numParams() == 0
            :
              false;

          std::vector<CVarSet> deferred;
          for (const auto &A : E->arguments()) {
            CVarSet ArgumentConstraints;
            if(TFD != nullptr && i < TFD->getNumParams()) {
              // Remove casts to void* on polymorphic types that are used
              // consistently.
              const auto *Ty = getTypeVariableType(TFD->getParamDecl(i));
              if (Ty != nullptr && consistentTypeParams.find(Ty->GetIndex())
                  != consistentTypeParams.end())
                ArgumentConstraints =
                    CB.getExprConstraintVars(A->IgnoreImpCasts());
              else
                ArgumentConstraints = CB.getExprConstraintVars(A);
            } else
              ArgumentConstraints = CB.getExprConstraintVars(A);


            if (callUntyped) {
              deferred.push_back(ArgumentConstraints);
            } else if (i < TargetFV->numParams()) {
              // constrain the arg CV to the param CV
              ConstraintVariable *ParameterDC = TargetFV->getParamVar(i);
              // Do not handle bounds key here because we will be
              // doing context-sensitive assignment next.
              constrainConsVarGeq(ParameterDC, ArgumentConstraints, CS, &PL,
                                  Wild_to_Safe, false, &Info, false);
              
              if (AllTypes && TFD != nullptr) {
                auto *PVD = TFD->getParamDecl(i);
                auto &ABI = Info.getABoundsInfo();
                // Here, we need to handle context-sensitive assignment.
                ABI.handleContextSensitiveAssignment(E, PVD, ParameterDC, A,
                                                  ArgumentConstraints,
                                                     Context, &CB);
              }
            } else {
              // The argument passed to a function ith varargs; make it wild
              if (HandleVARARGS) {
                CB.constraintAllCVarsToWild(ArgumentConstraints,
                                            "Passing argument to a function "
                                            "accepting var args.",
                                            E);
              } else {
                if (Verbose) {
                  std::string FuncName = TargetFV->getName();
                  errs() << "Ignoring function as it contains varargs:"
                         << FuncName << "\n";
                }
              }
            }
            i++;
          }
          if (callUntyped)
            TargetFV->addDeferredParams(PL, deferred);

        }
      }
    }
    return true;
  }


  // e1[e2]
  bool VisitArraySubscriptExpr(ArraySubscriptExpr *E) {
    Constraints &CS = Info.getConstraints();
    constraintInBodyVariable(E->getBase(), CS.getArr());
    return true;
  }

  // return e;
  bool VisitReturnStmt(ReturnStmt *S) {
    // Get function variable constraint of the body
    PersistentSourceLoc PL =
        PersistentSourceLoc::mkPSL(S, *Context);
    CVarOption CVOpt = Info.getVariable(Function, Context);

    // Constrain the value returned (if present) against the return value
    // of the function.
    Expr *RetExpr = S->getRetValue();

    CVarSet RconsVar = CB.getExprConstraintVars(RetExpr);
    // Constrain the return type of the function
    // to the type of the return expression.
    if (CVOpt.hasValue()) {
      if (FVConstraint *FV = dyn_cast<FVConstraint>(&CVOpt.getValue())) {
        // This is to ensure that the return type of the function is same
        // as the type of return expression.
        constrainConsVarGeq(FV->getReturnVar(), RconsVar, Info.getConstraints(),
                            &PL, Same_to_Same, false, &Info);
      }
    }
    return true;
  }

  // ++x
  bool VisitUnaryPreInc(UnaryOperator *O) {
    constraintPointerArithmetic(O->getSubExpr());
    return true;
  }

  // x++
  bool VisitUnaryPostInc(UnaryOperator *O) {
    constraintPointerArithmetic(O->getSubExpr());
    return true;
  }

  // --x
  bool VisitUnaryPreDec(UnaryOperator *O) {
    constraintPointerArithmetic(O->getSubExpr());
    return true;
  }

  // x--
  bool VisitUnaryPostDec(UnaryOperator *O) {
    constraintPointerArithmetic(O->getSubExpr());
    return true;
  }

  // e1 + e2
  bool VisitBinAdd(BinaryOperator *O) {
    arithBinop(O);
    return true;
  }

  // e1 - e2
  bool VisitBinSub(BinaryOperator *O) {
    arithBinop(O);
    return true;
  }

private:

  // Constraint all the provided vars to be
  // equal to the provided type i.e., (V >= type).
  void constrainVarsTo(CVarSet &Vars,
                       ConstAtom *CAtom) {
    Constraints &CS = Info.getConstraints();
    for (const auto &I : Vars)
      if (PVConstraint *PVC = dyn_cast<PVConstraint>(I)) {
        PVC->constrainOuterTo(CS, CAtom);
      }
  }

  // Constraint helpers.
  void constraintInBodyVariable(Expr *e, ConstAtom *CAtom) {
    CVarSet Var = CB.getExprConstraintVars(e);
    constrainVarsTo(Var, CAtom);
  }

  // Constraint all the argument of the provided
  // call expression to be WILD.
  void constraintAllArgumentsToWild(CallExpr *E) {
    PersistentSourceLoc psl = PersistentSourceLoc::mkPSL(E, *Context);
    for (const auto &A : E->arguments()) {
      // Get constraint from within the function body
      // of the caller.
      CVarSet ParameterEC = CB.getExprConstraintVars(A);

      // Assign WILD to each of the constraint variables.
      FunctionDecl *FD = E->getDirectCallee();
      std::string Rsn = "Argument to function " +
                        (FD != nullptr ? FD->getName().str() : "pointer call");
      Rsn += " with out Constraint vars.";
      CB.constraintAllCVarsToWild(ParameterEC, Rsn, E);
    }
  }

  // Here the flag, ModifyingExpr indicates if the arithmetic operation
  // is modifying any variable.
  void arithBinop(BinaryOperator *O, bool ModifyingExpr = false) {
      constraintPointerArithmetic(O->getLHS(), ModifyingExpr);
      constraintPointerArithmetic(O->getRHS(), ModifyingExpr);
  }

  // Pointer arithmetic constrains the expression to be at least ARR,
  // unless it is on a function pointer. In this case the function pointer
  // is WILD.
  void constraintPointerArithmetic(Expr *E, bool ModifyingExpr = true) {
    if (E->getType()->isFunctionPointerType()) {
      CVarSet Var = CB.getExprConstraintVars(E);
      std::string Rsn = "Pointer arithmetic performed on a function pointer.";
      CB.constraintAllCVarsToWild(Var, Rsn, E);
    } else {
      if (ModifyingExpr)
        Info.getABoundsInfo().recordArithmeticOperation(E, &CB);
      constraintInBodyVariable(E, Info.getConstraints().getArr());
    }
  }

  ASTContext *Context;
  ProgramInfo &Info;
  FunctionDecl *Function;
  ConstraintResolver CB;
  TypeVarInfo &TVInfo;
};


// This class visits a global declaration, generating constraints
//   for functions, variables, types, etc. that are visited
class ConstraintGenVisitor : public RecursiveASTVisitor<ConstraintGenVisitor> {
public:
  explicit ConstraintGenVisitor(ASTContext *Context, ProgramInfo &I, TypeVarInfo &TVI)
      : Context(Context), Info(I), CB(Info, Context), TVInfo(TVI) {}

  bool VisitVarDecl(VarDecl *G) {

    if (G->hasGlobalStorage() &&
        (G->getType()->isPointerType() || G->getType()->isArrayType())) {
      if (G->hasInit()) {
        CB.constrainLocalAssign(nullptr, G, G->getInit());
      }
      // If the location of the previous RecordDecl and the current VarDecl
      // coincide with one another, we constrain the VarDecl to be wild
      // in order to allow the fields of the RecordDecl to be converted
      unsigned int BeginLoc = G->getBeginLoc().getRawEncoding();
      unsigned int EndLoc = G->getEndLoc().getRawEncoding();
      if (lastRecordLocation >= BeginLoc && lastRecordLocation <= EndLoc) {
        CVarOption CV = Info.getVariable(G, Context);
        CB.constraintCVarToWild(CV, "Inline struct encountered.");
      }
    }

    return true;
  }

  bool VisitInitListExpr(InitListExpr *E){
    if (E->getType()->isStructureType()) {
      const RecordDecl *Definition =
          E->getType()->getAsStructureType()->getDecl()->getDefinition();

      unsigned int initIdx = 0;
      const auto fields = Definition->fields();
      for (auto it = fields.begin(); initIdx < E->getNumInits() && it != fields.end(); initIdx++, it++) {
        Expr *InitExpr = E->getInit(initIdx);
        CB.constrainLocalAssign(nullptr, *it, InitExpr);
      }
    }
    return true;
  }

  bool VisitFunctionDecl(FunctionDecl *D) {
    FullSourceLoc FL = Context->getFullLoc(D->getBeginLoc());

    if (Verbose)
      errs() << "Analyzing function " << D->getName() << "\n";

    if (FL.isValid()) { // TODO: When would this ever be false?
      if (D->hasBody() && D->isThisDeclarationADefinition()) {
        Stmt *Body = D->getBody();
        FunctionVisitor FV = FunctionVisitor(Context, Info, D, TVInfo);
        FV.TraverseStmt(Body);
        if (AllTypes) {
          // Only do this, if all types is enabled.
          LengthVarInference LVI(Info, Context, D);
          LVI.Visit(Body);
        }
      }
    }

    if (Verbose)
      errs() << "Done analyzing function\n";

    return true;
  }

  bool VisitRecordDecl(RecordDecl *Declaration) {
    processRecordDecl(Declaration, Info, Context, CB, false);
    return true;
  }

private:
  ASTContext *Context;
  ProgramInfo &Info;
  ConstraintResolver CB;
  TypeVarInfo &TVInfo;
};

// Some information about variables in the program is required by the type
// variable analysis and constraint building passes. This is gathered by this
// visitor which is executed before both of the other visitors.
class VariableAdderVisitor : public RecursiveASTVisitor<VariableAdderVisitor> {
public:
  explicit VariableAdderVisitor(ASTContext *Context, ProgramVariableAdder &VA)
      : Context(Context), VarAdder(VA) {}

  bool VisitVarDecl(VarDecl *D) {
    FullSourceLoc FL = Context->getFullLoc(D->getBeginLoc());
    if (FL.isValid() && !isa<ParmVarDecl>(D))
      addVariable(D);
    return true;
  }

  bool VisitFunctionDecl(FunctionDecl *D) {
    FullSourceLoc FL = Context->getFullLoc(D->getBeginLoc());
    if (FL.isValid())
      VarAdder.addVariable(D, Context);
    return true;
  }

  bool VisitRecordDecl(RecordDecl *Declaration) {
    if (Declaration->isThisDeclarationADefinition()) {
      RecordDecl *Definition = Declaration->getDefinition();
      assert("Declaration is a definition, but getDefinition() is null?"
                 && Definition);
      FullSourceLoc FL = Context->getFullLoc(Definition->getBeginLoc());
      if (FL.isValid()) {
        SourceManager &SM = Context->getSourceManager();
        FileID FID = FL.getFileID();
        const FileEntry *FE = SM.getFileEntryForID(FID);
        if (FE && FE->isValid())
          for (auto *const D : Definition->fields())
            addVariable(D);
      }
    }
    return true;
  }

private:
  ASTContext *Context;
  ProgramVariableAdder &VarAdder;

  void addVariable(DeclaratorDecl *D) {
    VarAdder.addABoundsVariable(D);
    if (D->getType()->isPointerType() || D->getType()->isArrayType())
      VarAdder.addVariable(D, Context);
  }
};

void ConstraintBuilderConsumer::HandleTranslationUnit(ASTContext &C) {
  Info.enterCompilationUnit(C);
  if (Verbose) {
    SourceManager &SM = C.getSourceManager();
    FileID MainFileId = SM.getMainFileID();
    const FileEntry *FE = SM.getFileEntryForID(MainFileId);
    if (FE != nullptr)
      errs() << "Analyzing file " << FE->getName() << "\n";
    else
      errs() << "Analyzing\n";
  }


  VariableAdderVisitor VAV = VariableAdderVisitor(&C, Info);
  TypeVarVisitor TV = TypeVarVisitor(&C, Info);
  ConstraintResolver CSResolver(Info, &C);
  ContextSensitiveBoundsKeyVisitor CSBV =
      ContextSensitiveBoundsKeyVisitor(&C, Info, &CSResolver);
  ConstraintGenVisitor GV = ConstraintGenVisitor(&C, Info, TV);
  TranslationUnitDecl *TUD = C.getTranslationUnitDecl();
  // Generate constraints.
  for (const auto &D : TUD->decls()) {
    // The order of these traversals CANNOT be changed because both the type
    // variable and constraint gen visitor require that variables have been
    // added to ProgramInfo, and the constraint gen visitor requires the type
    // variable information gathered in the type variable traversal.
    VAV.TraverseDecl(D);
    CSBV.TraverseDecl(D);
    TV.TraverseDecl(D);
    GV.TraverseDecl(D);
  }

  // Store type variable information for use in rewriting
  TV.setProgramInfoTypeVars();

  if (Verbose)
    outs() << "Done analyzing\n";

  Info.exitCompilationUnit();
  return;
}
