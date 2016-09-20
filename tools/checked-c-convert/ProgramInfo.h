//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// This class is used to collect information for the program being analyzed.
// The class allocates constraint variables and maps the constraint variables
// to AST elements of the program.
//
// The allocation of constraint variables is a little nuanced. For a given
// variable, there might be multiple constraint variables. For example, some
// declaration of the form:
//
//  int **p = ... ;
//
// would be given two constraint variables, visualized like this:
//
//  int * q_(i+1) * q_i p = ... ; 
//
// The constraint variable at the "highest" or outer-most level of the type 
// is the lowest numbered constraint variable for a given declaration.
//===----------------------------------------------------------------------===//
#ifndef _PROGRAM_INFO_H
#define _PROGRAM_INFO_H
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

#include "Constraints.h"
#include "utils.h"
#include "PersistentSourceLoc.h"

typedef std::set<uint32_t> CVars;

// TODO: document what these three classes do.
class ConstraintVariable {
public:
  enum ConstraintVariableKind {
    PointerVariable,
    FunctionVariable
  };

  ConstraintVariableKind getKind() const { return Kind; }

private:
  ConstraintVariableKind Kind;
protected:
  std::string BaseType;
public:
  ConstraintVariable(ConstraintVariableKind K, std::string T) : 
    Kind(K),BaseType(T) {}

  virtual std::string mkString(Constraints::EnvironmentMap &E) = 0;
  virtual void print(llvm::raw_ostream &O) const = 0;
  virtual void dump() const = 0;
  virtual void constrainTo(Constraints &CS, ConstAtom *C) = 0;
  virtual bool anyChanges(Constraints::EnvironmentMap &E) = 0;

  std::string getTy() { return BaseType; }
};

class PointerVariableConstraint;
class FunctionVariableConstraint;

class PointerVariableConstraint : public ConstraintVariable {
private:
  CVars vars;
  FunctionVariableConstraint *FV;
public:
  PointerVariableConstraint(CVars V, std::string T) : 
    ConstraintVariable(PointerVariable, T),vars(V),FV(nullptr) {}
  PointerVariableConstraint(clang::DeclaratorDecl *D, uint32_t &K,
    Constraints &CS);
  PointerVariableConstraint(const clang::Type *Ty, uint32_t &K,
    std::string N, Constraints &CS);

  const CVars &getCvars() const { return vars; }

  static bool classof(const ConstraintVariable *S) {
    return S->getKind() == PointerVariable;
  }

  std::string mkString(Constraints::EnvironmentMap &E);

  FunctionVariableConstraint *getFV() { return FV; }

  void print(llvm::raw_ostream &O) const ;
  void dump() const { print(llvm::errs()); }
  void constrainTo(Constraints &CS, ConstAtom *C);
  bool anyChanges(Constraints::EnvironmentMap &E);
};

typedef PointerVariableConstraint PVConstraint;

class FunctionVariableConstraint : public ConstraintVariable {
private:
  std::set<ConstraintVariable*> returnVars;
  std::vector<std::set<ConstraintVariable*>> paramVars;
  std::string name;
  bool hasproto;
public:
  FunctionVariableConstraint(clang::DeclaratorDecl *D, uint32_t &K,
    Constraints &CS);
  FunctionVariableConstraint(const clang::Type *Ty, uint32_t &K,
    std::string N, Constraints &CS);

  std::set<ConstraintVariable*> &
  getReturnVars() { return returnVars; }

  size_t numParams() { return paramVars.size(); }

  bool hasProtoType() { return hasproto; }

  static bool classof(const ConstraintVariable *S) {
    return S->getKind() == FunctionVariable;
  }

  std::set<ConstraintVariable*> &
  getParamVar(unsigned i) {
    assert(i < paramVars.size());
    return paramVars.at(i);
  }

  std::string mkString(Constraints::EnvironmentMap &E);
  void print(llvm::raw_ostream &O) const;
  void dump() const { print(llvm::errs()); }
  void constrainTo(Constraints &CS, ConstAtom *C);
  bool anyChanges(Constraints::EnvironmentMap &E);
};

typedef FunctionVariableConstraint FVConstraint;

