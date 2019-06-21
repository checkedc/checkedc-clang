//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Implementation of visitor methods for the FunctionVisitor class. These 
// visitors create constraints based on the AST of the program. 
//===----------------------------------------------------------------------===//
#include "ConstraintBuilder.h"

using namespace llvm;
using namespace clang;

// flags
// constraint all the arguments to a function
// accepting var args to be wild.
#define CONSTRAINT_ARGS_TO_VARGS_WILD

// Special-case handling for decl introductions. For the moment this covers:
//  * void-typed variables
//  * va_list-typed variables
// TODO: Github issue #61: improve handling of types for
// variable arguments.
static
void specialCaseVarIntros(ValueDecl *D, ProgramInfo &Info, ASTContext *C) {
  // Constrain everything that is void to wild.
  Constraints &CS = Info.getConstraints();

  // Special-case for va_list, constrain to wild.
  if (D->getType().getAsString() == "va_list" ||
      D->getType()->isVoidType()) {
    for (const auto &I : Info.getVariable(D, C))
      if (const PVConstraint *PVC = dyn_cast<PVConstraint>(I))
        for (const auto &J : PVC->getCvars())
          CS.addConstraint(
            CS.createEq(CS.getOrCreateVar(J), CS.getWild()));
  }
}

void constrainEq(std::set<ConstraintVariable*> &RHS,
  std::set<ConstraintVariable*> &LHS, ProgramInfo &Info);
// Given two ConstraintVariables, do the right thing to assign 
// constraints. 
// If they are both PVConstraint, then do an element-wise constraint
// generation.
// If they are both FVConstraint, then do a return-value and parameter
// by parameter constraint generation.
// If they are of an unequal parameter type, constrain everything in both
// to wild.
void constrainEq(ConstraintVariable *LHS,
  ConstraintVariable *RHS, ProgramInfo &Info) {
  ConstraintVariable *CRHS = RHS;
  ConstraintVariable *CLHS = LHS;
  Constraints &CS = Info.getConstraints();

  if (CRHS->getKind() == CLHS->getKind()) {
    if (FVConstraint *FCLHS = dyn_cast<FVConstraint>(CLHS)) {
      if (FVConstraint *FCRHS = dyn_cast<FVConstraint>(CRHS)) {
        // Element-wise constrain the return value of FCLHS and 
        // FCRHS to be equal. Then, again element-wise, constrain 
        // the parameters of FCLHS and FCRHS to be equal.
        constrainEq(FCLHS->getReturnVars(), FCRHS->getReturnVars(), Info);

        // Constrain the parameters to be equal.
        if (FCLHS->numParams() == FCRHS->numParams()) {
          for (unsigned i = 0; i < FCLHS->numParams(); i++) {
            std::set<ConstraintVariable*> &V1 =
              FCLHS->getParamVar(i);
            std::set<ConstraintVariable*> &V2 =
              FCRHS->getParamVar(i);
            constrainEq(V1, V2, Info);
          }
        } else {
          // Constrain both to be top.
          CRHS->constrainTo(CS, CS.getWild());
          CLHS->constrainTo(CS, CS.getWild());
        }
      } else {
        llvm_unreachable("impossible");
      }
    }
    else if (const PVConstraint *PCLHS = dyn_cast<PVConstraint>(CLHS)) {
      if (const PVConstraint *PCRHS = dyn_cast<PVConstraint>(CRHS)) {
        // Element-wise constrain PCLHS and PCRHS to be equal
        CVars CLHS = PCLHS->getCvars();
        CVars CRHS = PCRHS->getCvars();
        if (CLHS.size() == CRHS.size()) {
          CVars::iterator I = CLHS.begin();
          CVars::iterator J = CRHS.begin();
          while (I != CLHS.end()) {
            CS.addConstraint(
              CS.createEq(CS.getOrCreateVar(*I), CS.getOrCreateVar(*J)));
            ++I;
            ++J;
          }
        } else {
          // There is un-even-ness in the arity of CLHS and CRHS. The 
          // conservative thing to do would be to constrain both to 
          // wild. We'll do one step below the conservative step, which
          // is to constrain everything in PCLHS and PCRHS to be equal.
          for (const auto &I : PCLHS->getCvars())
            for (const auto &J : PCRHS->getCvars())
              CS.addConstraint(
                CS.createEq(CS.getOrCreateVar(I), CS.getOrCreateVar(J)));
        }
      } else
        llvm_unreachable("impossible");
    } else
      llvm_unreachable("unknown kind");
  }
  else {
    // Assigning from a function variable to a pointer variable?
    PVConstraint *PCLHS = dyn_cast<PVConstraint>(CLHS);
    FVConstraint *FCRHS = dyn_cast<FVConstraint>(CRHS);
    if (PCLHS && FCRHS) {
      if (FVConstraint *FCLHS = PCLHS->getFV()) {
        constrainEq(FCLHS, FCRHS, Info);
      } else {
        CLHS->constrainTo(CS, CS.getWild());
        CRHS->constrainTo(CS, CS.getWild());
      }
    } else {
      // Constrain everything in both to wild.
      CLHS->constrainTo(CS, CS.getWild());
      CRHS->constrainTo(CS, CS.getWild());
    }
  }
}

