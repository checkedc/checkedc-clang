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
#include "clang/CConv/ArrayBoundsInferenceConsumer.h"
#include "clang/CConv/CCGlobalOptions.h"

using namespace llvm;
using namespace clang;

// Special-case handling for decl introductions. For the moment this covers:
//  * void-typed variables
//  * va_list-typed variables
static
void specialCaseVarIntros(ValueDecl *D, ProgramInfo &Info, ASTContext *C,
                         bool FuncCtx = false) {
  // Constrain everything that is void to wild.
  Constraints &CS = Info.getConstraints();

  // Special-case for va_list, constrain to wild.
  if (isVarArgType(D->getType().getAsString()) ||
      hasVoidType(D)) {
    // set the reason for making this variable WILD.
    std::string Rsn = "Variable type void.";
    PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(D, *C);
    if (!D->getType()->isVoidType())
      Rsn = "Variable type is va_list.";
    for (const auto &I : Info.getVariable(D, C, FuncCtx)) {
      if (PVConstraint *PVC = dyn_cast<PVConstraint>(I)) {
        PVC->constrainToWild(CS, Rsn, &PL, false);
      }
    }
  }
}

// This class visits functions and adds constraints to the
// Constraints instance assigned to it.
// Each VisitXXX method is responsible either for looking inside statements
// to find constraints
// The results of this class are returned via the ProgramInfo
// parameter to the user.
class FunctionVisitor : public RecursiveASTVisitor<FunctionVisitor> {
public:
  explicit FunctionVisitor(ASTContext *C, ProgramInfo &I, FunctionDecl *FD)
      : Context(C), Info(I), Function(FD) {}


