//=--AVarBoundsInfo.h---------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the bounds information about various ARR atoms.
//
//===----------------------------------------------------------------------===//
#ifndef _AVARBOUNDSINFO_H
#define _AVARBOUNDSINFO_H

#include "ABounds.h"
#include "AVarGraph.h"
#include "ProgramVar.h"
#include "clang/3C/ConstraintVariables.h"
#include "clang/3C/PersistentSourceLoc.h"
#include "clang/AST/Decl.h"

class ProgramInfo;
class ConstraintResolver;

// Class that maintains stats about how the bounds of various variables is
// computed.
class AVarBoundsStats {
public:
  // Found by using variables that start with same prefix as the corresponding
  // array variable.
  std::set<BoundsKey> NamePrefixMatch;
  // Found by using allocation sites.
  std::set<BoundsKey> AllocatorMatch;
  // Found by using variable names that match size related words.
  std::set<BoundsKey> VariableNameMatch;
  // Neighbour scalar parameter match.
  std::set<BoundsKey> NeighbourParamMatch;
  // These are dataflow matches i.e., matches found by dataflow analysis
  std::set<BoundsKey> DataflowMatch;
  // These are bounds keys for which the bounds are declared.
  std::set<BoundsKey> DeclaredBounds;
  AVarBoundsStats() { clear(); }
  ~AVarBoundsStats() { clear(); }

  bool isDataflowMatch(BoundsKey BK) {
    return DataflowMatch.find(BK) != DataflowMatch.end();
  }
  bool isNamePrefixMatch(BoundsKey BK) {
    return NamePrefixMatch.find(BK) != NamePrefixMatch.end();
  }
  bool isAllocatorMatch(BoundsKey BK) {
    return AllocatorMatch.find(BK) != AllocatorMatch.end();
  }
  bool isVariableNameMatch(BoundsKey BK) {
    return VariableNameMatch.find(BK) != VariableNameMatch.end();
  }
  bool isNeighbourParamMatch(BoundsKey BK) {
    return NeighbourParamMatch.find(BK) != NeighbourParamMatch.end();
  }
  void print(llvm::raw_ostream &O, const std::set<BoundsKey> *InSrcArrs,
             bool JsonFormat = false) const;
  void dump(const std::set<BoundsKey> *InSrcArrs) const {
    print(llvm::errs(), InSrcArrs);
  }

private:
  void clear() {
    NamePrefixMatch.clear();
    AllocatorMatch.clear();
    VariableNameMatch.clear();
    NeighbourParamMatch.clear();
    DataflowMatch.clear();
    DeclaredBounds.clear();
  }
};

// Priority for bounds.
enum BoundsPriority {
  Declared = 1, // Highest priority: These are declared by the user.
  Allocator,    // Second priority: allocator based bounds.
  FlowInferred, // Flow based bounds.
  Heuristics,   // Least-priority, based on heuristics.
  Invalid       // Invalid priority type.
};

class AVarBoundsInfo;
typedef std::map<ABounds::BoundsKind, std::set<BoundsKey>> BndsKindMap;
// The main class that handles figuring out bounds of arr variables.
class AvarBoundsInference {
public:
  AvarBoundsInference(AVarBoundsInfo *BoundsInfo) : BI(BoundsInfo) {
    clearInferredBounds();
  }

  // Clear all possible inferred bounds for all the BoundsKeys
  void clearInferredBounds() {
    CurrIterInferBounds.clear();
    BKsFailedFlowInference.clear();
  }

  // Infer bounds for the given key from the set of given ARR atoms.
  // The flag FromPB requests the inference to use potential length variables.
  bool inferBounds(BoundsKey K, AVarGraph &BKGraph, bool FromPB = false);

  // Get a consistent bound for all the arrays whose bounds have been
  // inferred.
  bool convergeInferredBounds();

private:
  // Find all the reachable variables form FromVarK that are visible
  // in DstScope
  bool getReachableBoundKeys(const ProgramVarScope *DstScope,
                             BoundsKey FromVarK, std::set<BoundsKey> &PotK,
                             AVarGraph &BKGraph, bool CheckImmediate = false);

