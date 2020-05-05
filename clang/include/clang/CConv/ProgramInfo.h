//=--ProgramInfo.h------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
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
#include "CConvInteractiveData.h"
#include "GatherTypes.h"
#include "GatherTool.h"


class ProgramInfo;

class ProgramInfo {
public:
  typedef std::pair<std::string, FVConstraint *> GlobFuncConstraintType;
  typedef std::map<std::string, std::map<std::string, std::set<FVConstraint *>>>
      StaticFunctionMapType;

  typedef std::map<std::string, std::set<FVConstraint *>>
      ExternalFunctionMapType;

  ProgramInfo();
  void print(llvm::raw_ostream &O) const;
  void dump() const { print(llvm::errs()); }
  void dump_json(llvm::raw_ostream &O) const;
  void dump_stats(std::set<std::string> &F) { print_stats(F, llvm::errs()); }
  void print_stats(std::set<std::string> &F, llvm::raw_ostream &O,
                   bool OnlySummary =false);

  void merge_MF(ParameterMap &MF);
  ParameterMap &get_MF();

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
  bool addVariable(clang::DeclaratorDecl *D, clang::DeclStmt *St,
                   clang::ASTContext *C);

  bool getDeclStmtForDecl(clang::Decl *D, clang::DeclStmt *&St);

  // Checks the structural type equality of two constrained locations. This is 
  // needed if you are casting from U to V. If this returns true, then it's 
  // safe to add an implication that if U is wild, then V is wild. However,
  // if this returns false, then both U and V must be constrained to wild.
  bool checkStructuralEquality( std::set<ConstraintVariable *> V,
                                std::set<ConstraintVariable *> U,
                                clang::QualType VTy,
                                clang::QualType UTy);
  bool checkStructuralEquality(clang::QualType, clang::QualType);

  // Check if casting from srcType to dstType is fine.
  bool isExplicitCastSafe(clang::QualType DstType,
                          clang::QualType SrcType);

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
    clang::ASTContext *C, bool Ifc);

  // Given some expression E, what is the top-most constraint variable that
  // E refers to? 
  // inFunctionContext controls whether or not this operation is within
  // a function context. If set to true, we find Declarations associated with 
  // the function Definition (if present). If set to false, we skip the 
  // Declaration associated with the Definition and find the first 
  // non-Declaration Definition.
  std::set<ConstraintVariable *>
    getVariable(clang::Expr *E, clang::ASTContext *C,
              bool InFuncCtx = false);
  std::set<ConstraintVariable *>
    getVariableOnDemand(clang::Decl *D, clang::ASTContext *C,
                      bool InFuncCtx = false);
  std::set<ConstraintVariable *>
    getVariable(clang::Decl *D, clang::ASTContext *C,
              bool InFuncCtx = false);
  // Get constraint variable for the provided function or its parameter.
  std::set<ConstraintVariable *>
    getVariable(clang::Decl *D, clang::ASTContext *C, FunctionDecl *FD,
              int PIdx =-1);

  VariableMap &getVarMap();

  // Get on demand function declaration constraint. This is needed for functions
  // that do not have corresponding declaration.
  // For all functions that do not have corresponding declaration,
  // We create an on demand FunctionVariableConstraint.
  std::set<FVConstraint *>&
  getOnDemandFuncDeclarationConstraint(FunctionDecl *D, ASTContext *C);

  std::set<FVConstraint *> *
  getFuncDefnConstraints(FunctionDecl *D, ASTContext *C);

  std::set<FVConstraint *> *
  getFuncDeclConstraints(FunctionDecl *D, ASTContext *C);

  // Get a unique key for a given function declaration node.
  std::string getUniqueFuncKey(FunctionDecl *D, ASTContext *C);

  // Get a unique string representing the declaration object.
  std::string getUniqueDeclKey(Decl *D, ASTContext *C);

  // Get the constraint set for external (i.e., non-static function)
  std::set<FVConstraint *> *
    getExtFuncDeclConstraintSet(std::string FuncName);

  std::set<FVConstraint *> *
    getExtFuncDefnConstraintSet(std::string FuncName);

  std::set<FVConstraint *> *
  getStaticFuncDeclConstraintSet(std::string FuncName, std::string FileName);

  std::set<FVConstraint *> *
  getStaticFuncDefnConstraintSet(std::string FuncName, std::string FileName);

  // Handle assigning constraints based on function subtyping.
  bool handleFunctionSubtyping();

  ArrayBoundsInformation &getArrayBoundsInformation() {
    return *ArrBoundsInfo;
  }

  DisjointSet &getPointerConstraintDisjointSet() {
    return ConstraintDisjointSet;
  }

  bool computePointerDisjointSet();

  // Check if the provided constraint variable is a valid pointer constraint.
  static bool isAValidPVConstraint(ConstraintVariable *C);

  // Perform multiple rewrites because declarations are spread across multiple
  // files and need to be rewritten multiple times.
  bool MultipleRewrites;

  // Check if the given function is an extern function.
  bool isAnExternFunction(const std::string &FName);

  ExternalFunctionMapType &getExternFuncDefFVMap() {
    return ExternalFunctionDefnFVCons;
  }

  StaticFunctionMapType &getStaticFuncDefFVMap() {
    return StaticFunctionDefnFVCons;
  }

