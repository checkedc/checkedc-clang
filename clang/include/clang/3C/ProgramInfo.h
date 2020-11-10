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

#include "3CInteractiveData.h"
#include "AVarBoundsInfo.h"
#include "ConstraintVariables.h"
#include "PersistentSourceLoc.h"
#include "Utils.h"

class ProgramVariableAdder {
public:
  virtual void addVariable(clang::DeclaratorDecl *D,
                           clang::ASTContext *astContext) = 0;
  void addABoundsVariable(clang::Decl *D) {
    getABoundsInfo().insertVariable(D);
  }

protected:
  virtual AVarBoundsInfo &getABoundsInfo() = 0;
};

class ProgramInfo : public ProgramVariableAdder {
public:
  // This map holds similar information as the type variable map in
  // ConstraintBuilder.cpp, but it is stored in a form that is usable during
  // rewriting.
  typedef std::map<unsigned int, ConstraintVariable *> CallTypeParamBindingsT;
  typedef std::map<PersistentSourceLoc, CallTypeParamBindingsT>
      TypeParamBindingsT;

  typedef std::map<std::string, FVConstraint *> ExternalFunctionMapType;
  typedef std::map<std::string, ExternalFunctionMapType> StaticFunctionMapType;

  ProgramInfo();
  void print(llvm::raw_ostream &O) const;
  void dump() const { print(llvm::errs()); }
  void dump_json(llvm::raw_ostream &O) const;
  void dump_stats(const std::set<std::string> &F) {
    print_stats(F, llvm::errs());
  }
  void print_stats(const std::set<std::string> &F, llvm::raw_ostream &O,
                   bool OnlySummary = false, bool JsonFormat = false);

  // Populate Variables, VarDeclToStatement, RVariables, and DepthMap with
  // AST data structures that correspond do the data stored in PDMap and
  // ReversePDMap.
  void enterCompilationUnit(clang::ASTContext &Context);

  // Remove any references we maintain to AST data structure pointers.
  // After this, the Variables, VarDeclToStatement, RVariables, and DepthMap
  // should all be empty.
  void exitCompilationUnit();

  bool hasPersistentConstraints(clang::Expr *E, ASTContext *C) const;
  const CVarSet &getPersistentConstraints(clang::Expr *E, ASTContext *C) const;
  void storePersistentConstraints(clang::Expr *E, const CVarSet &Vars,
                                  ASTContext *C);

  // Get constraint variable for the provided Decl
  CVarOption getVariable(clang::Decl *D, clang::ASTContext *C);

  // Retrieve a function's constraints by decl, or by name; nullptr if not found
  FVConstraint *getFuncConstraint(FunctionDecl *D, ASTContext *C) const;
  FVConstraint *getExtFuncDefnConstraint(std::string FuncName) const;
  FVConstraint *getStaticFuncConstraint(std::string FuncName,
                                        std::string FileName) const;

  // Check if the given function is an extern function.
  bool isAnExternFunction(const std::string &FName);

  // Called when we are done adding constraints and visiting ASTs.
  // Links information about global symbols together and adds
  // constraints where appropriate.
  bool link();

  const VariableMap &getVarMap() const { return Variables; }
  Constraints &getConstraints() { return CS; }
  AVarBoundsInfo &getABoundsInfo() { return ArrBInfo; }

  ConstraintsInfo &getInterimConstraintState() { return CState; }
  bool computeInterimConstraintState(const std::set<std::string> &FilePaths);

  const ExternalFunctionMapType &getExternFuncDefFVMap() const {
    return ExternalFunctionFVCons;
  }

  const StaticFunctionMapType &getStaticFuncDefFVMap() const {
    return StaticFunctionFVCons;
  }

  void setTypeParamBinding(CallExpr *CE, unsigned int TypeVarIdx,
                           ConstraintVariable *CV, ASTContext *C);
  bool hasTypeParamBindings(CallExpr *CE, ASTContext *C) const;
  const CallTypeParamBindingsT &getTypeParamBindings(CallExpr *CE,
                                                     ASTContext *C) const;

  void constrainWildIfMacro(ConstraintVariable *CV, SourceLocation Location,
                            PersistentSourceLoc *PSL = nullptr);

private:
  // List of constraint variables for declarations, indexed by their location in
  // the source. This information persists across invocations of the constraint
  // analysis from compilation unit to compilation unit.
  VariableMap Variables;

  // Map with the same purpose as the Variables map, this stores constraint
  // variables for non-declaration expressions.
  std::map<PersistentSourceLoc, CVarSet> ExprConstraintVars;

  // Constraint system.
  Constraints CS;
  // Is the ProgramInfo persisted? Only tested in asserts. Starts at true.
  bool persisted;

  // Map of global decls for which we don't have a body, the keys are
  // names of external functions/vars, the value is whether the body/def
  // has been seen before.
  std::map<std::string, bool> ExternFunctions;
  std::map<std::string, bool> ExternGVars;

  // Maps for global/static functions, global variables
  ExternalFunctionMapType ExternalFunctionFVCons;
  StaticFunctionMapType StaticFunctionFVCons;
  std::map<std::string, std::set<PVConstraint *>> GlobalVariableSymbols;

  // Object that contains all the bounds information of various array variables.
  AVarBoundsInfo ArrBInfo;
  // Constraints state.
  ConstraintsInfo CState;

  // For each call to a generic function, remember how the type parameters were
  // instantiated so they can be inserted during rewriting.
  TypeParamBindingsT TypeParamBindings;

  // Function to check if an external symbol is okay to leave constrained.
  bool isExternOkay(const std::string &Ext);

  // Insert the given FVConstraint* set into the provided Map.
  // Returns true if successful else false.
  bool insertIntoExternalFunctionMap(ExternalFunctionMapType &Map,
                                     const std::string &FuncName,
                                     FVConstraint *ToIns);

  // Inserts the given FVConstraint* set into the provided static map.
  // Returns true if successful else false.
  bool insertIntoStaticFunctionMap(StaticFunctionMapType &Map,
                                   const std::string &FuncName,
                                   const std::string &FileName,
                                   FVConstraint *ToIns);

  // Special-case handling for decl introductions. For the moment this covers:
  //  * void-typed variables
  //  * va_list-typed variables
  void specialCaseVarIntros(ValueDecl *D, ASTContext *Context);

  // Inserts the given FVConstraint* set into the global map, depending
  // on whether static or not; returns true on success
  bool insertNewFVConstraint(FunctionDecl *FD, FVConstraint *FVCon,
                             ASTContext *C);

  // Retrieves a FVConstraint* from a Decl (which could be static, or global)
  FVConstraint *getFuncFVConstraint(FunctionDecl *FD, ASTContext *C);

  // For each pointer type in the declaration of D, add a variable to the
  // constraint system for that pointer type.
  void addVariable(clang::DeclaratorDecl *D, clang::ASTContext *AstContext);

  void insertIntoPtrSourceMap(const PersistentSourceLoc *PSL,
                              ConstraintVariable *CV);

  void computePtrLevelStats();

  void insertCVAtoms(ConstraintVariable *CV,
                     std::map<ConstraintKey, ConstraintVariable *> &AtomMap);
};

#endif
