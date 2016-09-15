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

// Special-case handling for decl introductions. For the moment this covers:
//  * void-typed variables
//  * va_list-typed variables
//  * function pointer variables
static
void specialCaseVarIntros(ValueDecl *D, ProgramInfo &Info, ASTContext *C) {
  // Constrain everything that is void to wild.
  Constraints &CS = Info.getConstraints();
  if (D->getType().getAsString() == "void") {
    std::set<uint32_t> V;
    Info.getVariable(D, V, C);
    for (const auto &I : V)
      CS.addConstraint(
        CS.createEq(CS.getOrCreateVar(I), CS.getWild()));
  }

  // Special-case for va_list, constrain to wild.
  if (D->getType().getAsString() == "va_list") {
    std::set<uint32_t> V;
    Info.getVariable(D, V, C);
    for (const auto &I : V)
      CS.addConstraint(
        CS.createEq(CS.getOrCreateVar(I), CS.getWild()));
  }

  // Is it a function pointer? For the moment, special case those to wild.
  /*if (D->getType()->isFunctionPointerType()) {
    std::set<uint32_t> V;
    Info.getVariable(D, V, C);
    for (const auto &I : V)
      CS.addConstraint(
        CS.createEq(CS.getOrCreateVar(I), CS.getWild()));
  }*/
}

struct FPC {
  std::set<uint32_t> ReturnVal;
  std::vector<std::set<uint32_t>> Parameters;
};

static
void FPAssign(FPC &lhs, FPC &rhs, ProgramInfo &I, ASTContext *C) {
  // Constrain the return value.
  Constraints &CS = I.getConstraints();
  if (lhs.ReturnVal.size() == rhs.ReturnVal.size()) {
    std::set<uint32_t>::iterator I = lhs.ReturnVal.begin();
    std::set<uint32_t>::iterator J = rhs.ReturnVal.begin();

    while ((I != lhs.ReturnVal.end()) && (J != rhs.ReturnVal.end())) {
      CS.addConstraint(
        CS.createEq(CS.getOrCreateVar(*I), CS.getOrCreateVar(*J)));
      
      ++I;
      ++J;
    }
  }

  // Constrain the parameters.
  if (lhs.Parameters.size() == rhs.Parameters.size()) {
    std::vector<std::set<uint32_t>>::iterator I = lhs.Parameters.begin();
    std::vector<std::set<uint32_t>>::iterator J = rhs.Parameters.begin();

    while ((I != lhs.Parameters.end()) && (J != rhs.Parameters.end())) {
      std::set<uint32_t> lhsVS = *I;
      std::set<uint32_t> rhsVS = *J;

      if (lhsVS.size() == rhsVS.size()) {
        std::set<uint32_t>::iterator K = lhsVS.begin();
        std::set<uint32_t>::iterator L = rhsVS.begin();

        while ((K != lhsVS.end()) && (L != rhsVS.end())) {
          CS.addConstraint(
            CS.createEq(CS.getOrCreateVar(*K), CS.getOrCreateVar(*L)));

          ++K;
          ++L;
        }
      }

      ++I;
      ++J;
    }
  }
}

// Given an Expr LHS which has type function pointer, propagate 
// constraints from the RHS to the LHS for both return and parameter
// types. RHS might be either a function or function pointer type.
static
void FPAssign(Expr *LHS, Expr *RHS, ProgramInfo &I, ASTContext *C) {
  
  if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(LHS)) {
    ValueDecl *VD = DRE->getDecl();
    FPAssign(VD, RHS, I, C);
  }
}

