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


  // Introduce a variable into the environment.
  bool MyVisitVarDecl(VarDecl *D) {
    if (D->isLocalVarDecl()) {
      FullSourceLoc FL = Context->getFullLoc(D->getBeginLoc());
      SourceRange SR = D->getSourceRange();

      if (SR.isValid() && FL.isValid() && !FL.isInSystemHeader() &&
        (D->getType()->isPointerType() || D->getType()->isArrayType())) {
        // Add the variable with in the function body context.
        Info.addVariable(D, Context);

        CB.specialCaseVarIntros(D);
        // If this is a static array declaration.
        // Make this an array.
        if (D->getType()->isArrayType()) {
          // Try to see if this is a multi-dimensional array?
          // If yes, assign ARR constraint to all the inside vars.
          const clang::Type *TypePtr = D->getType().getTypePtr();
          Constraints &CS = Info.getConstraints();
          std::set<ConstraintVariable *> Var = Info.getVariable(D, Context, true);
          assert(Var.size() == 1 && "Invalid number of ConstraintVariables.");
          auto *PvConstr = dyn_cast<PVConstraint>(*(Var.begin()));
          assert(PvConstr != nullptr && "Constraint variable cannot be nullptr");
          const CAtoms &PtrCVars = PvConstr->getCvars();
          for (Atom *ConsKey : PtrCVars) {
            if (const clang::ArrayType *AT =
                    dyn_cast<clang::ArrayType>(TypePtr)) {
              if (VarAtom *VA = dyn_cast<VarAtom>(ConsKey)) {
                // FIXME: We shouldn't be adding constraints directly. Use constrainOuter
                CS.addConstraint(CS.createGeq(CS.getArr(), VA, false));
              }
              TypePtr = AT->getElementType().getTypePtr();
              continue;
            }
            break;
          }

        }
      }
    }

    return true;
  }

  bool VisitDeclStmt(DeclStmt *S) {
    // Introduce variables as needed.
      for (const auto &D : S->decls())
        if (VarDecl *VD = dyn_cast<VarDecl>(D))
          MyVisitVarDecl(VD);

    // Build rules based on initializers.
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

  bool VisitCStyleCastExpr(CStyleCastExpr *C) {
    // If we're casting from something with a constraint variable to something
    // that isn't a pointer type, we should constrain up.
    CB.getExprConstraintVars(C, C->getSubExpr()->getType(), true);

    return true;
  }

  bool VisitCompoundAssignOperator(CompoundAssignOperator *O) {
    arithBinop(O);
    return true;
  }

  bool VisitBinAssign(BinaryOperator *O) {
    Expr *LHS = O->getLHS();
    Expr *RHS = O->getRHS();
    CB.constrainLocalAssign(O, LHS, RHS, Same_to_Same);
    return true;
  }

  bool VisitCallExpr(CallExpr *E) {
    Decl *D = E->getCalleeDecl();
    if (!D)
      return true;
    PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(E, *Context);
    if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
      // Get the function declaration, if exists
      if (getDeclaration(FD) != nullptr) {
        FD = getDeclaration(FD);
      }
      // Call of a function directly.
      unsigned i = 0;
      for (const auto &A : E->arguments()) {
        // Get constraint variables for the argument
        // from with in the context of the caller body.
        std::set<ConstraintVariable *> ArgumentConstraintVars =
          CB.getExprConstraintVars(A, A->getType(), true, true);

        if (i < FD->getNumParams()) {
          auto *PD = FD->getParamDecl(i);
          // Here, we need to get the constraints of the
          // parameter from the callee's declaration.
          std::set<ConstraintVariable *> ParameterConstraintVars =
              Info.getVariable(PD, Context, false);
          // Add constraint that the arguments are equal to the
          // parameters.
          constrainConsVarGeq(ParameterConstraintVars,
                              ArgumentConstraintVars,
                              Info.getConstraints(), &PL, Wild_to_Safe,
                              false, &Info);
        } else {
          // This is the case of an argument passed to a function
          // with varargs.
          // Constrain this parameter to be wild.
          if (HandleVARARGS) {
            CB.constraintAllCVarsToWild(ArgumentConstraintVars,
                                        "Passing argument to a function "
                                        "accepting var args.", E);
          } else {
            if (Verbose) {
              std::string FuncName = FD->getName();
              errs() << "Ignoring function as it contains varargs:" << FuncName
                     << "\n";
            }
          }
        }

        i++;
      }
    } else if (DeclaratorDecl *DD = dyn_cast<DeclaratorDecl>(D)){
      handleFunctionPointerCall(E);
    } else {
      // Constrain all arguments to wild.
      constraintAllArgumentsToWild(E);
    }
    
    return true;
  }

  // This will add the constraint that
  // variable is an array i.e., (V=ARR).
  bool VisitArraySubscriptExpr(ArraySubscriptExpr *E) {
    Constraints &CS = Info.getConstraints();
    constraintInBodyVariable(E->getBase(), CS.getArr());
    return true;
  }

  bool VisitReturnStmt(ReturnStmt *S) {
    // Get function variable constraint of the body
    // We need to call getVariableOnDemand to avoid auto-correct.
    PersistentSourceLoc PL =
        PersistentSourceLoc::mkPSL(S, *Context);
    std::set<ConstraintVariable *> Fun =
      Info.getVariableOnDemand(Function, Context, true);

    // Constrain the value returned (if present) against the return value
    // of the function.
    Expr *RetExpr = S->getRetValue();
    QualType Typ = Function->getReturnType();

    std::set<ConstraintVariable *> RconsVar = CB.getExprConstraintVars(RetExpr,
                                                             Function->getReturnType(),
                                                             true,
                                                             true);
    // Constrain the return type of the function
    // to the type of the return expression.
    for (const auto &F : Fun) {
      if (FVConstraint *FV = dyn_cast<FVConstraint>(F)) {
        // This is to ensure that the return type of the function is same
        // as the type of return expression.
        constrainConsVarGeq(FV->getReturnVars(), RconsVar,
                            Info.getConstraints(), &PL, Same_to_Same,
                            false, &Info);
      }
    }
    return true;
  }

  // Pointer arithmetic ==> Must have at least an array

  bool VisitUnaryPreInc(UnaryOperator *O) {
    constraintInBodyVariable(O->getSubExpr(),Info.getConstraints().getArr());
    return true;
  }

  bool VisitUnaryPostInc(UnaryOperator *O) {
    constraintInBodyVariable(O->getSubExpr(),Info.getConstraints().getArr());
    return true;
  }

  bool VisitUnaryPreDec(UnaryOperator *O) {
    constraintInBodyVariable(O->getSubExpr(),Info.getConstraints().getArr());
    return true;
  }

  bool VisitUnaryPostDec(UnaryOperator *O) {
    constraintInBodyVariable(O->getSubExpr(),Info.getConstraints().getArr());
    return true;
  }

  bool VisitBinAdd(BinaryOperator *O) {
    arithBinop(O);
    return true;
  }

  bool VisitBinSub(BinaryOperator *O) {
    arithBinop(O);
    return true;
  }