  // Check if bounds specified by Bnds are declared bounds of K.
  bool areDeclaredBounds(
      BoundsKey K,
      const std::pair<ABounds::BoundsKind, std::set<BoundsKey>> &Bnds);

  // Get all the bounds of the given array i.e., BK
  bool getRelevantBounds(BoundsKey BK, BndsKindMap &ResBounds);

  // Predict possible bounds for DstArrK from the bounds of  Neighbours.
  // Return true if there is any change in the captured bounds information.
  bool predictBounds(BoundsKey DstArrK, std::set<BoundsKey> &Neighbours,
                     AVarGraph &BKGraph);

  void mergeReachableProgramVars(std::set<BoundsKey> &AllVars);

  AVarBoundsInfo *BI;

  // Potential Bounds for each bounds key inferred for the current iteration.
  std::map<BoundsKey, BndsKindMap> CurrIterInferBounds;
  // BoundsKey that failed the flow inference.
  std::set<BoundsKey> BKsFailedFlowInference;
};

class AVarBoundsInfo {
public:
  AVarBoundsInfo()
      : ProgVarGraph(this), CtxSensProgVarGraph(this),
        RevCtxSensProgVarGraph(this) {
    BCount = 1;
    PVarInfo.clear();
    InProgramArrPtrBoundsKeys.clear();
    BInfo.clear();
    DeclVarMap.clear();
    TmpBoundsKey.clear();
    CSBoundsKey.clear();
    ArrPointersWithArithmetic.clear();
  }

  typedef std::tuple<std::string, std::string, bool, unsigned> ParamDeclType;

  // Checks if the given declaration is a valid bounds variable.
  bool isValidBoundVariable(clang::Decl *D);

  void insertDeclaredBounds(clang::Decl *D, ABounds *B);
  bool mergeBounds(BoundsKey L, BoundsPriority P, ABounds *B);
  bool removeBounds(BoundsKey L, BoundsPriority P = Invalid);
  bool replaceBounds(BoundsKey L, BoundsPriority P, ABounds *B);
  ABounds *getBounds(BoundsKey L, BoundsPriority ReqP = Invalid,
                     BoundsPriority *RetP = nullptr);
  bool updatePotentialCountBounds(BoundsKey BK, std::set<BoundsKey> &CntBK);

  // Try and get BoundsKey, into R, for the given declaration. If the
  // declaration does not have a BoundsKey then return false.
  bool tryGetVariable(clang::Decl *D, BoundsKey &R);
  // Try and get bounds for the expression.
  bool tryGetVariable(clang::Expr *E, const ASTContext &C, BoundsKey &R);

  // Insert the variable into the system.
  void insertVariable(clang::Decl *D);

  // Get variable helpers. These functions will fatal fail if the provided
  // Decl cannot have a BoundsKey
  BoundsKey getVariable(clang::VarDecl *VD);
  BoundsKey getVariable(clang::ParmVarDecl *PVD);
  BoundsKey getVariable(clang::FieldDecl *FD);
  BoundsKey getVariable(clang::FunctionDecl *FD);
  BoundsKey getConstKey(uint64_t value);

  // Generate a random bounds key to be used for inference.
  BoundsKey getRandomBKey();

  // Add Assignments between variables. These methods will add edges between
  // corresponding BoundsKeys
  bool addAssignment(clang::Decl *L, clang::Decl *R);
  bool addAssignment(clang::DeclRefExpr *L, clang::DeclRefExpr *R);
  bool addAssignment(BoundsKey L, BoundsKey R);
  bool handlePointerAssignment(clang::Stmt *St, clang::Expr *L, clang::Expr *R,
                               ASTContext *C, ConstraintResolver *CR);
  bool handleAssignment(clang::Expr *L, const CVarSet &LCVars, clang::Expr *R,
                        const CVarSet &RCVars, ASTContext *C,
                        ConstraintResolver *CR);
  bool handleAssignment(clang::Decl *L, CVarOption LCVar, clang::Expr *R,
                        const CVarSet &RCVars, ASTContext *C,
                        ConstraintResolver *CR);
  // Handle context sensitive assignment.
  bool handleContextSensitiveAssignment(CallExpr *CE, clang::Decl *L,
                                        ConstraintVariable *LCVar,
                                        clang::Expr *R, CVarSet &RCVars,
                                        ASTContext *C, ConstraintResolver *CR);

