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

#ifndef LLVM_CLANG_3C_PROGRAMINFO_H
#define LLVM_CLANG_3C_PROGRAMINFO_H

#include "clang/3C/3CInteractiveData.h"
#include "clang/3C/3CStats.h"
#include "clang/3C/AVarBoundsInfo.h"
#include "clang/3C/ConstraintVariables.h"
#include "clang/3C/MultiDecls.h"
#include "clang/3C/PersistentSourceLoc.h"
#include "clang/3C/Utils.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

class ProgramVariableAdder {
public:
  virtual void addVariable(clang::DeclaratorDecl *D,
                           clang::ASTContext *AstContext) = 0;
  void addABoundsVariable(clang::Decl *D) {
    getABoundsInfo().insertVariable(D);
  }

  virtual bool seenTypedef(PersistentSourceLoc PSL) = 0;

  virtual void addTypedef(PersistentSourceLoc PSL, TypedefDecl *TD,
                          ASTContext &C) = 0;

protected:
  virtual AVarBoundsInfo &getABoundsInfo() = 0;
};

typedef std::pair<CVarSet, BKeySet> CSetBkeyPair;

// The pair of CVs are the type param constraint and an optional
// constraint used to get the generic index. A better solution would have
// generic constraints saved within ConstraintVariables, but those don't
// exist at this time.
struct TypeParamConstraint {
  ConstraintVariable *MainConstraint;
  ConstraintVariable *GenericAddition;
  TypeParamConstraint() :
      MainConstraint(nullptr), GenericAddition(nullptr) {}
  TypeParamConstraint(ConstraintVariable *M, ConstraintVariable *G) :
      MainConstraint(M), GenericAddition(G) {}
  // Fast. Whether `getConstraint` will return something other than nullptr.
  bool isConsistent() const { return MainConstraint != nullptr; }
  // Provides generic information if available and safe. This is somewhat of
  // a hack for nested generics and returns (the constraint for) a local
  // parameter. Otherwise, returns the generated constraint, which can also be
  // accessed as `MainConstraint`.
  ConstraintVariable *getConstraint(const EnvironmentMap &E) {
    if (MainConstraint != nullptr && GenericAddition != nullptr &&
        GenericAddition->isSolutionChecked(E)) {
      return GenericAddition;
    } else {
      return MainConstraint;
    }
  }
};

class ProgramInfo : public ProgramVariableAdder {
public:

  // This map holds similar information as the type variable map in
  // ConstraintBuilder.cpp, but it is stored in a form that is usable during
  // rewriting.
  typedef std::map<unsigned int, TypeParamConstraint> CallTypeParamBindingsT;

  typedef std::map<std::string, FVConstraint *> ExternalFunctionMapType;
  typedef std::map<std::string, ExternalFunctionMapType> StaticFunctionMapType;

  ProgramInfo();
  virtual ~ProgramInfo();
  void clear();
  void print(llvm::raw_ostream &O) const;
  void dump() const { print(llvm::errs()); }
  void dumpJson(llvm::raw_ostream &O) const;
  void dumpStats(const std::set<std::string> &F) {
    printStats(F, llvm::errs());
  }
  void printStats(const std::set<std::string> &F, llvm::raw_ostream &O,
                  bool OnlySummary = false, bool JsonFormat = false);

  void printAggregateStats(const std::set<std::string> &F,
                           llvm::raw_ostream &O);

  // Populate Variables, VarDeclToStatement, RVariables, and DepthMap with
  // AST data structures that correspond do the data stored in PDMap and
  // ReversePDMap.
  void enterCompilationUnit(clang::ASTContext &Context);

  // Remove any references we maintain to AST data structure pointers.
  // After this, the Variables, VarDeclToStatement, RVariables, and DepthMap
  // should all be empty.
  void exitCompilationUnit();

  bool hasPersistentConstraints(clang::Expr *E, ASTContext *C) const;
  const CSetBkeyPair &getPersistentConstraints(clang::Expr *E,
                                               ASTContext *C) const;
  void storePersistentConstraints(clang::Expr *E, const CSetBkeyPair &Vars,
                                  ASTContext *C);
  // Get only constraint vars from the persistent contents of the
  // expression E.
  const CVarSet &getPersistentConstraintsSet(clang::Expr *E,
                                             ASTContext *C) const;
  // Store CVarSet with an empty set of BoundsKey into persistent contents.
  void storePersistentConstraints(clang::Expr *E, const CVarSet &Vars,
                                  ASTContext *C);
  void removePersistentConstraints(Expr *E, ASTContext *C);

  // Get constraint variable for the provided Decl
  CVarOption getVariable(clang::Decl *D, clang::ASTContext *C);

  // Retrieve a function's constraints by decl, or by name; nullptr if not found
  FVConstraint *getFuncConstraint(FunctionDecl *D, ASTContext *C) const;
  FVConstraint *
  getFuncConstraint(const std::string &FuncName, const std::string &FileName,
                    bool IsStatic) const;
  FVConstraint *getExtFuncDefnConstraint(std::string FuncName) const;
  FVConstraint *getStaticFuncConstraint(std::string FuncName,
                                        std::string FileName) const;

  // Called when we are done adding constraints and visiting ASTs.
  // Links information about global symbols together and adds
  // constraints where appropriate.
  bool link();

  const VariableMap &getVarMap() const { return Variables; }
  Constraints &getConstraints() { return CS; }
  const Constraints &getConstraints() const { return CS; }
  AVarBoundsInfo &getABoundsInfo() override { return ArrBInfo; }

