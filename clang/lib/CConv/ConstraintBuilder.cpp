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

#include "clang/CConv/ConstraintBuilder.h"
#include "clang/CConv/ConstraintResolver.h"
#include "clang/CConv/ArrayBoundsInferenceConsumer.h"
#include "clang/CConv/CCGlobalOptions.h"

using namespace llvm;
using namespace clang;

// This class visits functions and adds constraints to the
// Constraints instance assigned to it.
// Each VisitXXX method is responsible either for looking inside statements
// to find constraints
// The results of this class are returned via the ProgramInfo
// parameter to the user.
class FunctionVisitor : public RecursiveASTVisitor<FunctionVisitor> {
public:
  explicit FunctionVisitor(ASTContext *C, ProgramInfo &I, FunctionDecl *FD)
      : Context(C), Info(I), Function(FD), CB(Info, Context) {}

  // T x = e
  bool VisitDeclStmt(DeclStmt *S) {
    // Introduce variables as needed.
    for (const auto &D : S->decls())
      if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
        if (VD->isLocalVarDecl()) {
          /* FIXME: Are the following three lines really necessary?
           * We don't seem to have these shorts of checks elsewhere. */
          FullSourceLoc FL = Context->getFullLoc(VD->getBeginLoc());
          SourceRange SR = VD->getSourceRange();
          if (SR.isValid() && FL.isValid() && !FL.isInSystemHeader() &&
              (VD->getType()->isPointerType() ||
               VD->getType()->isArrayType())) {
            Info.addVariable(VD, Context);
          }
        }
      }