  // Handle the arithmetic expression. This is required to adjust bounds
  // for pointers that has pointer arithmetic performed on them.
  void recordArithmeticOperation(clang::Expr *E, ConstraintResolver *CR);

  // Check if the given bounds key has a pointer arithmetic done on it.
  bool hasPointerArithmetic(BoundsKey BK);

  // Get the ProgramVar for the provided VarKey.
  ProgramVar *getProgramVar(BoundsKey VK);

  // Function that does brain transplant of the provided bounds key (NewBK)
  // with existing bounds key (OldBK).
  void brainTransplant(BoundsKey NewBK, BoundsKey OldBK);

  // Propagate the array bounds information for all array ptrs.
  bool performFlowAnalysis(ProgramInfo *PI);

  // Reset (i.e., clear) the context sensitive bounds information.
  void resetContextSensitiveBoundsKey();
  // Create context sensitive BoundsKey variables for the given set of
  // ConstraintVariables.
  bool contextualizeCVar(CallExpr *CE, const std::set<ConstraintVariable *> &CV,
                         ASTContext *C);
  // Get the context sensitive BoundsKey for the given key.
  // If there exists no context-sensitive bounds key, we just return
  // the provided key.
  BoundsKey getContextSensitiveBoundsKey(CallExpr *CE, BoundsKey BK);

  AVarBoundsStats &getBStats() { return BoundsInferStats; }

  // Dump the AVar graph to the provided dot file.
  void dumpAVarGraph(const std::string &DFPath);

  // Print the stats about computed bounds information.
  void print_stats(llvm::raw_ostream &O, const CVarSet &SrcCVarSet,
                   bool JsonFormat = false) const;

  bool areSameProgramVar(BoundsKey B1, BoundsKey B2);

private:
  friend class AvarBoundsInference;

  friend struct llvm::DOTGraphTraits<AVarGraph>;
  // List of bounds priority in descending order of priorities.
  static std::vector<BoundsPriority> PrioList;

  // Variable that is used to generate new bound keys.
  BoundsKey BCount;
  // Map of VarKeys and corresponding program variables.
  std::map<BoundsKey, ProgramVar *> PVarInfo;
  // Map of APSInt (constants) and a BoundKey that correspond to it.
  std::map<uint64_t, BoundsKey> ConstVarKeys;
  // Map of BoundsKey and corresponding prioritized bounds information.
  // Note that although each PSL could have multiple ConstraintKeys Ex: **p.
  // Only the outer most pointer can have bounds.
  std::map<BoundsKey, std::map<BoundsPriority, ABounds *>> BInfo;
  // Set that contains BoundsKeys of variables which have invalid bounds.
  std::set<BoundsKey> InvalidBounds;
  // These are the bounds key of the pointers that has arithmetic operations
  // performed on them.
  std::set<BoundsKey> ArrPointersWithArithmetic;
  // Set of BoundsKeys that correspond to pointers.
  std::set<BoundsKey> PointerBoundsKey;
  // Set of BoundsKey that correspond to array pointers.
  std::set<BoundsKey> ArrPointerBoundsKey;
  // Set of BoundsKey that correspond to array pointers with in the program
  // being compiled i.e., it does not include array pointers that belong
  // to libraries.
  std::set<BoundsKey> InProgramArrPtrBoundsKeys;

  // These are temporary bound keys generated during inference.
  // They do not correspond to any bounds variable.
  std::set<BoundsKey> TmpBoundsKey;

  // BiMap of Persistent source loc and BoundsKey of regular variables.
  BiMap<PersistentSourceLoc, BoundsKey> DeclVarMap;
  // BiMap of parameter keys and BoundsKey for function parameters.
  BiMap<ParamDeclType, BoundsKey> ParamDeclVarMap;
  // BiMap of function keys and BoundsKey for function return values.
  BiMap<std::tuple<std::string, std::string, bool>, BoundsKey> FuncDeclVarMap;