// Given an RHS and a LHS, constrain them to be equal. 
void constrainEq(std::set<ConstraintVariable*> &RHS,
  std::set<ConstraintVariable*> &LHS, ProgramInfo &Info) {
  for (const auto &I : RHS)
    for (const auto &J : LHS)
      constrainEq(I, J, Info);
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
  bool MyVisitVarDecl(VarDecl *D, DeclStmt *S) {
    if (D->isLocalVarDecl()) {
      FullSourceLoc FL = Context->getFullLoc(D->getBeginLoc());
      SourceRange SR = D->getSourceRange();

      if (SR.isValid() && FL.isValid() && !FL.isInSystemHeader() &&
        (D->getType()->isPointerType() || D->getType()->isArrayType())) {
        // add the variable with in the function body context.
        Info.addVariable(D, S, Context);

        specialCaseVarIntros(D, Info, Context);
        // if this is a static array declaration.
        // make this an array.
        if(D->getType()->isArrayType()) {
          Constraints &CS = Info.getConstraints();
          constraintInBodyVariable(D, CS.getArr());
        }
      }
    }

    return true;
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
  //
  // In any of these cases, due to conditional expressions, the number of
  // variables on the RHS could be 0 or more. We just do the same rule
  // for each pair of q_i to q_j \forall j in variables_on_rhs.
  //
  // V is the set of constraint variables on the left hand side that we are
  // assigning to. V represents constraints on a pointer variable. RHS is 
  // an expression which might produce constraint variables, or, it might 
  // be some expression like NULL, an integer constant or a cast.
  void constrainLocalAssign( std::set<ConstraintVariable*> V,
                        QualType lhsType,
                        Expr *RHS) {
    if (!RHS || V.size() == 0)
      return;

    std::set<ConstraintVariable *> RHSConstraints;
    RHSConstraints.clear();

    Constraints &CS = Info.getConstraints();
    RHS = getNormalizedExpr(RHS);
    // if this is a call expression?
    if (dyn_cast<CallExpr>(RHS)) {
      // if this is a call expression?
      // get the constraint variable corresponding
      // to the declaration.
      RHSConstraints = Info.getVariable(RHS, Context, false);
      if (RHSConstraints.size() > 0) {
        constrainEq(V, RHSConstraints, Info);
      }
    } else {
      RHS = RHS->IgnoreParens();

      // Cases 2-4.
      if (RHS->isIntegerConstantExpr(*Context)) {
        // Case 2.
        if (!RHS->isNullPointerConstant(*Context,
                                        Expr::NPC_ValueDependentIsNotNull)) {
          for (const auto &U : V) {
            if (PVConstraint *PVC = dyn_cast<PVConstraint>(U))
              for (const auto &J : PVC->getCvars()) {
                CS.addConstraint(
                  CS.createEq(CS.getOrCreateVar(J), CS.getWild()));
              }
          }
        }
      } // Cases 3-4.
      if (UnaryOperator *UO = dyn_cast<UnaryOperator>(RHS)) {
        if (UO->getOpcode() == UO_AddrOf) {
          // Case 3.
          // Is there anything to do here, or is it implicitly handled?
        }
      } else if (CStyleCastExpr *C = dyn_cast<CStyleCastExpr>(RHS)) {
        // Case 4.
        Expr *SE = C->getSubExpr();
        // Remove any binding of a Checked C temporary variable.
        if (CHKCBindTemporaryExpr *Temp = dyn_cast<CHKCBindTemporaryExpr>(SE))
          SE = Temp->getSubExpr();
        RHSConstraints = Info.getVariable(SE, Context);
        QualType rhsTy = RHS->getType();
        bool rulesFired = false;
        if (Info.checkStructuralEquality(V, RHSConstraints, lhsType, rhsTy)) {
          // This has become a little stickier to think about.
          // What do you do here if we determine that two things with
          // very different arity are structurally equal? Is that even
          // possible?

          // We apply a few rules here to determine if there are any
          // finer-grained constraints we can add. One of them is if the
          // value being cast from on the RHS is a call to malloc, and if
          // the type passed to malloc is equal to both lhsType and rhsTy.
          // If it is, we can do something less conservative.
          if (CallExpr *CA = dyn_cast<CallExpr>(SE)) {
            // get the declaration constraints of the callee.
            RHSConstraints = Info.getVariable(SE, Context);
            // Is this a call to malloc? Can we coerce the callee
            // to a NamedDecl?
            FunctionDecl *calleeDecl =
              dyn_cast<FunctionDecl>(CA->getCalleeDecl());
            if (calleeDecl && calleeDecl->getName() == "malloc") {
              // It's a call to malloc. What about the parameter to the call?
              if (CA->getNumArgs() > 0) {
                UnaryExprOrTypeTraitExpr *arg =
                  dyn_cast<UnaryExprOrTypeTraitExpr>(CA->getArg(0));
                if (arg && arg->isArgumentType()) {
                  // Check that the argument is a sizeof.
                  if (arg->getKind() == UETT_SizeOf) {
                    QualType argTy = arg->getArgumentType();
                    // argTy should be made a pointer, then compared for
                    // equality to lhsType and rhsTy.
                    QualType argPTy = Context->getPointerType(argTy);

                    if (Info.checkStructuralEquality(V, RHSConstraints, argPTy, lhsType) &&
                        Info.checkStructuralEquality(V, RHSConstraints, argPTy, rhsTy)) {
                      rulesFired = true;
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
        }

        // If none of the above rules for cast behavior fired, then
        // we need to fall back to doing something conservative.
        if (rulesFired == false) {
          // Constrain everything in both to top.
          // Remove the casts from RHS and try again to get a variable
          // from it. We want to constrain that side to wild as well.
          if(dyn_cast<CallExpr>(SE)) {
            RHSConstraints = Info.getVariable(SE, Context);
          } else {
            // get in-function-body constraint variables.
            RHSConstraints = Info.getVariable(SE, Context, true);
          }
          for (const auto &A : RHSConstraints) {
            if (PVConstraint *PVC = dyn_cast<PVConstraint>(A))
              for (const auto &B : PVC->getCvars())
                CS.addConstraint(
                  CS.createEq(CS.getOrCreateVar(B), CS.getWild()));
          }

          for (const auto &A : V) {
            if (PVConstraint *PVC = dyn_cast<PVConstraint>(A))
              for (const auto &B : PVC->getCvars())
                CS.addConstraint(
                  CS.createEq(CS.getOrCreateVar(B), CS.getWild()));
          }
        }
      } else {
        // this is a regular assignment.
        RHSConstraints = Info.getVariable(RHS, Context);
        if(RHSConstraints.size() > 0) {
          // Case 1.
          // There are constraint variables for the RHS, so, use those over
          // anything else we could infer.
          constrainEq(V, RHSConstraints, Info);
        }
      }
    }
  }

  void constrainLocalAssign(Expr *LHS, Expr *RHS) {
    // get the in-context local constraints.
    std::set<ConstraintVariable*> V = Info.getVariable(LHS, Context, true);
    constrainLocalAssign(V, LHS->getType(), RHS);
  }

  void constrainLocalAssign(DeclaratorDecl *D, Expr *RHS) {
    // get the in-context local constraints.
    std::set<ConstraintVariable*> V = Info.getVariable(D, Context, true);
    constrainLocalAssign(V, D->getType(), RHS);
  }

  bool VisitDeclStmt(DeclStmt *S) {
    // Introduce variables as needed.
    if (S->isSingleDecl()) {
      if (VarDecl *VD = dyn_cast<VarDecl>(S->getSingleDecl()))
        MyVisitVarDecl(VD, S);
    } else
      for (const auto &D : S->decls())
        if (VarDecl *VD = dyn_cast<VarDecl>(D))
          MyVisitVarDecl(VD, S);

    // Build rules based on initializers.
    for (const auto &D : S->decls()) {
      if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
        std::set<uint32_t> V;
        Expr *InitE = VD->getInit();
        constrainLocalAssign(VD, InitE);
      }
    }

    return true;
  }

  // TODO: other visitors to visit statements and expressions that we use to
  // gather constraints.

  bool VisitCStyleCastExpr(CStyleCastExpr *C) {
    // If we're casting from something with a constraint variable to something
    // that isn't a pointer type, we should constrain up. 
    auto W = Info.getVariable(C->getSubExpr(), Context, true); 

    if (W.size() > 0) {
      // Get the source and destination types. 
      QualType    Source = C->getSubExpr()->getType();
      QualType    Dest = C->getType();
      Constraints &CS = Info.getConstraints();

      // If these aren't compatible, constrain the source to wild. 
      if (!Info.checkStructuralEquality(Dest, Source))
        for (auto &C : W)
          C->constrainTo(CS, CS.getWild());
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
      // Call of a function directly.
      unsigned i = 0;
      for (const auto &A : E->arguments()) {
        // get constraint variables for the argument
        // from with in the context of the caller body
        std::set<ConstraintVariable*> ArgumentConstraints =
          Info.getVariable(A, Context, true);

        if (i < FD->getNumParams()) {
          bool handled = false;
          if(FD->getParamDecl(i)->hasInteropTypeExpr()) {
            // we only handle itype of NTArray for now.
            CheckedPointerKind ptrKind = getItypeCheckedPointerKind(FD->getParamDecl(i));
            if(ptrKind == CheckedPointerKind::NtArray) {
              // if the itype is an NTArray
              handled = true;
              Constraints &CS = Info.getConstraints();
              for (const auto &C : ArgumentConstraints) {
                // add constraint to NTArray
                C->constrainTo(CS, getCheckedPointerConstraint(ptrKind));
              }
            }
          }
          if(!handled) {
            // Here, we need to get the constraints of the
            // parameter from the callee's declaration.
            std::set<ConstraintVariable*> ParameterConstraints =
              Info.getVariable(FD->getParamDecl(i), Context, false);
            // add constraint that the arguments are equal to the
            // parameters.
            constrainEq(ParameterConstraints, ArgumentConstraints, Info);
          }
        } else {
          // this is the case of an argument passed to a function
          // with varargs.
          // Constrain this parameter to be wild.
#ifdef CONSTRAINT_ARGS_TO_VARGS_WILD

          Constraints &CS = Info.getConstraints();
          for (const auto &C : ArgumentConstraints)
            C->constrainTo(CS, CS.getWild());
#endif
        }

        i++;
      }
    } else if (DeclaratorDecl *DD = dyn_cast<DeclaratorDecl>(D)){
      // This could be a function pointer,
      // get the declaration of the function pointer variable
      // with in the caller context.
      std::set<ConstraintVariable*> V = Info.getVariable(DD, Context, true);
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
              std::set<ConstraintVariable*> ArgumentConstraints =
                Info.getVariable(A, Context, true);
              
              if (i < FV->numParams()) {
                std::set<ConstraintVariable*> ParameterDC = 
                  FV->getParamVar(i);
                constrainEq(ArgumentConstraints, ParameterDC, Info);
              } else {
                // Constrain argument to wild since we can't match it
                // to a parameter from the type.
                Constraints &CS = Info.getConstraints();
                for (const auto &V : ArgumentConstraints) {
                  V->constrainTo(CS, CS.getWild());
                }
              }
              i++;
            }
          } else {
            // This can happen when someone does something really wacky, like 
            // cast a char* to a function pointer, then call it. Constrain
            // everything.
            // what we do is, constraint all arguments to wild.
            constraintAllArgumentsToWild(E);
            Constraints &CS = Info.getConstraints();
            // also constraint
            C->constrainTo(CS, CS.getWild());
          }
        }
      } else {
        // Constrain all arguments to wild.
        constraintAllArgumentsToWild(E);
      }
    } else {
      // Constrain all arguments to wild.
      constraintAllArgumentsToWild(E);
    }
    
    return true;
  }

  // this will add the constraint that
  // variable is an array i.e., (V=ARR)
  bool VisitArraySubscriptExpr(ArraySubscriptExpr *E) {
    Constraints &CS = Info.getConstraints();
    constraintInBodyVariable(E->getBase(), CS.getArr());
    return true;
  }

  bool VisitReturnStmt(ReturnStmt *S) {
    // Here, we should constrain the return type
    // of the function body with the type of the
    // return expression.

    // get function variable constraint of the body
    std::set<ConstraintVariable*> Fun =
      Info.getVariable(Function, Context, true);
    // get the constraint of the return variable (again with in the context of the body)
    std::set<ConstraintVariable*> Var =
      Info.getVariable(S->getRetValue(), Context, true);

    // Constrain the value returned (if present) against the return value
    // of the function.   
    for (const auto &F : Fun ) {
      if (FVConstraint *FV = dyn_cast<FVConstraint>(F)) {
        constrainEq(FV->getReturnVars(), Var, Info);
      }
    }
    return true;
  }

  // these are the expressions, that will
  // add the constraint ~(V = Ptr) and ~(V = NTArr)
  // i.e., the variable is not a pointer or nt array

  bool VisitUnaryPreInc(UnaryOperator *O) {
    constrainInBodyExprNotPtrNotNt(O->getSubExpr());
    return true;
  }

  bool VisitUnaryPostInc(UnaryOperator *O) {
    constrainInBodyExprNotPtrNotNt(O->getSubExpr());
    return true;
  }

  bool VisitUnaryPreDec(UnaryOperator *O) {
    constrainInBodyExprNotPtrNotNt(O->getSubExpr());
    return true;
  }

  bool VisitUnaryPostDec(UnaryOperator *O) {
    constrainInBodyExprNotPtrNotNt(O->getSubExpr());
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

  // constraint all the provided vars to be
  // not equal to the provided type i.e., ~(V = type)
  void constrainVarsNotEq(std::set<ConstraintVariable*> &Vars, ConstAtom *type) {
    Constraints &CS = Info.getConstraints();
    for (const auto &I : Vars)
      if (PVConstraint *PVC = dyn_cast<PVConstraint>(I)) {
        if (PVC->getCvars().size() > 0)
          CS.addConstraint(
            CS.createNot(
              CS.createEq(
                CS.getOrCreateVar(*(PVC->getCvars().begin())), type)));
      }
  }

  // constraint all the provided vars to be
  // equal to the provided type i.e., (V = type)
  void constrainVarsEq(std::set<ConstraintVariable*> &Vars, ConstAtom *type) {
    Constraints &CS = Info.getConstraints();
    for (const auto &I : Vars)
      if (PVConstraint *PVC = dyn_cast<PVConstraint>(I)) {
        if (PVC->getCvars().size() > 0)
          CS.addConstraint(
            CS.createEq(
              CS.getOrCreateVar(*(PVC->getCvars().begin())), type));
      }
  }

  // Apply ~(V = Ptr) and ~(V = NTArr) to the
  // first 'level' constraint variable associated with
  // 'E' for in-body variables
  void constrainInBodyExprNotPtrNotNt(Expr *E) {
    // get the constrain variables
    // with in the body context
    std::set<ConstraintVariable*> Var =
      Info.getVariable(E, Context, true);
    Constraints &CS = Info.getConstraints();
    constrainVarsNotEq(Var, CS.getPtr());
    constrainVarsNotEq(Var, CS.getNTArr());
  }

  // constraint helpers.
  void constraintInBodyVariable(Expr *e, ConstAtom *target) {
    std::set<ConstraintVariable*> Var =
      Info.getVariable(e, Context, true);
    Constraints &CS = Info.getConstraints();
    constrainVarsEq(Var, target);
  }

  void constraintInBodyVariable(Decl *d, ConstAtom *target) {
    std::set<ConstraintVariable*> Var =
      Info.getVariable(d, Context, true);
    Constraints &CS = Info.getConstraints();
    constrainVarsEq(Var, target);
  }

  // constraint all the argument of the provided
  // call expression to be WILD
  void constraintAllArgumentsToWild(CallExpr *E) {
    for (const auto &A : E->arguments()) {
      // get constraint from within the function body
      // of the caller
      std::set<ConstraintVariable*> ParameterEC =
        Info.getVariable(A, Context, true);

      Constraints &CS = Info.getConstraints();
      for (const auto &C : ParameterEC)
        C->constrainTo(CS, CS.getWild());
    }
  }

  void arithBinop(BinaryOperator *O) {
    constrainInBodyExprNotPtrNotNt(O->getLHS());
    constrainInBodyExprNotPtrNotNt(O->getRHS());
  }

  ConstAtom* getCheckedPointerConstraint(CheckedPointerKind ptrKind) {
    Constraints &CS = Info.getConstraints();
    switch(ptrKind) {
      case CheckedPointerKind::NtArray:
        return CS.getNTArr();
      case CheckedPointerKind::Array:
        return CS.getArr();
      case CheckedPointerKind::Ptr:
        return CS.getPtr();
      case CheckedPointerKind::Unchecked:
        llvm_unreachable("Unchecked type inside an itype. This should be impossible.");
    }
    assert(false && "Invalid Pointer kind.");
  }

  Expr* getNormalizedExpr(Expr *CE) {
    if(dyn_cast<CHKCBindTemporaryExpr>(CE)) {
      CE = (dyn_cast<CHKCBindTemporaryExpr>(CE))->getSubExpr();
    }
    if(dyn_cast<ImplicitCastExpr>(CE)) {
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
      if (G->getType()->isPointerType() || G->getType()->isArrayType())
        Info.addVariable(G, nullptr, Context);

    Info.seeGlobalDecl(G);

    return true;
  }

  bool VisitFunctionDecl(FunctionDecl *D) {
    FullSourceLoc FL = Context->getFullLoc(D->getBeginLoc());

    if (FL.isValid()) {

      Info.addVariable(D, nullptr, Context);
      Info.seeFunctionDecl(D, Context);

      if (D->hasBody() && D->isThisDeclarationADefinition()) {
        Stmt *Body = D->getBody();
        FunctionVisitor FV = FunctionVisitor(Context, Info, D);

        // Visit the body of the function and build up information.
        FV.TraverseStmt(Body);
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
              Info.addVariable(D, NULL, Context);
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
    FileID mainFileID = SM.getMainFileID();
    const FileEntry *FE = SM.getFileEntryForID(mainFileID);
    if (FE != NULL)
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