private:

  // Apply function subtyping between given function definition and
  // declaration constraints.
  bool applyFunctionSubtyping(std::set<ConstraintVariable *> &DefCVars,
                              std::set<ConstraintVariable *> &DeclCVars);

  // Apply function sub-typing relation from srcCVar to dstCVar.
  bool applySubtypingRelation(ConstraintVariable *SrcCVar,
                              ConstraintVariable *DstCVar);
  // Check if the given set has the corresponding constraint variable type.
  template <typename T>
  bool hasConstraintType(std::set<ConstraintVariable *> &S);
  // Function to check if an external symbol is okay to leave 
  // constrained. 
  bool isExternOkay(std::string Ext);

  // Insert the given FVConstraint* set into the provided Map.
  // Returns true if successful else false.
  bool insertIntoExternalFunctionMap(ExternalFunctionMapType &Map,
                                     const std::string &FuncName,
                                     std::set<FVConstraint *> &ToIns);

  // Inserts the given FVConstraint* set into the provided static map.
  // Returns true if successful else false.
  bool insertIntoStaticFunctionMap(StaticFunctionMapType &Map,
                                   const std::string &FuncName,
                                   const std::string &FileName,
                                   std::set<FVConstraint *> &ToIns);

  // Next available integer to assign to a variable.
  uint32_t freeKey;

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

  // This map contains FVConstraint* objects for non-static
  // function declarations. For each non-static function, we will have an
  // entry in this map, which represents the FVConstraint for the declaration
  // of the corresponding function.
  // FunctionName -> [FVConstraint*].
  ExternalFunctionMapType ExternalFunctionDeclFVCons;

  // Similar to the external function map, this is for internal functions i.e.,
  // static function.
  // FunctionName -> {FileName -> [FVConstraint*]}
  StaticFunctionMapType StaticFunctionDeclFVCons;

  // This map is for non-static function definitions.
  ExternalFunctionMapType ExternalFunctionDefnFVCons;

  // This map is for static function definitions.
  // FunctionName -> {FileName -> [FVConstraint*]}
  StaticFunctionMapType StaticFunctionDefnFVCons;

  std::map<std::string, std::set<PVConstraint *>> GlobalVariableSymbols;
  ParameterMap MF;
  // Object that contains all the bounds information of various
  // array variables.
  ArrayBoundsInformation *ArrBoundsInfo;
  // Disjoint sets for constraints.
  DisjointSet ConstraintDisjointSet;
};

#endif