  // Graph of all program variables.
  AVarGraph ProgVarGraph;
  // Graph that contains only edges from normal BoundsKey to
  // context-sensitive BoundsKey.
  AVarGraph CtxSensProgVarGraph;
  // Same as above but in the reverse direction.
  AVarGraph RevCtxSensProgVarGraph;
  // Stats on techniques used to find length for various variables.
  AVarBoundsStats BoundsInferStats;
  // This is the map of pointer variable bounds key and set of bounds key
  // which can be the count bounds.
  std::map<BoundsKey, std::set<BoundsKey>> PotentialCntBounds;

  // Context sensitive bounds key.
  // For each call-site a map of original bounds key and the bounds key
  // specific to this call-site.
  // Note: This map is only active for the compilation unit being parsed.
  std::map<CallExpr *, std::map<BoundsKey, BoundsKey>> CSBoundsKey;

  // BoundsKey helper function: These functions help in getting bounds key from
  // various artifacts.
  bool hasVarKey(PersistentSourceLoc &PSL);

  BoundsKey getVarKey(PersistentSourceLoc &PSL);

  BoundsKey getVarKey(llvm::APSInt &API);

  void insertVarKey(PersistentSourceLoc &PSL, BoundsKey NK);

  void insertProgramVar(BoundsKey NK, ProgramVar *PV);

  void insertCtxSensBoundsKey(ProgramVar *OldPV, BoundsKey NK,
                              const CtxFunctionArgScope *CFAS);

  // Check if the provided bounds key corresponds to function return.
  bool isFunctionReturn(BoundsKey BK);

  // Of all the pointer bounds key, find arr pointers.
  void computerArrPointers(ProgramInfo *PI, std::set<BoundsKey> &Ret);

  // Get all the array pointers that need bounds.
  void getBoundsNeededArrPointers(const std::set<BoundsKey> &ArrPtrs,
                                  std::set<BoundsKey> &AB);

  // Keep only highest priority bounds for all the provided BoundsKeys
  // returns true if any thing changed, else false.
  bool keepHighestPriorityBounds(std::set<BoundsKey> &ArrPtrs);

  // Perform worklist based inference on the requested array variables using
  // the provided graph and potential length variables.
  bool performWorkListInference(const std::set<BoundsKey> &ArrNeededBounds,
                                AVarGraph &BKGraph, AvarBoundsInference &BI);

  void insertParamKey(ParamDeclType ParamDecl, BoundsKey NK);
};

// This class creates context sensitive bounds key information that is
// useful to resolve certain bounds information.
// Consider the following example:
// _Arry_ptr<int> foo(unsigned int s) : count(s);
// ....
// int *a, *c;
// unsigned b, d;
// a = foo(b);
// c = foo(d);
// ...
// Here, when we do our analysis we do not know whether b or d is the bounds
// of a.
// The reason for this is because we maintain a single bounds variable for foo,
// consequently, when we do our flow analysis we see that b and d both propagate
// to s (which is the bounds of the return value of foo).
// However, if we maintain context sensitive bounds keys, then we know that
// at a = foo(b), it is b that is passed to s and there by helps us infer that
// the bounds of a should be b i.e., _Array_ptr<a> : count(b).
// This class helps in maintaining the context sensitive bounds information.
class ContextSensitiveBoundsKeyVisitor
    : public RecursiveASTVisitor<ContextSensitiveBoundsKeyVisitor> {
public:
  explicit ContextSensitiveBoundsKeyVisitor(ASTContext *C, ProgramInfo &I,
                                            ConstraintResolver *CResolver);

  virtual ~ContextSensitiveBoundsKeyVisitor();

  bool VisitCallExpr(CallExpr *CE);

private:
  ASTContext *Context;
  ProgramInfo &Info;
  ConstraintResolver *CR;
};

#endif // _AVARBOUNDSINFO_H