private:

  bool handleFunctionPointerCall(CallExpr *E) {
    Decl *D = E->getCalleeDecl();
    Constraints &CS = Info.getConstraints();
    if (D) {
      PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(E, *Context);
      if (DeclaratorDecl *DD = dyn_cast<DeclaratorDecl>(D)){
        // This could be a function pointer,
        // get the declaration of the function pointer variable
        // with in the caller context.
        std::set<ConstraintVariable *> V =
            Info.getVariable(DD, Context, true);
        if (V.size() > 0) {
          for (const auto &C : V) {
            FVConstraint *FV = nullptr;
            if (PVConstraint *PVC = dyn_cast<PVConstraint>(C)) {
              if (FVConstraint *F = PVC->getFV()) {
                FV = F;
              }
            } else if (FVConstraint *FVC = dyn_cast<FVConstraint>(C)) {
              FV = FVC;
            }

            if (FV) {
              // Constrain arguments to be of the same type
              // as the corresponding parameters.
              unsigned i = 0;
              for (const auto &A : E->arguments()) {
                std::set<ConstraintVariable *> ArgumentConstraints =
                  CB.getExprConstraintVars(A, A->getType(),
                                             true, true);

                if (i < FV->numParams()) {
                  std::set<ConstraintVariable *> ParameterDC =
                    FV->getParamVar(i);
                  constrainConsVarGeq(ParameterDC, ArgumentConstraints, CS, &PL,
                                      Wild_to_Safe, false, &Info);
                } else {
                  // Constrain argument to wild since we can't match it
                  // to a parameter from the type.
                  for (const auto &V : ArgumentConstraints) {
                    std::string argWILD = "Argument to VarArg Function:"+
                                          FV->getName();
                    V->constrainToWild(CS, argWILD, &PL, false);
                  }
                }
                i++;
              }
            } else {
              // This can happen when someone does something really wacky, like
              // cast a char* to a function pointer, then call it. Constrain
              // everything.
              // What we do is, constraint all arguments to wild.
              constraintAllArgumentsToWild(E);
              // Also constraint parameter with-in the body to WILD.
              std::string rsn = "Function pointer to/from non-function "
                                "pointer cast.";
              C->constrainToWild(CS, rsn, &PL, false);
            }
          }
        } else {
          // Constrain all arguments to wild.
          constraintAllArgumentsToWild(E);
        }
      }
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
      CB.getExprConstraintVars(e, e->getType(), true);
    constrainVarsTo(Var, CAtom);
  }

  void constraintInBodyVariable(Decl *d, ConstAtom *CAtom) {
    std::set<ConstraintVariable *> Var =
      Info.getVariable(d, Context, true);
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
        CB.getExprConstraintVars(A, A->getType(), true);

      // Assign WILD to each of the constraint variables.
      FunctionDecl *FD = E->getDirectCallee();
      std::string Rsn = "Argument to function " +
                        (FD != nullptr ? FD->getName().str() : "pointer call");
      CB.constraintAllCVarsToWild(ParameterEC, Rsn, E);
    }
  }

  void arithBinop(BinaryOperator *O) {
      ConstAtom *ARR = Info.getConstraints().getArr();
      constraintInBodyVariable(O->getLHS(),ARR);
      constraintInBodyVariable(O->getRHS(),ARR);
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
    
    if (G->hasGlobalStorage())
      if (G->getType()->isPointerType() || G->getType()->isArrayType()) {
        Info.addVariable(G, Context);
        Info.seeGlobalDecl(G, Context);

        if (G->hasInit()) {
          CB.constrainLocalAssign(nullptr, G, G->getInit());
        }
      }

    return true;
  }

  bool VisitFunctionDecl(FunctionDecl *D) {
    FullSourceLoc FL = Context->getFullLoc(D->getBeginLoc());

    if (FL.isValid()) {

      Info.addVariable(D, Context);
      Info.seeFunctionDecl(D, Context);
      bool HasBody = false;

      if (D->hasBody() && D->isThisDeclarationADefinition()) {
        HasBody = true;
        Stmt *Body = D->getBody();
        FunctionVisitor FV = FunctionVisitor(Context, Info, D);

        // Visit the body of the function and build up information.
        FV.TraverseStmt(Body);
        // Add constraints based on heuristics.
        AddArrayHeuristics(Context, Info, D);
      }

      // Iterate through all parameter declarations and insert constraints
      // based on types.
      if (D->getType().getTypePtrOrNull() != nullptr) {
        const FunctionProtoType *FT =
            D->getType().getTypePtr()->getAs<FunctionProtoType>();
        if (FT != nullptr) {
          for (unsigned i = 0; i < FT->getNumParams(); i++) {
            if (i < D->getNumParams()) {
              ParmVarDecl *PVD = D->getParamDecl(i);
              Info.addVariable(PVD, Context);
              CB.specialCaseVarIntros(PVD, HasBody);
            }
          }
        }
      }
    }

    return true;
  }

  bool VisitRecordDecl(RecordDecl *Declaration) {
    if (RecordDecl *Definition = Declaration->getDefinition()) {
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
              CB.specialCaseVarIntros(D);
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
