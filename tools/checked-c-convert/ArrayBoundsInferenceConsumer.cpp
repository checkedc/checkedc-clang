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
  Expr *LHS = O->getLHS();
  Expr *RHS = O->getRHS();

  Expr *sizeExpression;
  // is the RHS expression a call to allocator function?
  if(isAllocatorCall(RHS, &sizeExpression)) {
    // if this an allocator function then
    // sizeExpression contains the argument
    // used for size argument

    // if LHS is just a variable?
    // i.e., ptr = ..
    // if yes, get the AST node of the target variable
    Decl *targetVar;
    if(isExpressionSimpleLocalVar(LHS, &targetVar)) {
      if(Info.isIdentifiedArrayVar(targetVar)) {

      }
    }
  }
}

bool LocalVarABVisitor::VisitDeclStmt(DeclStmt *S) {
  // Build rules based on initializers.
  for (const auto &D : S->decls()) {
    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      Expr *InitE = VD->getInit();
      Expr *sizeArg;
      if(isAllocatorCall(InitE, &sizeArg)) {
        if(Info.isIdentifiedArrayVar(D)) {
          Info.addAllocationBasedSizeExpr(D, sizeArg);
        }
      }
    }
  }

  return true;
}

// check if the provided expression is a call
// to known memory allocators.
// if yes, return true along with the argument used as size
// assigned to the second paramter i.e., sizeArgument
bool LocalVarABVisitor::isAllocatorCall(Expr *currExpr, Expr **sizeArgument) {
  if(currExpr != nullptr) {
    if (CStyleCastExpr *C = dyn_cast<CStyleCastExpr>(currExpr)) {
      currExpr = C->getSubExpr();
    }
    currExpr = removeImpCasts(currExpr);
    // check if this is a call expression.
    if (CallExpr *CA = dyn_cast<CallExpr>(currExpr)) {
      // Is this a call to a named function?
      FunctionDecl *calleeDecl = dyn_cast<FunctionDecl>(CA->getCalleeDecl());
      if (calleeDecl) {
        StringRef funcName = calleeDecl->getName();
        // check if the called function is a known allocator?
        if (LocalVarABVisitor::AllocatorFunctionNames.find(funcName) !=
            LocalVarABVisitor::AllocatorFunctionNames.end()) {
          if (sizeArgument != nullptr) {
            *sizeArgument = CA->getArg(0);
          }
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
  if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(toCheck)) {
    if (DeclaratorDecl *FD = dyn_cast<DeclaratorDecl>(DRE->getDecl())) {
      if (Decl *V = dyn_cast<Decl>(FD)) {
        *targetDecl = V;
        return true;
      }
    }
  }
  return false;
}

Expr *LocalVarABVisitor::removeImpCasts(Expr *toConvert) {
  if(ImplicitCastExpr *impCast =dyn_cast<ImplicitCastExpr>(toConvert)) {
    return impCast->getSubExpr();
  }
  return toConvert;
}

std::set<std::string> LocalVarABVisitor::AllocatorFunctionNames = {"malloc", "calloc"};

void HandleArrayVariablesBoundsDetection(ASTContext *C, ProgramInfo &I) {
  // Run array bounds
  LocalVarABVisitor LVAB(C, I);
  TranslationUnitDecl *TUD = C->getTranslationUnitDecl();
  for (const auto &D : TUD->decls())
    LVAB.TraverseDecl(D);
}