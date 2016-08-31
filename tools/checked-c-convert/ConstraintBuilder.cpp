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
            if (safe)
              CS.addConstraint(CS.createImplies(
                CS.createEq(CS.getOrCreateVar(V), CS.getWild()),
                CS.createEq(CS.getOrCreateVar(I), CS.getWild())));
            else
              CS.addConstraint(
                CS.createEq(CS.getOrCreateVar(V), CS.getWild()));
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
      }
    }

    return true;
  }

  // TODO: other visitors to visit statements and expressions that we use to
  // gather constraints.

  bool VisitBinAssign(BinaryOperator *O) {
    Expr *LHS = O->getLHS();
    Expr *RHS = O->getRHS();
    std::set<uint32_t> V;
    Info.getVariable(LHS, V, Context);
    for (const auto &I : V)
      constrainAssign(I, RHS);

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
        std::set<uint32_t> Ws;
        if (i < FD->getNumParams()) {
          Info.getVariable(FD->getParamDecl(i), Ws, Context);
          if (Ws.size() > 0) {
            assert(Ws.size() == 1);
            uint32_t W = *Ws.begin();

            std::set<uint32_t> V;
            Info.getVariable(A, V, Context);
            for (const auto &I : V)
              CS.addConstraint(
                CS.createEq(CS.getOrCreateVar(W), CS.getOrCreateVar(I)));
          }
        }
        i++;
      }
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

    return true;
  }

  bool VisitBinSub(BinaryOperator *O) {
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

    return true;
  }

private:
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
      for (const auto &P : D->params())
        if (P->getType()->isPointerType())
          Info.addVariable(P, NULL, Context);

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

          bool anyPointers = false;

          for (const auto &F : Definition->fields()) {
            if (F->getType()->isPointerType()) {
              anyPointers = true;
              break;
            }
          }

          if (anyPointers) {
            Info.addRecordDecl(Definition, Context);
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
  outs() << "analyzing\n";
  GlobalVisitor GV = GlobalVisitor(&C, Info);
  TranslationUnitDecl *TUD = C.getTranslationUnitDecl();
  // Generate constraints.
  for (const auto &D : TUD->decls()) {
    GV.TraverseDecl(D);
  }
  outs() << "done analyzing\n";
  Info.exitCompilationUnit();
  return;
}