  PerformanceStats &getPerfStats() { return PerfS; }

  ConstraintsInfo &getInterimConstraintState() { return CState; }
  bool computeInterimConstraintState(const std::set<std::string> &FilePaths);

  const ExternalFunctionMapType &getExternFuncDefFVMap() const {
    return ExternalFunctionFVCons;
  }

  const StaticFunctionMapType &getStaticFuncDefFVMap() const {
    return StaticFunctionFVCons;
  }

  void setTypeParamBinding(CallExpr *CE, unsigned int TypeVarIdx,
                           ConstraintVariable *CV,
                           ConstraintVariable* Ident, ASTContext *C);
  bool hasTypeParamBindings(CallExpr *CE, ASTContext *C) const;
  const CallTypeParamBindingsT &getTypeParamBindings(CallExpr *CE,
                                                     ASTContext *C) const;

  void constrainWildIfMacro(ConstraintVariable *CV, SourceLocation Location,
                            const ReasonLoc &Rsn);

  void ensureNtCorrect(const QualType &QT, const PersistentSourceLoc &PSL,
                       PointerVariableConstraint *PV);

  void unifyIfTypedef(const QualType &QT, clang::ASTContext &,
                      PVConstraint *, ConsAction CA = Same_to_Same);

  CVarOption lookupTypedef(PersistentSourceLoc PSL);

  bool seenTypedef(PersistentSourceLoc PSL) override;

  void addTypedef(PersistentSourceLoc PSL, TypedefDecl *TD,
                  ASTContext &C) override;

  // Store mapping from ASTContexts to a unique index in the ASTs vector in
  // the ProgramInfo object. This function must be called prior to any AST
  // traversals so that the map is populated.
  void registerTranslationUnits(
      const std::vector<std::unique_ptr<clang::ASTUnit>> &ASTs);

  ProgramMultiDeclsInfo TheMultiDeclsInfo;

private:
  // List of constraint variables for declarations, indexed by their location in
  // the source. This information persists across invocations of the constraint
  // analysis from compilation unit to compilation unit.
  VariableMap Variables;

  // Map storing constraint information for typedefed types
  // The set contains all the constraint variables that also use this tyepdef
  // rewritten.
  std::map<PersistentSourceLoc, CVarOption> TypedefVars;

  // A pair containing an AST node ID and an index that uniquely identifies the
  // translation unit. Translation unit identifiers are drawn from the
  // TranslationUnitIdxMap. Used as a key to index expression in the following
  // maps.
  typedef std::pair<int64_t, unsigned int> IDAndTranslationUnit;
  IDAndTranslationUnit getExprKey(clang::Expr *E, clang::ASTContext *C) const;

  // Map with the similar purpose as the Variables map. This stores a set of
  // constraint variables and bounds key for non-declaration expressions.
  std::map<IDAndTranslationUnit, CSetBkeyPair> ExprConstraintVars;

  // For each expr stored in the ExprConstraintVars, also store the source
  // location for the expression. This is used to emit diagnostics. It is
  // expected that multiple entries will map to the same source location.
  std::map<IDAndTranslationUnit, PersistentSourceLoc> ExprLocations;

  // This map holds similar information as the type variable map in
  // ConstraintBuilder.cpp, but it is stored in a form that is usable during
  // rewriting.
  typedef std::map<IDAndTranslationUnit, CallTypeParamBindingsT>
      TypeParamBindingsT;

  std::map<ConstraintKey, PersistentSourceLoc> DeletedAtomLocations;

  //Performance stats
  PerformanceStats PerfS;

  // Constraint system.
  Constraints CS;
  // Is the ProgramInfo persisted? Only tested in asserts. Starts at true.
  bool Persisted;

  // Map of global decls for which we don't have a definition, the keys are
  // names of external vars, the value is whether the def
  // has been seen before.
  std::map<std::string, bool> ExternGVars;

  // Maps for global/static functions, global variables.
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

  // Maps each ASTContext to a unique index in the vector of ASTs being
  // processed. This is used to uniquely determine the translation unit an AST
  // belongs to given its corresponding ASTContext. By using this index instead
  // of the name of the main file, this uniquely identifies the translation unit
  // even when a file is the main file of multiple translation units. The values
  // in this map are used as part of the IDAndTranslationUnit which is the type
  // used as keys for maps from ASTNodes.
  std::map<ASTContext *, unsigned int> TranslationUnitIdxMap;

  // Inserts the given FVConstraint set into the extern or static function map.
  // Returns the merged version if it was a redeclaration, or the constraint
  // parameter if it was new.
  FunctionVariableConstraint *
  insertNewFVConstraint(FunctionDecl *FD, FVConstraint *FVCon, ASTContext *C);

  // Retrieves a FVConstraint* from a Decl (which could be static, or global)
  FVConstraint *getFuncFVConstraint(FunctionDecl *FD, ASTContext *C);

  void insertIntoPtrSourceMap(PersistentSourceLoc PSL, ConstraintVariable *CV);

  void computePtrLevelStats();

  void insertCVAtoms(ConstraintVariable *CV,
                     std::map<ConstraintKey, ConstraintVariable *> &AtomMap);

  // For each pointer type in the declaration of D, add a variable to the
  // constraint system for that pointer type.
  void addVariable(clang::DeclaratorDecl *D,
                   clang::ASTContext *AstContext) override;

  void linkFunction(FunctionVariableConstraint *FV);
};

#endif
