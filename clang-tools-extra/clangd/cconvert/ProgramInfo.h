//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// This class represents all the information about a source file
// collected by the converter.
//===----------------------------------------------------------------------===//
#ifndef _PROGRAM_INFO_H
#define _PROGRAM_INFO_H
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

#include "ConstraintVariables.h"
#include "Utils.h"
#include "PersistentSourceLoc.h"
#include "ArrayBoundsInformation.h"
#include "CConvInteractive.h"

class ProgramInfo;

class ProgramInfo {
public:
  ProgramInfo();
  void print(llvm::raw_ostream &O) const;
  void dump() const { print(llvm::errs()); }
  void dump_json(llvm::raw_ostream &O) const;
  void dump_stats(std::set<std::string> &F) { print_stats(F, llvm::errs()); }
  void print_stats(std::set<std::string> &F, llvm::raw_ostream &O, bool onlySummary=false);

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

  // Checks the structural type equality of two constrained locations. This is 
  // needed if you are casting from U to V. If this returns true, then it's 
  // safe to add an implication that if U is wild, then V is wild. However,
  // if this returns false, then both U and V must be constrained to wild.
  bool checkStructuralEquality( std::set<ConstraintVariable*> V, 
                                std::set<ConstraintVariable*> U,
                                clang::QualType VTy,
                                clang::QualType UTy);
  bool checkStructuralEquality(clang::QualType, clang::QualType);

  // check if casting from srcType to dstType is fine.
  bool isExplicitCastSafe(clang::QualType dstType,
                          clang::QualType srcType);

  // Called when we are done adding constraints and visiting ASTs. 
  // Links information about global symbols together and adds 
  // constraints where appropriate.
  bool link();

  // These functions make the linker aware of function and global variables
  // declared in the program. 
  void seeFunctionDecl(clang::FunctionDecl *, clang::ASTContext *);
  void seeGlobalDecl(clang::VarDecl *, clang::ASTContext *);

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
    clang::ASTContext *C, bool ifc);

  // Given some expression E, what is the top-most constraint variable that
  // E refers to? 
  // inFunctionContext controls whether or not this operation is within
  // a function context. If set to true, we find Declarations associated with 
  // the function Definition (if present). If set to false, we skip the 
  // Declaration associated with the Definition and find the first 
  // non-Declaration Definition.
  std::set<ConstraintVariable*>
    getVariable(clang::Expr *E, clang::ASTContext *C, bool inFunctionContext = false);
  std::set<ConstraintVariable*>
    getVariableOnDemand(clang::Decl *D, clang::ASTContext *C, bool inFunctionContext = false);
  std::set<ConstraintVariable*>
    getVariable(clang::Decl *D, clang::ASTContext *C, bool inFunctionContext = false);
  // get constraint variable for the provided function or its parameter
  std::set<ConstraintVariable*>
    getVariable(clang::Decl *D, clang::ASTContext *C, FunctionDecl *FD, int parameterIndex=-1);

  VariableMap &getVarMap();

  // get on demand function declaration constraint. This is needed for functions
  // that do not have corresponding declaration.
  // for all functions that do not have corresponding declaration,
  // we create an on demand FunctionVariableConstraint.
  std::set<ConstraintVariable*>&
  getOnDemandFuncDeclarationConstraint(FunctionDecl *targetFunc, ASTContext *C);

  std::set<ConstraintVariable*>&
  getFuncDefnConstraints(FunctionDecl *targetFunc, ASTContext *C);

  // get a unique key for a given function declaration node.
  std::string getUniqueFuncKey(FunctionDecl *funcDecl, ASTContext *C);

  // get a unique string representing the declaration object.
  std::string getUniqueDeclKey(Decl *decl, ASTContext *C);

  // given the unique key for the function definition, get the pointer to
  // the constraint set of the declaration (if exists) else null.
  std::set<ConstraintVariable*> *getFuncDeclConstraintSet(std::string funcDefKey);

  std::map<std::string, std::set<ConstraintVariable*>>& getOnDemandFuncDeclConstraintMap();

  // handle assigning constraints based on function subtyping.
  bool handleFunctionSubtyping();

  ArrayBoundsInformation &getArrayBoundsInformation() {
    return *ArrBoundsInfo;
  }

  DisjointSet& getPointerConstraintDisjointSet() { return ConstraintDisjointSet; }

  bool computePointerDisjointSet();
private:
  // apply function sub-typing relation from srcCVar to dstCVar
  bool applySubtypingRelation(ConstraintVariable *srcCVar, ConstraintVariable *dstCVar);
  // check if the given set has the corresponding constraint variable type
  template <typename T>
  bool hasConstraintType(std::set<ConstraintVariable*> &S);
  // Function to check if an external symbol is okay to leave 
  // constrained. 
  bool isExternOkay(std::string ext);

  // Map that contains function name and corresponding
  // set of function variable constraints.
  // We only create on demand variables for non-declared functions.
  // we store the constraints based on function name
  // as the information needs to be stored across multiple
  // instances of the program AST
  std::map<std::string, std::set<ConstraintVariable*>> OnDemandFuncDeclConstraint;

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
  std::map<std::string, std::set<FVConstraint*>> GlobalFunctionSymbols;
  std::map<std::string, std::set<PVConstraint*>> GlobalVariableSymbols;
  // object that contains all the bounds information of various
  // array variables.
  ArrayBoundsInformation *ArrBoundsInfo;
  // Disjoint sets for constraints.
  DisjointSet ConstraintDisjointSet;
};

#endif
