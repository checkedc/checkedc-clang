//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Implementation of all the methods of the class ArrayBoundsInferenceConsumer.
//===----------------------------------------------------------------------===//

#include "ArrayBoundsInferenceConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"

// This visitor handles the bounds of function local array variables.

bool LocalVarABVisitor::VisitBinAssign(BinaryOperator *O) {
  Expr *LHS = O->getLHS()->IgnoreImpCasts();
  Expr *RHS = O->getRHS()->IgnoreImpCasts();

  Expr *sizeExpression;
  // is the RHS expression a call to allocator function?
  if (isAllocatorCall(RHS, &sizeExpression)) {
    // if this is an allocator function then sizeExpression contains the
    // argument used for size argument

    // if LHS is just a variable? i.e., ptr = .., get the AST node of the
    // target variable
    Decl *targetVar;
    if (isExpressionSimpleLocalVar(LHS, &targetVar)) {
      if (Info.isIdentifiedArrayVar(targetVar))
        Info.addAllocationBasedSizeExpr(targetVar, sizeExpression);
      else
        dumpNotArrayIdentifiedVariable(targetVar, RHS, llvm::dbgs());
    }
  }

  return true;
}

bool LocalVarABVisitor::VisitDeclStmt(DeclStmt *S) {
  // Build rules based on initializers.
  for (const auto &D : S->decls())
    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      Expr *InitE = VD->getInit();
      Expr *sizeArg;
      if (isAllocatorCall(InitE, &sizeArg)) {
        if (Info.isIdentifiedArrayVar(D))
          Info.addAllocationBasedSizeExpr(D, sizeArg);
        else
          dumpNotArrayIdentifiedVariable(D, InitE, llvm::dbgs());
      }
    }

  return true;
}

// check if the provided expression is a call to known memory allocators.
// if yes, return true along with the argument used as the size assigned
// to the second parameter i.e., sizeArgument
bool LocalVarABVisitor::isAllocatorCall(Expr *currExpr, Expr **sizeArgument) {
  if (currExpr != nullptr) {
    currExpr = removeAuxiliaryCasts(currExpr);
    // check if this is a call expression to a named function.
    if (CallExpr *CA = dyn_cast<CallExpr>(currExpr))
      if (CA->getCalleeDecl() != nullptr) {
        FunctionDecl *calleeDecl = dyn_cast<FunctionDecl>(CA->getCalleeDecl());
        if (calleeDecl) {
          StringRef funcName = calleeDecl->getName();
          // check if the called function is a known allocator?
          if (LocalVarABVisitor::AllocatorFunctionNames.find(funcName) !=
              LocalVarABVisitor::AllocatorFunctionNames.end()) {
            if (sizeArgument != nullptr)
              *sizeArgument = CA->getArg(0);
            return true;
          }
        }
      }
  }
  return false;
}

// check if expression is a simple local variable
// i.e., ptr = .
// if yes, return the referenced local variable as the return
// value of the argument.
bool LocalVarABVisitor::isExpressionSimpleLocalVar(Expr *toCheck, Decl **targetDecl) {
  if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(toCheck))
    if (DeclaratorDecl *FD = dyn_cast<DeclaratorDecl>(DRE->getDecl()))
      if (Decl *V = dyn_cast<Decl>(FD)) {
        *targetDecl = V;
        return true;
      }
  return false;
}

void LocalVarABVisitor::dumpNotArrayIdentifiedVariable(Decl *LHS, Expr *RHS, raw_ostream &O) {
#ifdef DEBUG
  O << "Not identified as a array variable.\n RHS:";
  RHS->dump(O);
  O << "\n LHS:";
  LHS->dump(O);
  O << "\n";
#endif
}

Expr *LocalVarABVisitor::removeAuxiliaryCasts(Expr *srcExpr) {
  srcExpr = srcExpr->IgnoreParenImpCasts();
  if (CStyleCastExpr *C = dyn_cast<CStyleCastExpr>(srcExpr))
    srcExpr = C->getSubExpr();
  srcExpr = srcExpr->IgnoreParenImpCasts();
  return srcExpr;
}

std::set<std::string> LocalVarABVisitor::AllocatorFunctionNames = {"malloc", "calloc"};

void HandleArrayVariablesBoundsDetection(ASTContext *C, ProgramInfo &I) {
  // Run array bounds
  LocalVarABVisitor LVAB(C, I);
  TranslationUnitDecl *TUD = C->getTranslationUnitDecl();
  for (const auto &D : TUD->decls()) {
#ifdef DEBUG
    if (auto *FB = dyn_cast<FunctionDecl>(D))
      llvm::dbgs() << "Analyzing function:" << FB->getName() << "\n";
#endif
    LVAB.TraverseDecl(D);
  }
}