    // FIXME: Merge into the loop above; but: we should process inits
    //   even for non-pointers because things like structs and unions
    //   can contain pointers
    for (const auto &D : S->decls()) {
      if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
        Expr *InitE = VD->getInit();
        CB.constrainLocalAssign(S, VD, InitE);
      }
    }

    return true;
  }

  // TODO: other visitors to visit statements and expressions that we use to
  // Gather constraints.

  // (T)e
  bool VisitCStyleCastExpr(CStyleCastExpr *C) {
    // Is cast compatible with LHS type?
    if (!isExplicitCastSafe(C->getType(), C->getSubExpr()->getType())) {
      auto CVs = CB.getExprConstraintVars(C, C->getType(), true);
      CB.constraintAllCVarsToWild(CVs, "Casted to a different type.", C);
    }
    return true;
  }

  // x += e
  bool VisitCompoundAssignOperator(CompoundAssignOperator *O) {
    switch(O->getOpcode()) {
    case BO_AddAssign:
    case BO_SubAssign:
      arithBinop(O);
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
    std::set<ConstraintVariable *> FVCons;
    if (D == nullptr) {
      // If the callee declaration could not be found, then we're doing some
      // sort of indirect call through an array or conditional. FV constraints
      // can be obtained for this from getExprConstraintVars.
      Expr *CalledExpr = E->getCallee();
      FVCons = CB.getExprConstraintVars(CalledExpr, CalledExpr->getType());

      // When multiple function variables are used in the same expression, they
      // must have the same type.
      if(FVCons.size() > 1) {
        PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(CalledExpr, *Context);
        constrainConsVarGeq(FVCons, FVCons, Info.getConstraints(), &PL,
                            Same_to_Same, false, false, &Info);
      }

      handleFunctionCall(E, FVCons);
    } else if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
      // Get the function declaration, if exists
      if (getDeclaration(FD) != nullptr) {
        FD = getDeclaration(FD);
      }
      FVCons = Info.getVariable(FD, Context);

      handleFunctionCall(E, FVCons);
    } else if (DeclaratorDecl *DD = dyn_cast<DeclaratorDecl>(D)) {
      // This could be a function pointer,
      // get the declaration of the function pointer variable
      // with in the caller context.
      FVCons = Info.getVariable(DD, Context);
      handleFunctionCall(E, FVCons);
    } else {
      // Constrain all arguments to wild.
      constraintAllArgumentsToWild(E);
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
    std::set<ConstraintVariable *> Fun =
        Info.getVariable(Function, Context);

    // Constrain the value returned (if present) against the return value
    // of the function.
    Expr *RetExpr = S->getRetValue();
    QualType Typ = Function->getReturnType();

    std::set<ConstraintVariable *> RconsVar =
        CB.getExprConstraintVars(RetExpr, Typ, true);
    // Constrain the return type of the function
    // to the type of the return expression.
    for (const auto &F : Fun) {
      if (FVConstraint *FV = dyn_cast<FVConstraint>(F)) {
        // This is to ensure that the return type of the function is same
        // as the type of return expression.
        constrainConsVarGeq(FV->getReturnVars(), RconsVar,
                            Info.getConstraints(), &PL, Same_to_Same, false,
                            false, &Info);
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

  bool handleFunctionCall(CallExpr *E,
                          std::set<ConstraintVariable *> &FuncCVars) {
    PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(E, *Context);
    auto &CS = Info.getConstraints();
    if (!FuncCVars.empty()) {
      // Constrain arguments to be of the same type
      // as the corresponding parameters.
      unsigned i = 0;
      for (const auto &A : E->arguments()) {
        std::set<ConstraintVariable *> ArgumentConstraints =
            CB.getExprConstraintVars(A, A->getType(), true);
        for (auto *TmpC : FuncCVars) {
          if (PVConstraint *PVC = dyn_cast<PVConstraint>(TmpC)) {
            TmpC = PVC->getFV();
            assert(TmpC != nullptr &&
                   "Function pointer with null FVConstraint.");
          }
          if (FVConstraint *TargetFV = dyn_cast<FVConstraint>(TmpC)) {

            if (i < TargetFV->numParams()) {
              std::set<ConstraintVariable *> ParameterDC =
                  TargetFV->getParamVar(i);
              constrainConsVarGeq(ParameterDC, ArgumentConstraints, CS, &PL,
                                  Wild_to_Safe, false, false, &Info);
            } else {
              // This is the case of an argument passed to a function
              // with varargs.
              // Constrain this parameter to be wild.
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
          }
        }
        i++;
      }
    }  else {
      // Constraints for the function call are empty.
      // Constrain all arguments of the function call to wild.
      constraintAllArgumentsToWild(E);
    }
    return true;
  }

  // Constraint all the provided vars to be
  // equal to the provided type i.e., (V >= type).
  void constrainVarsTo(std::set<ConstraintVariable *> &Vars,
                       ConstAtom *CAtom) {
    Constraints &CS = Info.getConstraints();
    for (const auto &I : Vars)
      if (PVConstraint *PVC = dyn_cast<PVConstraint>(I)) {
        PVC->constrainOuterTo(CS, CAtom);
      }
  }

  // Constraint helpers.
  void constraintInBodyVariable(Expr *e, ConstAtom *CAtom) {
    std::set<ConstraintVariable *> Var =
        CB.getExprConstraintVars(e, e->getType());
    constrainVarsTo(Var, CAtom);
  }

  void constraintInBodyVariable(Decl *d, ConstAtom *CAtom) {
    std::set<ConstraintVariable *> Var = Info.getVariable(d, Context);
    constrainVarsTo(Var, CAtom);
  }

  // Constraint all the argument of the provided
  // call expression to be WILD.
  void constraintAllArgumentsToWild(CallExpr *E) {
    PersistentSourceLoc psl = PersistentSourceLoc::mkPSL(E, *Context);
    for (const auto &A : E->arguments()) {
      // Get constraint from within the function body
      // of the caller.
      std::set<ConstraintVariable *> ParameterEC =
          CB.getExprConstraintVars(A, A->getType());

      // Assign WILD to each of the constraint variables.
      FunctionDecl *FD = E->getDirectCallee();
      std::string Rsn = "Argument to function " +
                        (FD != nullptr ? FD->getName().str() : "pointer call");
      Rsn += " with out Constraint vars.";
      CB.constraintAllCVarsToWild(ParameterEC, Rsn, E);
    }
  }

  void arithBinop(BinaryOperator *O) {
      constraintPointerArithmetic(O->getLHS());
      constraintPointerArithmetic(O->getRHS());
  }

  // Pointer arithmetic constrains the expression to be at least ARR,
  // unless it is on a function pointer. In this case the function pointer
  // is WILD.
  void constraintPointerArithmetic(Expr *E) {
    if (E->getType()->isFunctionPointerType()) {
      std::set<ConstraintVariable *> Var =
          CB.getExprConstraintVars(E, E->getType());
      std::string Rsn = "Pointer arithmetic performed on a function pointer.";
      CB.constraintAllCVarsToWild(Var, Rsn, E);
    } else {
      constraintInBodyVariable(E, Info.getConstraints().getArr());
    }
  }

  ASTContext *Context;
  ProgramInfo &Info;
  FunctionDecl *Function;
  ConstraintResolver CB;
};

// This class visits a global declaration and either
// - Builds an _enviornment_ and _constraints_ for each function
// - Builds _constraints_ for declared struct/records in the translation unit
// The results are returned in the ProgramInfo parameter to the user.
class GlobalVisitor : public RecursiveASTVisitor<GlobalVisitor> {
public:
  explicit GlobalVisitor(ASTContext *Context, ProgramInfo &I)
      : Context(Context), Info(I), CB(Info, Context) {}

  bool VisitVarDecl(VarDecl *G) {

    if (G->hasGlobalStorage() &&
        (G->getType()->isPointerType() || G->getType()->isArrayType())) {
      // If the location of the previous RecordDecl and the current VarDecl are the same,
      // this implies an inline struct as per Clang's AST, so set a flag in ProgramInfo
      // to indicate that this variable should be constrained to wild later
      Info.addVariable(G, Context);
      if (G->hasInit()) {
        CB.constrainLocalAssign(nullptr, G, G->getInit());
      }
      if(InLineStructEncountered) {
        const std::set<ConstraintVariable *> &C = Info.getVariable(G, Context);
        for(const auto &Var : C) {
          std::string Rsn = "Variable " + G->getNameAsString() + " is an inline struct definition.";
          Var->constrainToWild(Info.getConstraints(), Rsn);
        }
      }
    }

    return true;
  }

  bool VisitFunctionDecl(FunctionDecl *D) {
    FullSourceLoc FL = Context->getFullLoc(D->getBeginLoc());

    if (Verbose)
      errs() << "Analyzing function " << D->getName() << "\n";

    if (FL.isValid()) { // TODO: When would this ever be false?
      Info.addVariable(D, Context);
      if (D->hasBody() && D->isThisDeclarationADefinition()) {
        Stmt *Body = D->getBody();
        FunctionVisitor FV = FunctionVisitor(Context, Info, D);
        FV.TraverseStmt(Body);
        AddArrayHeuristics(Context, Info, D);
      }
    }

    if (Verbose)
      errs() << "Done analyzing function\n";

    return true;
  }

  bool VisitRecordDecl(RecordDecl *Declaration) {
    if (RecordDecl *Definition = Declaration->getDefinition()) {

      //store the current record's location to cross reference later in a VarDecl
      InLineStructEncountered =
              Definition->getBeginLoc().getRawEncoding() == Declaration->getNextDeclInContext()->getBeginLoc().getRawEncoding();

      FullSourceLoc FL = Context->getFullLoc(Definition->getBeginLoc());

      if (FL.isValid() && !FL.isInSystemHeader()) {
        SourceManager &SM = Context->getSourceManager();
        FileID FID = FL.getFileID();
        const FileEntry *FE = SM.getFileEntryForID(FID);

        if (FE && FE->isValid()) {
          // We only want to re-write a record if it contains
          // any pointer types, to include array types. 
          // Most record types probably do,
          // but let's scan it and not consider any records
          // that don't have any pointers or arrays. 

          for (const auto &D : Definition->fields())
            if (D->getType()->isPointerType() || D->getType()->isArrayType()) {
              Info.addVariable(D, Context);
            }
        }
      }
    }

    return true;
  }

private:
  ASTContext *Context;
  ProgramInfo &Info;
  ConstraintResolver CB;
  bool InLineStructEncountered;
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
  GlobalVisitor GV = GlobalVisitor(&C, Info);
  TranslationUnitDecl *TUD = C.getTranslationUnitDecl();
  // Generate constraints.
  for (const auto &D : TUD->decls()) {
    GV.TraverseDecl(D);
  }

  if (Verbose)
    outs() << "Done analyzing\n";

  Info.exitCompilationUnit();
  return;
}