  // Introduce a variable into the environment.
  bool MyVisitVarDecl(VarDecl *D) {
    if (D->isLocalVarDecl()) {
      FullSourceLoc FL = Context->getFullLoc(D->getBeginLoc());
      SourceRange SR = D->getSourceRange();

      if (SR.isValid() && FL.isValid() && !FL.isInSystemHeader() &&
        (D->getType()->isPointerType() || D->getType()->isArrayType())) {
        // Add the variable with in the function body context.
        Info.addVariable(D, Context);

        specialCaseVarIntros(D, Info, Context);
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

  std::set<ConstraintVariable *>
  getRHSConsVariables(Expr *RHS, QualType LhsType, ASTContext *C) {
    if (LhsType->isFunctionPointerType()) {
      // We are assigning to a function pointer.
      std::set<ConstraintVariable *> RHSCSet = Info.getVariable(RHS, C, false);
      // Here, we should equate the constraints of inside and outside.
      for (auto *ConsVar : RHSCSet) {
        if (FVConstraint *FV = dyn_cast<FVConstraint>(ConsVar)) {
          FV->equateInsideOutsideVars(Info);
        }
      }
      return RHSCSet;
    }
    return Info.getVariable(RHS, C, true);
  }

  bool handleFuncCall(CallExpr *CA, QualType LhsType,
                      std::set<ConstraintVariable *> V) {
    bool RulesFired = false;
    // get the declaration constraints of the callee.
    std::set<ConstraintVariable *> RHSConstraints =
        Info.getVariable(CA, Context);
    // Is this a call to malloc? Can we coerce the callee
    // to a NamedDecl?
    FunctionDecl *CalleeDecl =
        dyn_cast<FunctionDecl>(CA->getCalleeDecl());
    // FIXME: Right now we don't look at what malloc is doing
    // but I don't think this works in the new regime.
    if (CalleeDecl && isFunctionAllocator(CalleeDecl->getName())) {
      // This is an allocator, should we treat it as safe?
      if (!ConsiderAllocUnsafe) {
        RulesFired = true;
      } else {
        // It's a call to allocator.
        // What about the parameter to the call?
        if (CA->getNumArgs() > 0) {
          UnaryExprOrTypeTraitExpr *arg =
              dyn_cast<UnaryExprOrTypeTraitExpr>(CA->getArg(0));
          if (arg && arg->isArgumentType()) {
            // Check that the argument is a sizeof.
            if (arg->getKind() == UETT_SizeOf) {
              QualType ArgTy = arg->getArgumentType();
              // argTy should be made a pointer, then compared for
              // equality to lhsType and rhsTy.
              QualType ArgPTy = Context->getPointerType(ArgTy);

              if (Info.checkStructuralEquality(V, RHSConstraints,
                                               ArgPTy, LhsType)) {
                RulesFired = true;
                // At present, I don't think we need to add an
                // implication based constraint since this rule
                // only fires if there is a cast from a call to malloc.
                // Since malloc is an external, there's no point in
                // adding constraints to it.
              }
            }
          }
        }
      }
    }
    return RulesFired;
  }

  // Adds constraints for the case where an expression RHS is being assigned
  // to a variable V. There are a few different cases:
  //  1. Straight-up assignment, i.e. int * a = b; with no casting. In this
  //     case, the rule would be that q_a = q_b.
  //  2. Assignment from a constant. If the constant is NULL, then V
  //     is left as constrained as it was before. If the constant is any
  //     other value, then we constrain V to be wild.
  //  3. Assignment from the address-taken of a variable. If no casts are
  //     involved, this is safe. We don't have a constraint variable for the
  //     address-taken variable, since it's referring to something "one-higher"
  //     however sometimes you could, like if you do:
  //     int **a = ...;
  //     int ** b = &(*(a));
  //     and the & * cancel each other out.
  //  4. Assignments from casts. Here, we use the implication rule.
  //  5. Assignments from call expressions i.e., a = foo(..)
  //
  // In any of these cases, due to conditional expressions, the number of
  // variables on the RHS could be 0 or more. We just do the same rule
  // for each pair of q_i to q_j \forall j in variables_on_rhs.
  //
  // V is the set of constraint variables on the left hand side that we are
  // assigning to. V represents constraints on a pointer variable. RHS is 
  // an expression which might produce constraint variables, or, it might 
  // be some expression like NULL, an integer constant or a cast.
  void constrainLocalAssign(std::set<ConstraintVariable *> V,
                            QualType LhsType,
                            Expr *RHS,
                            ConsAction CAction) {
    if (!RHS || V.size() == 0)
      return;

    Constraints &CS = Info.getConstraints();
    std::set<ConstraintVariable *> RHSConstraints;
    RHSConstraints.clear();
    RHS = getNormalizedExpr(RHS);
    PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(RHS, *Context);

    // If this is a call expression to a function.
    CallExpr *CE = dyn_cast<CallExpr>(RHS);
    if (CE != nullptr && CE->getDirectCallee() != nullptr) {
      // case 5
      // If this is a call expression?
      // Is this functions return type an itype
      FunctionDecl *Calle = CE->getDirectCallee();
      // Get the function declaration and look for
      // itype in the return.
      if (getDeclaration(Calle) != nullptr) {
        Calle = getDeclaration(Calle);
      }
      // If this is not a safe function call.
      if (!handleFuncCall(CE, LhsType, V)) {
        // Get the constraint variable corresponding
        // to the declaration.
        RHSConstraints = Info.getVariable(RHS, Context, false);
        // This is call-expression. We should use c2u for returns.
        if (RHSConstraints.size() > 0) {
          constrainConsVarGeq(V, RHSConstraints, CS, &PL, Safe_to_Wild, false);
        }
      }
    } else {
      RHS = RHS->IgnoreParens();
      PL = PersistentSourceLoc::mkPSL(RHS, *Context);

      // Cases 2
      if (isNULLExpression(RHS, *Context)) {
        // Do Nothing.
      } else if (RHS->isIntegerConstantExpr(*Context) &&
                !RHS->isNullPointerConstant(*Context,
                                             Expr::NPC_ValueDependentIsNotNull)) {
        // Case 2, Special handling. If this is an assignment of non-zero
        // integer constraint, then make the pointer WILD.
        std::string Rsn = "Casting to pointer from constant.";
        for (const auto &U : V) {
          if (PVConstraint *PVC = dyn_cast<PVConstraint>(U))
            PVC->constrainToWild(CS, Rsn, &PL, false);
        }
      } else if (CStyleCastExpr *C = dyn_cast<CStyleCastExpr>(RHS)) {
        // Case 4.
        Expr *SE = C->getSubExpr();
        // Remove any binding of a Checked C temporary variable.
        if (CHKCBindTemporaryExpr *Temp = dyn_cast<CHKCBindTemporaryExpr>(SE))
          SE = Temp->getSubExpr();
        RHSConstraints = Info.getVariable(SE, Context);
        QualType RhsTy = RHS->getType();
        bool ExternalCastSafe = false;
        bool RulesFired = false;
        if (Info.checkStructuralEquality(V, RHSConstraints, LhsType, RhsTy)) {
          ExternalCastSafe = true;
          // This has become a little stickier to think about.
          // What do you do here if we determine that two things with
          // very different arity are structurally equal? Is that even
          // possible?

          // We apply a few rules here to determine if there are any
          // finer-grained constraints we can add. One of them is if the
          // value being cast from on the RHS is a call to malloc, and if
          // the type passed to malloc is equal to both lhsType and rhsTy.
          // If it is, we can do something less conservative.
          if (CallExpr *CE = dyn_cast<CallExpr>(SE)) {
            RulesFired = handleFuncCall(CE, LhsType, V);
          }
        }

        // If none of the above rules for cast behavior fired, then
        // we need to fall back to doing something conservative.
        if (!RulesFired) {
          // Is the explicit cast safe?
          if (!ExternalCastSafe ||
              !Info.isExplicitCastSafe(LhsType, SE->getType())) {
            std::string CToDiffType = "Casted To Different Type.";
            std::string CFDifType = "Casted From Different Type.";
            // Constrain everything in both to Wild.
            // Remove the casts from RHS and try again to get a variable
            // from it. We want to constrain that side to wild as well.
            RHSConstraints = Info.getVariable(SE, Context, true);
            for (const auto &A : RHSConstraints) {
              if (PVConstraint *PVC = dyn_cast<PVConstraint>(A))
                PVC->constrainToWild(CS, CToDiffType, &PL, false);
            }

            for (const auto &A : V) {
              if (PVConstraint *PVC = dyn_cast<PVConstraint>(A))
                PVC->constrainToWild(CS, CFDifType, &PL, false);
            }
          } else {
            // Remove external cast and recursively process
            // the child expression.
            constrainLocalAssign(V, LhsType, SE, CAction);
          }
        }
      } else {
        // Get the constraint variables of the
        // expression from RHS side.
        RHSConstraints = getRHSConsVariables(RHS, LhsType, Context);
        if (RHSConstraints.size() > 0) {
          // There are constraint variables for the RHS, so, use those over
          // anything else we could infer.
          constrainConsVarGeq(V, RHSConstraints, CS, &PL, CAction, false);
        }
      }
    }
  }

  void constrainLocalAssign(Expr *LHS, Expr *RHS) {
    // Get the in-context local constraints.
    std::set<ConstraintVariable *> V = Info.getVariable(LHS, Context, true);
    constrainLocalAssign(V, LHS->getType(), RHS, Same_to_Same);
  }

  void constrainLocalAssign(DeclaratorDecl *D, Expr *RHS) {
    // Get the in-context local constraints.
    std::set<ConstraintVariable *> V = Info.getVariable(D, Context, true);
    constrainLocalAssign(V, D->getType(), RHS, Same_to_Same);
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
        constrainLocalAssign(VD, InitE);
      }
    }

    return true;
  }

  // TODO: other visitors to visit statements and expressions that we use to
  // Gather constraints.

  bool VisitCStyleCastExpr(CStyleCastExpr *C) {
    // If we're casting from something with a constraint variable to something
    // that isn't a pointer type, we should constrain up. 
    std::set<ConstraintVariable *> W =
        Info.getVariable(C->getSubExpr(), Context, true);

    if (W.size() > 0) {
      // Get the source and destination types. 
      QualType    Source = C->getSubExpr()->getType();
      QualType    Dest = C->getType();
      Constraints &CS = Info.getConstraints();

      std::string Rsn = "Casted To a Different Type.";
      PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(C, *Context);

      // If these aren't compatible, constrain the source to wild. 
      if (!Info.checkStructuralEquality(Dest, Source))
        for (auto &C : W)
          C->constrainToWild(CS, Rsn, &PL, false);
    }

    return true;
  }

  bool VisitCompoundAssignOperator(CompoundAssignOperator *O) {
    arithBinop(O);
    return true;
  }

  bool VisitBinAssign(BinaryOperator *O) {
    Expr *LHS = O->getLHS();
    Expr *RHS = O->getRHS();
    constrainLocalAssign(LHS, RHS);
    return true;
  }

  bool VisitCallExpr(CallExpr *E) {
    Decl *D = E->getCalleeDecl();
    if (!D)
      return true;

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
          Info.getVariable(A, Context, true);

        if (i < FD->getNumParams()) {
          auto *PD = FD->getParamDecl(i);
          // Here, we need to get the constraints of the
          // parameter from the callee's declaration.
          std::set<ConstraintVariable *> ParameterConstraintVars =
              Info.getVariable(PD, Context, false);
          // Add constraint that the arguments are equal to the
          // parameters.
          constrainLocalAssign(ParameterConstraintVars, PD->getType(),
                               A, Wild_to_Safe);
        } else {
          // This is the case of an argument passed to a function
          // with varargs.
          // Constrain this parameter to be wild.
          if (HandleVARARGS) {
            PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(E, *Context);
            std::string Rsn = "Passing argument to a function "
                              "accepting var args.";
              constrainVarsToWild(ArgumentConstraintVars, Rsn, &PL);
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
    std::set<ConstraintVariable *> Fun =
      Info.getVariableOnDemand(Function, Context, true);

    // Constrain the value returned (if present) against the return value
    // of the function.
    Expr *RetExpr = S->getRetValue();
    QualType Typ = Function->getReturnType();

    // Constrain the return type of the function
    // to the type of the return expression.
    for (const auto &F : Fun) {
      if (FVConstraint *FV = dyn_cast<FVConstraint>(F)) {
    	constrainLocalAssign(FV->getReturnVars(), Typ, RetExpr, Same_to_Same);
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
                  Info.getVariable(A, Context, true);

                if (i < FV->numParams()) {
                  std::set<ConstraintVariable *> ParameterDC =
                    FV->getParamVar(i);
                  constrainConsVarGeq(ParameterDC, ArgumentConstraints, CS, &PL,
                                      Wild_to_Safe, false);
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
      Info.getVariable(e, Context, true);
    constrainVarsTo(Var, CAtom);
  }

  void constraintInBodyVariable(Decl *d, ConstAtom *CAtom) {
    std::set<ConstraintVariable *> Var =
      Info.getVariable(d, Context, true);
    constrainVarsTo(Var, CAtom);
  }

  // Assign the provided type (target)
  // to all the constraint variables (CVars).
  void constrainVarsToWild(std::set<ConstraintVariable *> &CVars) {
    Constraints &CS = Info.getConstraints();
    for (const auto &C : CVars) {
      C->constrainToWild(CS, false);
    }
  }

  // Assign the provided type (target)
  // to all the constraint variables (CVars).
  void constrainVarsToWild(std::set<ConstraintVariable *> &CVars,
                           std::string &Rsn,
                           PersistentSourceLoc *PL = nullptr) {
    Constraints &CS = Info.getConstraints();
    for (const auto &C : CVars) {
      C->constrainToWild(CS, Rsn, PL, false);
    }
  }

  // Constraint all the argument of the provided
  // call expression to be WILD.
  void constraintAllArgumentsToWild(CallExpr *E) {
    PersistentSourceLoc psl = PersistentSourceLoc::mkPSL(E, *Context);
    for (const auto &A : E->arguments()) {
      // Get constraint from within the function body
      // of the caller.
      std::set<ConstraintVariable *> ParameterEC =
        Info.getVariable(A, Context, true);

      // Assign WILD to each of the constraint variables.
      FunctionDecl *FD = E->getDirectCallee();
      std::string Rsn = "Argument to function " +
                        (FD != nullptr ? FD->getName().str() : "pointer call");
        constrainVarsToWild(ParameterEC, Rsn, &psl);
    }
  }

  void arithBinop(BinaryOperator *O) {
      ConstAtom *ARR = Info.getConstraints().getArr();
      constraintInBodyVariable(O->getLHS(),ARR);
      constraintInBodyVariable(O->getRHS(),ARR);
  }

  Expr *getNormalizedExpr(Expr *CE) {
    if (dyn_cast<ImplicitCastExpr>(CE)) {
      CE = (dyn_cast<ImplicitCastExpr>(CE))->getSubExpr();
    }
    if (dyn_cast<CHKCBindTemporaryExpr>(CE)) {
      CE = (dyn_cast<CHKCBindTemporaryExpr>(CE))->getSubExpr();
    }
    if (dyn_cast<ImplicitCastExpr>(CE)) {
      CE = (dyn_cast<ImplicitCastExpr>(CE))->getSubExpr();
    }
    return CE;
  }

  ASTContext *Context;
  ProgramInfo &Info;
  FunctionDecl *Function;
};

// This class visits a global declaration and either
// - Builds an _enviornment_ and _constraints_ for each function
// - Builds _constraints_ for declared struct/records in the translation unit
// The results are returned in the ProgramInfo parameter to the user.
class GlobalVisitor : public RecursiveASTVisitor<GlobalVisitor> {
public:
  explicit GlobalVisitor(ASTContext *Context, ProgramInfo &I)
      : Context(Context), Info(I) {}

  bool VisitVarDecl(VarDecl *G) {
    
    if (G->hasGlobalStorage())
      if (G->getType()->isPointerType() || G->getType()->isArrayType()) {
        Info.addVariable(G, Context);
        Info.seeGlobalDecl(G, Context);
        // TODO
        //  Expr *InitE = G->getInit();
        // if (InitE)
        //   constrainLocalAssign(); // Need to pull out this method
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
              specialCaseVarIntros(PVD, Info, Context, HasBody);
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
              specialCaseVarIntros(D, Info, Context);
            }
        }
      }
    }

    return true;
  }

private:
  ASTContext *Context;
  ProgramInfo &Info;
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
