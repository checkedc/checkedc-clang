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
class LocalVarABVisitor: public clang::RecursiveASTVisitor<LocalVarABVisitor> {
public:
  explicit LocalVarABVisitor(ASTContext *C, ProgramInfo &I)
  : Context(C), Info(I) {}

  // handles assignment expression.
  bool VisitBinAssign(BinaryOperator *O) {
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
          //TODO: handle this.
        }
      }
    }

    return true;
  }
private:
  // check if the provided expression is a call
  // to known memory allocators.
  // if yes, return true along with the argument used as size
  // assigned to the second paramter i.e., sizeArgument
  bool isAllocatorCall(Expr *currExpr, Expr **sizeArgument) {
    if (CStyleCastExpr *C = dyn_cast<CStyleCastExpr>(currExpr)) {
      currExpr = C->getSubExpr();
    }
    // check if this is a call expression.
    if(CallExpr *CA = dyn_cast<CallExpr>(currExpr)) {
      // Is this a call to a named function?
      FunctionDecl *calleeDecl = dyn_cast<FunctionDecl>(CA->getCalleeDecl());
      if(calleeDecl) {
        StringRef funcName = calleeDecl->getName();
        // check if the called function is a known allocator?
        if (LocalVarABVisitor::AllocatorFunctionNames.find(funcName) != LocalVarABVisitor::AllocatorFunctionNames.end()) {
          if(sizeArgument != nullptr) {
            *sizeArgument = CA->getArg(0);
          }
          return true;
        }
      }
    }
    return false;
  }

  // check if expression is a simple local variable
  // i.e., ptr = .
  // if yes, return the referenced local variable as the return
  // value of the argument.
  bool isExpressionSimpleLocalVar(Expr *toCheck, Decl **targetDecl) {
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

  ASTContext *Context;
  ProgramInfo &Info;
  static std::set<std::string> AllocatorFunctionNames;
};

std::set<std::string> LocalVarABVisitor::AllocatorFunctionNames = {"malloc", "calloc"};

void ArrayBoundsInferenceConsumer::HandleTranslationUnit(clang::ASTContext &Context) {
  // handle local variables.
  LocalVarABVisitor V(&Context, Info);
  TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
  for (const auto &D : TUD->decls())
    V.TraverseDecl(D);
}

