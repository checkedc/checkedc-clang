#ifndef _NEWTYP_H
#define _NEWTYP_H
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

#include "Constraints.h"

#include "utils.h"

// Intermediate data structures that describe a type to be re-written. 
class WildTyp;
class PtrTyp;
class ArrTyp;

// The base class for a new type to be inserted into the transformed program.
class NewTyp {
public:
  enum NewTypKind {
    N_BaseNonPointer,
    N_Ptr,
    N_Arr,
    N_Wild
  };

  NewTyp(NewTypKind K) : Kind(K), ReferentTyp(NULL), DeclRewrite(NULL) {}
  virtual ~NewTyp() {}
  NewTypKind getKind() const { return Kind; }

  // Given a set of solved constraints CS and a declaration D, produce a 
  // new checked C type 
  static NewTyp *mkTypForConstrainedType(Constraints &CS, clang::Decl *D,
    clang::DeclStmt *K, VariableMap &VM);

  // Returns the C-formatted type declaration of the new type, suitable for 
  // insertion into the source code.
  virtual std::string mkStr() = 0;

  clang::Decl *getDecl() { return DeclRewrite; }
  clang::DeclStmt *getWhere() { return StmtWhere; }

private:
  NewTypKind Kind;
protected:
  // Each type (except for BaseNonPointerTyp) wraps a sub-NewTyp value.
  NewTyp *ReferentTyp;
  // The outer-most NewTyp contains a reference to the Decl that this
  // NewTyp refers to.
  clang::Decl *DeclRewrite;
  clang::DeclStmt *StmtWhere;
};

// Represents a non-pointer type, a wrapper around a QualType from the 
// original program.
class BaseNonPointerTyp : public NewTyp {
public:
  BaseNonPointerTyp(clang::QualType _T) : T(_T), NewTyp(N_BaseNonPointer) {}

  std::string mkStr() {
    return T.getAsString();
  }

private:
  clang::QualType T;
};

// Represents a Checked C ptr<T> type.
class PtrTyp : public NewTyp {
public:
  PtrTyp() : NewTyp(N_Ptr) {}

  std::string mkStr() {
    return "ptr<" + ReferentTyp->mkStr() + "> ";
  }
};

// Represents a Checked C array_ptr type. Currently unused.
class ArrTyp : public NewTyp {
public:
  ArrTyp() : NewTyp(N_Arr) {}

  std::string mkStr() {
    return ReferentTyp->mkStr() + "* ";
  }
};

// Represents an unchecked pointer type.
class WildTyp : public NewTyp {
public:
  WildTyp() : NewTyp(N_Wild) {}

  std::string mkStr() {
    return ReferentTyp->mkStr() + "* ";
  }
};
#endif