class ProgramInfo {
public:
  ProgramInfo() : freeKey(0), persisted(true) {}
  void print(llvm::raw_ostream &O) const;
  void dump() const { print(llvm::errs()); }
  void dump_stats(std::set<std::string> &F) { print_stats(F, llvm::errs()); }
  void print_stats(std::set<std::string> &F, llvm::raw_ostream &O);

  Constraints &getConstraints() { return CS;  }

  // Populate Variables, VarDeclToStatement, RVariables, and DepthMap with 
  // AST data structures that correspond do the data stored in PDMap and 
  // ReversePDMap. 
  void enterCompilationUnit(clang::ASTContext &Context);

  // Remove any references we maintain to AST data structure pointers. 
  // After this, the Variables, VarDeclToStatement, RVariables, and DepthMap
  // should all be empty. 
  void exitCompilationUnit();

  // For each pointer type in the declaration of D, add a variable to the 
  // constraint system for that pointer type. 
  bool addVariable(clang::DeclaratorDecl *D, clang::DeclStmt *St, clang::ASTContext *C);

  bool getDeclStmtForDecl(clang::Decl *D, clang::DeclStmt *&St);

  // Checks the structural type equality of two constraint variables. This is 
  // needed if you are casting from U to V. If this returns true, then it's 
  // safe to add an implication that if U is wild, then V is wild. However,
  // if this returns false, then both U and V must be constrained to wild.
  bool checkStructuralEquality( std::set<ConstraintVariable*> V, 
                                std::set<ConstraintVariable*> U);

  // Called when we are done adding constraints and visiting ASTs. 
  // Links information about global symbols together and adds 
  // constraints where appropriate.
  bool link();

  // These functions make the linker aware of function and global variables
  // declared in the program. 
  void seeFunctionDecl(clang::FunctionDecl *, clang::ASTContext *);
  void seeGlobalDecl(clang::VarDecl *);

  // This is a bit of a hack. What we need to do is traverse the AST in a 
  // bottom-up manner, and, for a given expression, decide which,
  // if any, constraint variable(s) are involved in that expression. However, 
  // in the current version of clang (3.8.1), bottom-up traversal is not 
  // supported. So instead, we do a manual top-down traversal, considering
  // the different cases and their meaning on the value of the constraint
  // variable involved. This is probably incomplete, but, we're going to 
  // go with it for now. 
  //
  // V is (currentVariable, baseVariable, limitVariable)
  // E is an expression to recursively traverse.
  //
  // Returns true if E resolves to a constraint variable q_i and the 
  // currentVariable field of V is that constraint variable. Returns false if 
  // a constraint variable cannot be found.
  std::set<ConstraintVariable *> 
  getVariableHelper(clang::Expr *E,std::set<ConstraintVariable *>V,
    clang::ASTContext *C);

  // Given some expression E, what is the top-most constraint variable that
  // E refers to? 
  std::set<ConstraintVariable*>
    getVariable(clang::Expr *E, clang::ASTContext *C);
  std::set<ConstraintVariable*>
    getVariable(clang::Decl *D, clang::ASTContext *C);

  VariableMap &getVarMap() { return Variables;  }

private:
  std::list<clang::RecordDecl*> Records;
  // Next available integer to assign to a variable.
  uint32_t freeKey;
  // Map from a Decl to the DeclStmt that contains the Decl.
  // I can't figure out how to go backwards from a VarDecl to a DeclStmt, so 
  // this infrastructure is here so that the re-writer can do that to figure
  // out how to break up variable declarations that should span lines in the
  // new program.
  VariableDecltoStmtMap VarDeclToStatement;

  // List of all constraint variables, indexed by their location in the source.
  // This information persists across invocations of the constraint analysis
  // from compilation unit to compilation unit.
  VariableMap Variables;

  // Constraint system.
  Constraints CS;
  // Is the ProgramInfo persisted? Only tested in asserts. Starts at true.
  bool persisted;
  // Global symbol information used for mapping
  // Map of global functions for whom we don't have a body, the keys are 
  // names of external functions, the value is whether the body has been
  // seen before.
  std::map<std::string, bool> ExternFunctions;
  std::map<std::string, std::set<FVConstraint*>> GlobalSymbols;
};

#endif