static
void FPAssign(ValueDecl *LHS, Expr *RHS, ProgramInfo &I, ASTContext *C) {
  FPC fplhs;
  FPC fprhs;


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
      FullSourceLoc FL = Context->getFullLoc(D->getLocStart());
      SourceRange SR = D->getSourceRange();

      if (SR.isValid() && FL.isValid() && !FL.isInSystemHeader() &&
        D->getType()->isPointerType()) {
        Info.addVariable(D, S, Context);

        specialCaseVarIntros(D, Info, Context);
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
  void constrainAssign(uint32_t V, Expr *RHS) {
    if (!RHS)
      return;
    Constraints &CS = Info.getConstraints();
    std::set<uint32_t> W;
    Info.getVariable(RHS, W, Context);
    if (W.size() > 0) {
      // Case 1.
      for (const auto &I : W)
        CS.addConstraint(
            CS.createEq(CS.getOrCreateVar(V), CS.getOrCreateVar(I)));
    } else {
      // Cases 2-4.
      if (RHS->isIntegerConstantExpr(*Context)) {
        // Case 2.
        if (!RHS->isNullPointerConstant(*Context,
                                        Expr::NPC_ValueDependentIsNotNull))
          CS.addConstraint(CS.createEq(CS.getOrCreateVar(V), CS.getWild()));
      } else {
        // Cases 3-4.
        if (UnaryOperator *UO = dyn_cast<UnaryOperator>(RHS)) {
          if (UO->getOpcode() == UO_AddrOf) {
            // Case 3.
            // Is there anything to do here, or is it implicitly handled?
          }
        } else if (CStyleCastExpr *C = dyn_cast<CStyleCastExpr>(RHS)) {
          // Case 4.
          Info.getVariable(C->getSubExpr(), W, Context);
          bool safe = true;
          for (const auto &I : W)
            safe &= Info.checkStructuralEquality(V, I);

          for (const auto &I : W)
            if (safe) {
              CS.addConstraint(CS.createImplies(
                CS.createEq(CS.getOrCreateVar(V), CS.getWild()),
                CS.createEq(CS.getOrCreateVar(I), CS.getWild())));
            } else {
              CS.addConstraint(
                CS.createEq(CS.getOrCreateVar(V), CS.getWild()));
              CS.addConstraint(
                CS.createEq(CS.getOrCreateVar(I), CS.getWild()));
            }
        }
      }
    }
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
        Info.getVariable(VD, V, Context);
        if (V.size() > 0)
          for (const auto &I : V)
            constrainAssign(I, InitE);

        if (VD->getType()->isFunctionPointerType())
          FPAssign(VD, InitE, Info, Context);
      }
    }

    return true;
  }

  // TODO: other visitors to visit statements and expressions that we use to
  // gather constraints.

  bool VisitCompoundAssignOperator(CompoundAssignOperator *O) {
    arithBinop(O);
    return true;
  }

  bool VisitBinAssign(BinaryOperator *O) {
    Expr *LHS = O->getLHS();
    Expr *RHS = O->getRHS();
    std::set<uint32_t> V;
    Info.getVariable(LHS, V, Context);
    for (const auto &I : V)
      constrainAssign(I, RHS);

    if (LHS->getType()->isFunctionPointerType())
      FPAssign(LHS, RHS, Info, Context);

    return true;
  }

  bool VisitCallExpr(CallExpr *E) {
    Decl *D = E->getCalleeDecl();
    if (!D)
      return true;

    if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {

      Constraints &CS = Info.getConstraints();
      unsigned i = 0;
      for (const auto &A : E->arguments()) {
        std::set<uint32_t> V;
        Info.getVariable(A, V, Context);

        if (i < FD->getNumParams()) {
          ParmVarDecl *PVD = FD->getParamDecl(i);
          std::set<uint32_t> Ws;
          Info.getVariable(PVD, Ws, Context);
          
          if (Ws.size() == V.size()) {
            // If there are an equal number of constraint variables for 
            // both the parameter declaration and the expression argument,
            // then constrain them to be position-wise equal.
            std::set<uint32_t>::iterator I = Ws.begin();
            std::set<uint32_t>::iterator J = V.begin();
            while ((I != Ws.end()) && (J != V.end())) {
              
              CS.addConstraint(
                CS.createEq(CS.getOrCreateVar(*I), CS.getOrCreateVar(*J)));

              ++I;
              ++J;
            }

          }else {
            if (Verbose) {
              errs() << "arity of parameter and expr do not match!\n";
              PVD->dump();
              errs() << "\n";
              errs() << "constraining everything\n";
            }

            for (const auto &A : Ws)
              CS.addConstraint(
                CS.createEq(CS.getOrCreateVar(A), CS.getWild()));

            for (const auto &A : V)
              CS.addConstraint(
                CS.createEq(CS.getOrCreateVar(A), CS.getWild()));
          }
        } else {
          // We should constrain A to WILD since there isn't any parameter
          // information to constrain to, i.e. this is a VARARGS function.
          if (Verbose) {
            errs() << "Parameter passed to a function with no constraint ";
            errs() << "information for a parameter position\n";
          }

          for (const auto &A : V)
            CS.addConstraint(
              CS.createEq(CS.getOrCreateVar(A), CS.getWild()));
        }
        i++;
      }
    } else {
      
    }

    return true;
  }

  bool VisitArraySubscriptExpr(ArraySubscriptExpr *E) {
    std::set<uint32_t> V;
    Constraints &CS = Info.getConstraints();
    Info.getVariable(E->getBase(), V, Context);
    for (const auto &I : V)
      CS.addConstraint(CS.createEq(CS.getOrCreateVar(I), CS.getArr()));
    return true;
  }

  bool VisitReturnStmt(ReturnStmt *S) {
    std::set<uint32_t> V;
    std::set<uint32_t> F;
    Constraints &CS = Info.getConstraints();
    Info.getVariable(S->getRetValue(), V, Context);
    Info.getVariable(Function, F, Context);
    if (F.size() > 0) {
      assert(F.size() == 1);
      for (const auto &I : V)
        CS.addConstraint(
            CS.createEq(CS.getOrCreateVar(*F.begin()), CS.getOrCreateVar(I)));
    }

    return true;
  }

  bool VisitUnaryPreInc(UnaryOperator *O) {
    std::set<uint32_t> V;
    Constraints &CS = Info.getConstraints();
    Info.getVariable(O->getSubExpr(), V, Context);
    for (const auto &I : V)
      CS.addConstraint(
          CS.createNot(CS.createEq(CS.getOrCreateVar(I), CS.getPtr())));

    return true;
  }

  bool VisitUnaryPostInc(UnaryOperator *O) {
    std::set<uint32_t> V;
    Constraints &CS = Info.getConstraints();
    Info.getVariable(O->getSubExpr(), V, Context);
    for (const auto &I : V)
      CS.addConstraint(
          CS.createNot(CS.createEq(CS.getOrCreateVar(I), CS.getPtr())));

    return true;
  }

  bool VisitUnaryPreDec(UnaryOperator *O) {
    std::set<uint32_t> V;
    Constraints &CS = Info.getConstraints();
    Info.getVariable(O->getSubExpr(), V, Context);
    for (const auto &I : V)
      CS.addConstraint(
          CS.createNot(CS.createEq(CS.getOrCreateVar(I), CS.getPtr())));

    return true;
  }

  bool VisitUnaryPostDec(UnaryOperator *O) {
    std::set<uint32_t> V;
    Constraints &CS = Info.getConstraints();
    Info.getVariable(O->getSubExpr(), V, Context);
    for (const auto &I : V)
      CS.addConstraint(
          CS.createNot(CS.createEq(CS.getOrCreateVar(I), CS.getPtr())));

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

  void arithBinop(BinaryOperator *O) {
    std::set<uint32_t> V1;
    std::set<uint32_t> V2;
    Constraints &CS = Info.getConstraints();
    Info.getVariable(O->getLHS(), V1, Context);
    Info.getVariable(O->getRHS(), V2, Context);

    for (const auto &I : V1)
      CS.addConstraint(
        CS.createNot(CS.createEq(CS.getOrCreateVar(I), CS.getPtr())));

    for (const auto &I : V2)
      CS.addConstraint(
        CS.createNot(CS.createEq(CS.getOrCreateVar(I), CS.getPtr())));
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
    
    if (G->hasGlobalStorage() && G->getType()->isPointerType())
      Info.addVariable(G, NULL, Context);

    Info.seeGlobalDecl(G);

    return true;
  }

  bool VisitFunctionDecl(FunctionDecl *D) {
    FullSourceLoc FL = Context->getFullLoc(D->getLocStart());

    if (FL.isValid()) {
      std::string fn = D->getNameAsString();
 
      // Add variables for the value returned from the function.
      if (D->getReturnType()->isPointerType())
        Info.addVariable(D, NULL, Context);

      // Add variables for each parameter declared for the function.
      for (const auto &P : D->parameters())
        if (P->getType()->isPointerType()) {
          Info.addVariable(P, NULL, Context);
          specialCaseVarIntros(P, Info, Context);
        }

      if (D->hasBody() && D->isThisDeclarationADefinition()) {
        Stmt *Body = D->getBody();
        FunctionVisitor FV = FunctionVisitor(Context, Info, D);

        // Visit the body of the function and build up information.
        FV.TraverseStmt(Body);
      }

      Info.seeFunctionDecl(D, Context);
    }

    return true;
  }

  bool VisitRecordDecl(RecordDecl *Declaration) {
    if (RecordDecl *Definition = Declaration->getDefinition()) {
      FullSourceLoc FL = Context->getFullLoc(Definition->getLocStart());

      if (FL.isValid() && !FL.isInSystemHeader()) {
        SourceManager &SM = Context->getSourceManager();
        FileID FID = FL.getFileID();
        const FileEntry *FE = SM.getFileEntryForID(FID);

        if (FE && FE->isValid()) {
          // We only want to re-write a record if it contains
          // any pointer types. Most record types probably do,
          // but let's scan it and not consider any records
          // that don't have any pointers.

          for (const auto &D : Definition->fields())
            if (D->getType()->isPointerType()) {
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
