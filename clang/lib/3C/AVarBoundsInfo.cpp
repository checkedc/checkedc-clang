//=--AVarBoundsInfo.cpp-------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the implementation of methods in AVarBoundsInfo.h.
//
//===----------------------------------------------------------------------===//

#include "clang/3C/AVarBoundsInfo.h"
#include "clang/3C/ConstraintResolver.h"
#include "clang/3C/ProgramInfo.h"
#include "clang/3C/3CGlobalOptions.h"
#include <sstream>
#include <clang/3C/LowerBoundAssignment.h>

std::vector<BoundsPriority> AVarBoundsInfo::PrioList{Declared, Allocator,
                                                     FlowInferred, Heuristics};

void AVarBoundsStats::print(llvm::raw_ostream &O,
                            const std::set<BoundsKey> *InSrcArrs,
                            bool JsonFormat) const {
  std::set<BoundsKey> Tmp;
  if (!JsonFormat) {
    O << "Array Bounds Inference Stats:\n";
    findIntersection(NamePrefixMatch, *InSrcArrs, Tmp);
    O << "NamePrefixMatch:" << Tmp.size() << "\n";
    findIntersection(AllocatorMatch, *InSrcArrs, Tmp);
    O << "AllocatorMatch:" << Tmp.size() << "\n";
    findIntersection(VariableNameMatch, *InSrcArrs, Tmp);
    O << "VariableNameMatch:" << Tmp.size() << "\n";
    findIntersection(NeighbourParamMatch, *InSrcArrs, Tmp);
    O << "NeighbourParamMatch:" << Tmp.size() << "\n";
    findIntersection(DataflowMatch, *InSrcArrs, Tmp);
    O << "DataflowMatch:" << Tmp.size() << "\n";
    findIntersection(DeclaredBounds, *InSrcArrs, Tmp);
    O << "Declared:" << Tmp.size() << "\n";
    findIntersection(DeclaredButNotHandled, *InSrcArrs, Tmp);
    O << "DeclaredButNotHandled:" << Tmp.size() << "\n";
  } else {
    O << "\"ArrayBoundsInferenceStats\":{";
    findIntersection(NamePrefixMatch, *InSrcArrs, Tmp);
    O << "\"NamePrefixMatch\":" << Tmp.size() << ",\n";
    findIntersection(AllocatorMatch, *InSrcArrs, Tmp);
    O << "\"AllocatorMatch\":" << Tmp.size() << ",\n";
    findIntersection(VariableNameMatch, *InSrcArrs, Tmp);
    O << "\"VariableNameMatch\":" << Tmp.size() << ",\n";
    findIntersection(NeighbourParamMatch, *InSrcArrs, Tmp);
    O << "\"NeighbourParamMatch\":" << Tmp.size() << ",\n";
    findIntersection(DataflowMatch, *InSrcArrs, Tmp);
    O << "\"DataflowMatch\":" << Tmp.size() << ",\n";
    findIntersection(DeclaredBounds, *InSrcArrs, Tmp);
    O << "\"Declared\":" << Tmp.size() << ",\n";
    findIntersection(DeclaredButNotHandled, *InSrcArrs, Tmp);
    O << "\"DeclaredButNotHandled\":" << Tmp.size() << "\n";
    O << "}";
  }
}

bool hasArray(const ConstraintVariable *CK, const Constraints &CS) {
  auto &E = CS.getVariables();
  if (const auto *PV = dyn_cast<PVConstraint>(CK)) {
    if (PV->hasArr(E, 0) || PV->hasNtArr(E, 0)) {
      return true;
    }
  }
  return false;
}

bool hasOnlyNtArray(const ConstraintVariable *CK, const Constraints &CS) {
  auto &E = CS.getVariables();
  if (const auto *PV = dyn_cast<PVConstraint>(CK)) {
    if (PV->hasNtArr(E, 0)) {
      return true;
    }
  }
  return false;
}

bool isInSrcArray(const ConstraintVariable *CK, const Constraints &CS) {
  auto &E = CS.getVariables();
  if (const auto *PV = dyn_cast<PVConstraint>(CK)) {
    if ((PV->hasArr(E, 0) || PV->hasNtArr(E, 0)) && PV->isForValidDecl()) {
      return true;
    }
  }
  return false;
}

// This class picks variables that are in the same scope as the provided scope.
class ScopeVisitor {
public:
  ScopeVisitor(const ProgramVarScope *S, AVarBoundsInfo *BI)
    : Scope(S), InScopeKeys(), VisibleKeys(), BI(BI) {}
  void visitBoundsKey(BoundsKey V) {
    if (ProgramVar *S = BI->getProgramVar(V)) {
      // If the variable is constant or in the same scope?
      if (S->isNumConstant() || (*Scope == *(S->getScope()))) {
        InScopeKeys.insert(V);
        VisibleKeys.insert(V);
      } else if (Scope->isInInnerScope(*(S->getScope()))) {
        VisibleKeys.insert(V);
      }
    }
  }

  const std::set<BoundsKey> &getInScopeKeys() const { return InScopeKeys; }

  const std::set<BoundsKey> &getVisibleKeys() const { return VisibleKeys; }

private:
  const ProgramVarScope *Scope;

  // Contains high priority bounds keys. These are either directly in the scope
  // for this visitor, or they are constant bounds keys.
  std::set<BoundsKey> InScopeKeys;

  // This set contains all keys in InScopeKeys, but also contains non-constant
  // bounds keys from scopes where this scope is an inner scope.
  std::set<BoundsKey> VisibleKeys;

  const AVarBoundsInfo *BI;
};

void AvarBoundsInference::mergeReachableProgramVars(
    BoundsKey TarBK, std::set<BoundsKey> &AllVars) {
  if (AllVars.size() > 1) {
    bool IsTarNTArr = BI->NtArrPointerBoundsKey.find(TarBK) !=
                      BI->NtArrPointerBoundsKey.end();
    // First, find all variables that are in the SAME scope as TarBK.
    // If there is only one? Then use it.
    if (ProgramVar *TarBVar = BI->getProgramVar(TarBK)) {
      std::set<BoundsKey> SameScopeVars;
      for (auto TB : AllVars)
        if (*(BI->getProgramVar(TB)->getScope()) == *(TarBVar->getScope()))
          SameScopeVars.insert(TB);

      // There is only one same scope variable. Consider only that.
      if (SameScopeVars.size() == 1) {
        AllVars = SameScopeVars;
        return;
      }
    }

    // We want to merge all bounds vars. We give preference to
    // non-constants if there are multiple non-constant variables,
    // we give up.
    ProgramVar *BVar = nullptr;
    for (auto TmpBKey : AllVars) {
      // Convert the bounds key to corresponding program var.
      auto *TmpB = BI->getProgramVar(TmpBKey);
      // First case.
      if (BVar == nullptr) {
        BVar = TmpB;
      } else if (BVar->isNumConstant()) {
        // Case when one variable is constant and other is not.
        if (!TmpB->isNumConstant()) {
          // We give preference to non-constant lengths.
          BVar = TmpB;
        } else {
          // If we need to merge two constants?
          uint64_t CVal = BVar->getConstantVal();
          uint64_t TmpVal = TmpB->getConstantVal();
          if (IsTarNTArr) {
            // If this is an NTarr then the values should be same.
            if (TmpVal != CVal) {
              BVar = nullptr;
              break;
            }
          } else if (TmpVal < CVal) {
            // Else (if array), pick the lesser value.
            BVar = TmpB;
          }
        }
      } else if (!TmpB->isNumConstant()) {
        // Case when both are non-constant variables.
        auto *BScope = BVar->getScope();
        auto *TScope = TmpB->getScope();
        if (*BScope != *TScope) {
          // Is the new variable in inner scope (i.e., more close)?
          if (TScope->isInInnerScope(*BScope)) {
            BVar = TmpB;
          } else if (!BScope->isInInnerScope(*TScope)) {
            // Variables are in different scope and their visibilities are
            // incomparable. We give up.
            BVar = nullptr;
            break;
          }
        } else if (BVar->getKey() != TmpB->getKey()) {
          // The variables are in same scope, but are different variables.
          // We give up.
          BVar = nullptr;
          break;
        }
      }
    }
    AllVars.clear();
    if (BVar)
      AllVars.insert(BVar->getKey());
  }
}

// Consider all pointers, each of which may have multiple bounds, and intersect
// these. If they all converge to one possibility, use that. If not, give up and
// don't assign any bounds to the pointer.
void AvarBoundsInference::convergeInferredBounds() {
  for (auto &CInfABnds : CurrIterInferBounds) {
    BoundsKey PtrBoundsKey = CInfABnds.first;
    // If there are no bounds?
    if (BI->getBounds(PtrBoundsKey) == nullptr) {
      // Maps ABounds::BoundsKind to the set of possible bounds of this kind for
      // the current PtrBoundsKey.
      auto &BKindMap = CInfABnds.second;
      for (auto &TySet : BKindMap)
        mergeReachableProgramVars(PtrBoundsKey, TySet.second);

      ABounds *NewBound = getPreferredBound(PtrBoundsKey);
      // If we found any bounds?
      if (NewBound != nullptr) {
        // Record that we inferred bounds using data-flow.
        BI->BoundsInferStats.DataflowMatch.insert(PtrBoundsKey);
        BI->replaceBounds(PtrBoundsKey, BoundsPriority::FlowInferred, NewBound);
      } else {
        BKsFailedFlowInference.insert(PtrBoundsKey);
      }
    }
  }
}

// Construct an array bound with the most preferred kind from the bounds kind
// map. Count bounds have the highest priority, followed by byte count and then
// count-plus-one bounds. This function assumes that the BoundsKey sets in the
// map contain either zero or one BoundsKey. This is be achieved by first
// passing the sets to `mergeReachableProgramVars`.
ABounds *AvarBoundsInference::getPreferredBound(BoundsKey BK) {
  BoundsKey BaseVar = 0;
  bool NeedsBasePointer =
    BI->InvalidLowerBounds.find(BK) != BI->InvalidLowerBounds.end();
  if (NeedsBasePointer && BI->LowerBounds.find(BK) != BI->LowerBounds.end())
    BaseVar = BI->LowerBounds[BK];

  assert("Lower bound required but not available." &&
         (!NeedsBasePointer || BaseVar != 0));

  const auto &BKindMap = CurrIterInferBounds[BK];
  // Utility to check if the map contains a non-empty set of bounds for a
  // particular kind. This makes the following if statements much cleaner.
  auto HasBoundKind = [&BKindMap](ABounds::BoundsKind Kind) {
    return BKindMap.find(Kind) != BKindMap.end() && !BKindMap.at(Kind).empty();
  };

  // Order of preference: Count, Byte, Count-plus-one
  if (HasBoundKind(ABounds::CountBoundKind))
    return new CountBound(getOnly(BKindMap.at(ABounds::CountBoundKind)),
                          BaseVar);

  if (HasBoundKind(ABounds::ByteBoundKind))
    return new ByteBound(getOnly(BKindMap.at(ABounds::ByteBoundKind)), BaseVar);

  if (HasBoundKind(ABounds::CountPlusOneBoundKind))
    return new CountPlusOneBound(
      getOnly(BKindMap.at(ABounds::CountPlusOneBoundKind)), BaseVar);

  return nullptr;
}

bool AvarBoundsInference::hasImpossibleBounds(BoundsKey BK) {
  return this->BI->PointersWithImpossibleBounds.find(BK) !=
         this->BI->PointersWithImpossibleBounds.end();
}

void AvarBoundsInference::setImpossibleBounds(BoundsKey BK) {
  this->BI->PointersWithImpossibleBounds.insert(BK);
  this->BI->removeBounds(BK);
}

// This function finds all the BoundsKeys (i.e., variables) in
// scope `DstScope` that are reachable from `FromVarK` in the
// graph `BKGraph`. All the reachable bounds key will be stored in `PotK`.
bool AvarBoundsInference::getReachableBoundKeys(const ProgramVarScope *DstScope,
                                                BoundsKey FromVarK,
                                                std::set<BoundsKey> &PotK,
                                                const AVarGraph &BKGraph,
                                                bool CheckImmediate) {

  // First, find all the in-scope variable to which the SBKey flow to.
  auto *FromProgramVar = BI->getProgramVar(FromVarK);

  // If both are in the same scope?
  if (*DstScope == *FromProgramVar->getScope()) {
    PotK.insert(FromVarK);
    if (CheckImmediate) {
      return true;
    }
  }

  // All constants are reachable!
  if (FromProgramVar->isNumConstant()) {
    PotK.insert(FromVarK);
  }

  // Find all in scope variables reachable from the FromVarK bounds variable.
  ScopeVisitor TV(DstScope, BI);
  BKGraph.visitBreadthFirst(FromVarK,
                            [&TV](BoundsKey BK) { TV.visitBoundsKey(BK); });
  // Prioritize in scope keys.
  if (!TV.getInScopeKeys().empty()) {
    PotK.insert(TV.getInScopeKeys().begin(), TV.getInScopeKeys().end());
  } else {
    PotK.insert(TV.getVisibleKeys().begin(), TV.getVisibleKeys().end());

    // This condition is necessary for array bounds using global variables.
    // The bounds keys for global variable do not appear in the BKGraph array
    // bounds graph, so breadth first search finds visits an empty set of nodes,
    // not even visiting the initial bounds key. This ensures the global
    // variable is added to the set of potential keys.
    if (DstScope->isInInnerScope(*BI->getProgramVar(FromVarK)->getScope()))
      PotK.insert(FromVarK);
  }

  // This is to get all the constants that are assigned to the variables
  // reachable from FromVarK.
  if (!FromProgramVar->isNumConstant()) {
    std::set<BoundsKey> CurrBK;
    CurrBK.insert(PotK.begin(), PotK.end());
    CurrBK.insert(FromVarK);
    for (auto CK : CurrBK) {
      std::set<BoundsKey> Pre;
      BKGraph.getPredecessors(CK, Pre);
      for (auto T : Pre) {
        auto *TVar = BI->getProgramVar(T);
        if (TVar != nullptr && TVar->isNumConstant()) {
          PotK.insert(T);
        }
      }
    }
  }

  return !PotK.empty();
}

void AvarBoundsInference::getRelevantBounds(BoundsKey BK,
                                            BndsKindMap &ResBounds) {
  if (CurrIterInferBounds.find(BK) != CurrIterInferBounds.end()) {
    // get the bounds inferred from the current iteration
    ResBounds = CurrIterInferBounds[BK];
  } else if (ABounds *PrevBounds = BI->getBounds(BK)) {
    ResBounds[PrevBounds->getKind()].insert(PrevBounds->getLengthKey());
  }
}

bool AvarBoundsInference::areDeclaredBounds(
    BoundsKey K,
    const std::pair<ABounds::BoundsKind, std::set<BoundsKey>> &Bnds) {
  bool IsDeclaredB = false;
  // Get declared bounds and check that Bnds are same as the declared
  // bounds.
  ABounds *DeclB = this->BI->getBounds(K, BoundsPriority::Declared, nullptr);
  if (DeclB && DeclB->getKind() == Bnds.first) {
    IsDeclaredB = true;
    for (auto TmpNBK : Bnds.second) {
      if (!this->BI->areSameProgramVar(TmpNBK, DeclB->getLengthKey())) {
        IsDeclaredB = false;
        break;
      }
    }
  }
  return IsDeclaredB;
}

bool AvarBoundsInference::predictBounds(BoundsKey K,
                                        const std::set<BoundsKey> &Neighbours,
                                        const AVarGraph &BKGraph) {
  bool ErrorOccurred = false;
  bool IsFuncRet = BI->isFunctionReturn(K);
  ProgramVar *KVar = this->BI->getProgramVar(K);

  // Bounds inferred from each of the neighbours.
  std::map<BoundsKey, BndsKindMap> InferredNBnds;
  // For each of the Neighbour, try to infer possible bounds.
  for (auto NBK : Neighbours) {
    ErrorOccurred = false;
    BndsKindMap NeighboursBnds;
    getRelevantBounds(NBK, NeighboursBnds);
    if (!NeighboursBnds.empty()) {
      for (auto &NKBChoice : NeighboursBnds) {
        ABounds::BoundsKind NeighbourKind = NKBChoice.first;
        const std::set<BoundsKey> &NeighbourSet = NKBChoice.second;

        std::set<BoundsKey> InfBK;
        for (BoundsKey NeighborBK : NeighbourSet)
          getReachableBoundKeys(KVar->getScope(), NeighborBK, InfBK, BKGraph);

        if (!InfBK.empty()) {
          InferredNBnds[NBK][NeighbourKind] = InfBK;
        } else {
          bool IsDeclaredB = areDeclaredBounds(NBK, NKBChoice);

          if (!IsDeclaredB || _3COpts.DisableInfDecls) {
            // Oh, there are bounds for neighbour NBK but no bounds
            // can be inferred for K from it.
            InferredNBnds.clear();
            ErrorOccurred = true;
            break;
          }
        }
      }
    } else if (IsFuncRet || (BKsFailedFlowInference.find(NBK) !=
                             BKsFailedFlowInference.end())) {

      // If this is a function return we should have bounds from all
      // neighbours.
      ErrorOccurred = true;
    } else if (hasImpossibleBounds(NBK)) {
      // if the neighbour has impossible bounds?
      // Consider that current pointer to also have impossible bounds.
      setImpossibleBounds(K);
      ErrorOccurred = true;
    }
    if (ErrorOccurred) {
      // If an error occurred while processing bounds from neighbours/
      // clear the inferred bounds and break.
      InferredNBnds.clear();
      break;
    }
  }

  bool IsChanged = false;
  if (!InferredNBnds.empty()) {
    // All the possible inferred bounds for K.
    BndsKindMap InferredKBnds;
    // TODO: Figure out if there is a discrepancy and try to implement
    // root-cause analysis.

    // Find intersection of all bounds from neighbours.
    for (auto &IN : InferredNBnds) {
      for (auto &INB : IN.second) {
        ABounds::BoundsKind NeighbourKind = INB.first;
        const std::set<BoundsKey> &NeighbourSet = INB.second;
        if (InferredKBnds.find(NeighbourKind) == InferredKBnds.end()) {
          InferredKBnds[NeighbourKind] = NeighbourSet;
        } else {
          const std::set<BoundsKey> &KBoundsOfKind = InferredKBnds[NeighbourKind];
          // Keep the bounds in the intersection between the current bounds and
          // the bounds from the neighbor.
          std::set<BoundsKey> SharedBounds;
          findIntersection(KBoundsOfKind, NeighbourSet, SharedBounds);

          // Also keep all constant bounds. Later on we will keep only the
          // constant bound with the lowest value.
          std::set<BoundsKey> AllBounds = KBoundsOfKind;
          AllBounds.insert(NeighbourSet.begin(), NeighbourSet.end());
          for (auto CK : AllBounds) {
            auto *CKVar = this->BI->getProgramVar(CK);
            if (CKVar != nullptr && CKVar->isNumConstant())
              SharedBounds.insert(CK);
          }
          InferredKBnds[NeighbourKind] = SharedBounds;
        }
      }
    }

    // Now from the newly inferred bounds i.e., InferredKBnds, check
    // if is is different from previously known bounds of K
    for (auto &IKB : InferredKBnds) {
      ABounds::BoundsKind InferredKind = IKB.first;
      std::set<BoundsKey> InferredSet = IKB.second;
      bool Handled = false;
      if (CurrIterInferBounds.find(K) != CurrIterInferBounds.end()) {
        BndsKindMap &CurrentBoundsMap = CurrIterInferBounds[K];
        if (CurrentBoundsMap.find(InferredKind) != CurrentBoundsMap.end()) {
          Handled = true;
          if (CurrentBoundsMap[InferredKind] != InferredSet) {
            CurrentBoundsMap[InferredKind] = InferredSet;
            if (InferredSet.empty())
              CurrentBoundsMap.erase(InferredKind);
            IsChanged = true;
          }
        }
      }
      if (!Handled) {
        CurrIterInferBounds[K][InferredKind] = InferredSet;
        if (InferredSet.empty()) {
          CurrIterInferBounds[K].erase(InferredKind);
        } else {
          IsChanged = true;
        }
      }
    }
  } else if (ErrorOccurred) {
    // If any error occurred during inferring bounds then
    // remove any previously inferred bounds for K.
    IsChanged = CurrIterInferBounds.erase(K) != 0;
  }
  return IsChanged;
}

bool AvarBoundsInference::inferBounds(BoundsKey K, const AVarGraph &BKGraph,
                                      bool FromPB) {
  bool IsChanged = false;

  // If a lower bound could not be inferred for a BoundsKey, then we refuse to
  // infer an upper bound for it as well. This prevents inferring incorrect
  // bounds when a bound would propagate through a pointer without a lower
  // bound.
  if (BI->hasLowerBound(K) &&
      BI->InvalidBounds.find(K) == BI->InvalidBounds.end()) {
    // Infer from potential bounds?
    if (FromPB) {
      IsChanged = inferFromPotentialBounds(K, BKGraph);
    } else {
      // Infer from the flow-graph.
      // Try to predict bounds from predecessors.
      std::set<BoundsKey> PredKeys;
      BKGraph.getPredecessors(K, PredKeys);
      IsChanged = predictBounds(K, PredKeys, BKGraph);
    }
  }
  return IsChanged;
}


void AVarBoundsInfo::computeInvalidLowerBounds(ProgramInfo *PI) {
  // This will compute a breadth first search starting from the constant
  // InvalidLowerBoundKey. Any reachable keys are also invalid lower bounds.
  // This is essentially the same algorithm as is used for solving the checked
  // type constraint graph.
  assert(InvalidLowerBounds.empty());
  std::queue<BoundsKey> WorkList;
  WorkList.push(InvalidLowerBoundKey);

  // To check if a bounds key is already invalidated we check if it is either in
  // the set of invalidated keys, or is the constant InvalidLowerBoundKey.
  auto IsInvalidated = [this](BoundsKey BK) {
    return BK == InvalidLowerBoundKey ||
           InvalidLowerBounds.find(BK) != InvalidLowerBounds.end();
  };

  while (!WorkList.empty()) {
    BoundsKey Curr = WorkList.front();
    WorkList.pop();
    assert(IsInvalidated(Curr));

    std::set<BoundsKey> Neighbors;
    LowerBoundGraph.getSuccessors(Curr, Neighbors);
    for (BoundsKey NK : Neighbors) {
      // This is an awful hack to work around a problem during conversion phase
      // two. A parameter would be given count bounds, with a local being
      // created to hold the range bounds. A conversion is done with
      // -itypes-for-extern` and the headers are copied over. The version of
      // the header in the local directory now has count bounds on an itype. If
      // we trust those bounds, then the next conversion does not emit range
      // bounds.
      PointerVariableConstraint *PVC = getConstraintVariable(PI, NK);
      // Strictly speaking, this can occur outside of -itypes-for-extern, but
      // it is unlikely, and I've decided that the risk of unintentionally
      // changing other behavior is greater than the risk that this special
      // case will be needed in some other circumstance.
      bool IsItypeParam = _3COpts.ItypesForExtern && PVC && PVC->srcHasItype();

      // The neighbors of an invalid lower bound are also invalid, with the
      // exception that if there is a bound in the source code, then we assume
      // the bound is correct, and so the pointer is a valid lower bound for
      // itself.
      bool HasDeclaredBounds =
        getBounds(NK, BoundsPriority::Declared) != nullptr;
      if ((IsItypeParam || !HasDeclaredBounds) && !IsInvalidated(NK)) {
        InvalidLowerBounds.insert(NK);
        WorkList.push(NK);
      }
    }
  }
}

void
AVarBoundsInfo::inferLowerBounds(ProgramInfo *PI) {
  computeInvalidLowerBounds(PI);

  // This maps array pointers to a single consistent lower bound pointer, or
  // possible the constant InvalidLowerBoundKey if no lower bound could be found
  // or generated. Note that there can only be a single valid lower bound.
  // Future work can extend this map to track sets if possible lower bounds.
  std::map<BoundsKey, BoundsKey> InfLBs;

  // Lower bound inference will proceed as a traversal of the LowerBoundGraph.
  // The traversal starts at the direct predecessors of the pointers that need
  // an inferred lower bound.
  std::queue<BoundsKey> WorkList;
  for (BoundsKey BK: InvalidLowerBounds) {
    std::set<BoundsKey> Pred;
    LowerBoundGraph.getPredecessors(BK, Pred);
    for (BoundsKey Seed: Pred) {
      if (Seed != InvalidLowerBoundKey &&
          InvalidLowerBounds.find(Seed) == InvalidLowerBounds.end()) {
        // This pointer is a valid lower bound for itself, so add it to the
        // worklist and initialize it in the map of inferred lower bounds.
        InfLBs[Seed] = Seed;
        WorkList.push(Seed);
      }
    }
  }

  // It's possible for there to be invalid lower bounds that are not reachable
  // from any valid lower bounds, so we also initialize the worklist with all
  // invalid lower bounds. These come after the valid lower bounds so that is
  // less likely a fresh lower bound will be generated but later thrown out, as
  // that process is inefficient at least in the current implementation.
  for (BoundsKey InvLB : InvalidLowerBounds)
    WorkList.push(InvLB);

  // This set tracks the pointers for which we will need to generate a fresh
  // lower bound pointer. These pointers do not have a single consistent lower
  // bound in the source code, but 3C is able to insert a duplicate declaration
  // to act as the lower bound.
  std::set<BoundsKey> NeedFreshLB;

  // This set track the pointers that have multiple inconsistent lower bounds.
  // This is used to differentiate pointers that need a fresh lower bound
  // because no lower bounds could be found from pointers that need a fresh
  // lower bound because there were multiple inconsistent lower bounds. Note
  // that is not a subset of NeedFreshLB because a pointer may have conflicting
  // bounds but be ineligible for a fresh lower bound.
  std::set<BoundsKey> HasConflictingBounds;

  while (!WorkList.empty()) {
    BoundsKey BK = WorkList.front();
    WorkList.pop();

    if (isEligibleForFreshLowerBound(BK) &&
        (InfLBs.find(BK) == InfLBs.end() ||
         InfLBs[BK] == InvalidLowerBoundKey)) {
      // We've reached an array pointer in the work list that either has not
      // been assigned a lower bound, or has multiple conflicting lower bounds.
      // We will generate a fresh lower bound.
      assert(
        "Generating fresh bound for pointer that can be its own lower bound." &&
        InvalidLowerBounds.find(BK) != InvalidLowerBounds.end());
      InfLBs[BK] = getFreshLowerBound(BK);
      NeedFreshLB.insert(BK);
    }

    std::set<BoundsKey> Succ;
    LowerBoundGraph.getSuccessors(BK, Succ);
    for (BoundsKey S : Succ) {

      // Do not process any array pointers that are valid lower bounds. They
      // should just serve as their own lower bound.
      if (InvalidLowerBounds.find(S) == InvalidLowerBounds.end())
        continue;

      if (InfLBs.find(S) == InfLBs.end()) {
        // No prior lower bound known for `S`. Initialize it to use the same
        // lower bound as `BK`, if this is possible given their scopes.
        if (isInAccessibleScope(S, InfLBs[BK])) {
          InfLBs[S] = InfLBs[BK];
        } else {
          InfLBs[S] = InvalidLowerBoundKey;
        }
        WorkList.push(S);
      } else if (InfLBs[BK] != InfLBs[S] &&
                 HasConflictingBounds.find(S) == HasConflictingBounds.end()) {
        // The lower bound of `BK` is not the same as the current inferred lower
        // bounds of `S`. This is a problem, so we need to mark `S` as having
        // conflicting lower bounds. We only do this invalidation step once. If
        // the BoundsKey is already known to have conflicting bounds, then do
        // not reset it again. Doing so can cause an infinite loop when there is
        // a cycle of BoundsKeys needing a lower bound, where each BoundsKey in
        // the cycle is eligible for a fresh lower bound.
        HasConflictingBounds.insert(S);

        if (NeedFreshLB.find(S) != NeedFreshLB.end()) {
          // This case handles when we a fresh lower bounds was created for `S`
          // before any conflict was detected. It is possible that the conflict
          // we detect here only exists between the fresh lower bound and the
          // lower bound of `BK`. In this case, we can drop the fresh lower
          // bounds to use the inferred lower bound. In order to fully drop the
          // fresh bound, we must also drop it from all BoundsKey reachable from
          // `S`, as these may have already had a lower bound inferred based on
          // the fresh lower bound.
          NeedFreshLB.erase(S);
          BoundsKey SLB = InfLBs[S];
          // TODO: Erasing the bounds by a breadth first search from S is
          //       inefficient, probably resulting in quadratic worst case
          //       running time, but this hasn't shown up a real performance
          //       issue yet.
          LowerBoundGraph.visitBreadthFirst(S, [this, SLB, &InfLBs, &WorkList](
            BoundsKey BK) {
            if (InfLBs.find(BK) != InfLBs.end() && InfLBs[BK] == SLB) {
              InfLBs.erase(BK);
              std::set<BoundsKey> Pred;
              LowerBoundGraph.getPredecessors(BK, Pred);
              for (BoundsKey P : Pred) {
                if (P != InvalidLowerBoundKey &&
                    InfLBs.find(P) != InfLBs.end())
                  WorkList.push(P);
              }
            }
          });
        } else if (InfLBs[S] != InvalidLowerBoundKey) {
          // If no fresh lower bound was generated, then things are much
          // simpler. Just invalidate the lower bound of `S` and enqueue it.
          InfLBs[S] = InvalidLowerBoundKey;
          WorkList.push(S);
        }
      }
      // Otherwise, the a lower bound exists for `S`, and it's the same lower
      // bounds as `BK`. Nothing changes, so don't enqueue `S`.
    }
  }

  NeedFreshLowerBounds = NeedFreshLB;
  LowerBounds = InfLBs;

  // This is an awful hack to work around a problem during conversion phase
  // two.
  if (_3COpts.ItypesForExtern) {
    for (auto InferredLBPair : LowerBounds) {
      if (BInfo[InferredLBPair.first][Declared]) {
        BInfo[InferredLBPair.first][Declared]->setLowerBoundKey(
          InferredLBPair.second);
      }
    }
  }
}

BoundsKey AVarBoundsInfo::getFreshLowerBound(BoundsKey Arr) {
  ProgramVar *ArrVar = getProgramVar(Arr);
  BoundsKey FreshLB = getRandomBKey();
  ProgramVar *FreshLBVar =
    ProgramVar::createNewProgramVar(FreshLB,
                                    "__3c_lower_bound_" + ArrVar->getVarName(),
                                    ArrVar->getScope());
  insertProgramVar(FreshLB, FreshLBVar);
  return FreshLB;
}

bool AVarBoundsInfo::hasLowerBound(BoundsKey K)  {
  return InvalidLowerBounds.find(K) == InvalidLowerBounds.end() ||
         (LowerBounds.find(K) != LowerBounds.end() &&
          LowerBounds[K] != InvalidLowerBoundKey);
}

bool AvarBoundsInference::inferFromPotentialBounds(BoundsKey BK,
                                                   const AVarGraph &BKGraph) {
  // If we have any inferred bounds for K then ignore potential bounds.
  bool HasInferredBound = false;
  if (CurrIterInferBounds.find(BK) != CurrIterInferBounds.end()) {
    auto &BM = CurrIterInferBounds[BK];
    HasInferredBound = llvm::any_of(BM, [](auto InfB) {
      return !InfB.second.empty();
    });
  }

  if (!HasInferredBound) {
    auto &PotBDs = BI->PotBoundsInfo;
    // Here, the logic is:
    // We first try potential bounds and if there are no potential bounds?
    // then, we check if there are count(i+1) bounds.
    ProgramVar *Kvar = BI->getProgramVar(BK);
    // These are potential count bounds.
    ABounds::BoundsKind PotKind = ABounds::CountBoundKind;
    std::set<BoundsKey> PotentialB;
    if (PotBDs.hasPotentialCountBounds(BK)) {
      for (auto TK : PotBDs.getPotentialBounds(BK))
        getReachableBoundKeys(Kvar->getScope(), TK, PotentialB, BKGraph, true);
    }
    if (PotentialB.empty() && PotBDs.hasPotentialCountPOneBounds(BK)) {
      // These are potential count (i + 1) bounds.
      PotKind = ABounds::CountPlusOneBoundKind;
      for (auto TK : PotBDs.getPotentialBoundsPOne(BK))
        getReachableBoundKeys(Kvar->getScope(), TK, PotentialB, BKGraph, true);
    }
    if (!PotentialB.empty()) {
      CurrIterInferBounds[BK][PotKind] = PotentialB;
      return true;
    }
  }
  return false;
}

bool PotentialBoundsInfo::hasPotentialCountBounds(BoundsKey PtrBK) {
  return PotentialCntBounds.find(PtrBK) != PotentialCntBounds.end();
}

std::set<BoundsKey> &PotentialBoundsInfo::getPotentialBounds(BoundsKey PtrBK) {
  assert(hasPotentialCountBounds(PtrBK) && "Has no potential bounds");
  return PotentialCntBounds[PtrBK];
}

void PotentialBoundsInfo::addPotentialBounds(BoundsKey BK,
                                             const std::set<BoundsKey> &PotK) {
  if (!PotK.empty()) {
    auto &TmpK = PotentialCntBounds[BK];
    TmpK.insert(PotK.begin(), PotK.end());
  }
}

bool PotentialBoundsInfo::hasPotentialCountPOneBounds(BoundsKey PtrBK) {
  return PotentialCntPOneBounds.find(PtrBK) != PotentialCntPOneBounds.end();
}

std::set<BoundsKey> &
PotentialBoundsInfo::getPotentialBoundsPOne(BoundsKey PtrBK) {
  assert(hasPotentialCountPOneBounds(PtrBK) &&
         "Has no potential count+1 bounds");
  return PotentialCntPOneBounds[PtrBK];
}
void PotentialBoundsInfo::addPotentialBoundsPOne(
    BoundsKey BK, const std::set<BoundsKey> &PotK) {
  if (!PotK.empty()) {
    auto &TmpK = PotentialCntPOneBounds[BK];
    TmpK.insert(PotK.begin(), PotK.end());
  }
}

bool AVarBoundsInfo::isValidBoundVariable(clang::Decl *D) {
  if (D == nullptr)
    return false;

  // Any pointer declaration in an unwritable file without existing bounds
  // annotations is not valid. This ensures we do not add bounds onto pointers
  // where attempting to rewrite the variable to insert the bound would be an
  // error. If there are existing bounds, no new bound will be inferred, so no
  // rewriting will be attempted. By leaving existing bounds as valid, these
  // bounds can be used infer bounds on other (writeable) declarations.
  // Non-pointer types are also still valid because these will never need bounds
  // expression, and they need to remain valid so that they can be used by
  // existing array pointer bounds.
  auto PSL = PersistentSourceLoc::mkPSL(D, D->getASTContext());
  if (!canWrite(PSL.getFileName())) {
    if (auto *DD = dyn_cast<DeclaratorDecl>(D))
      return !DD->getType()->isPointerType() || DD->hasBoundsExpr();
    return false;
  }

  // All return and field values are valid bound variables.
  if (isa<FunctionDecl>(D) || isa<FieldDecl>(D))
    return true;

  // For Parameters, check if they belong to a valid function.
  // Function pointer types are not considered valid functions, so function
  // pointer parameters are disqualified as valid bound variables here.
  if (auto *PD = dyn_cast<ParmVarDecl>(D))
    return PD->getParentFunctionOrMethod() != nullptr;

  // For VarDecls, check if these are not dummy and have a name.
  if (auto *VD = dyn_cast<VarDecl>(D))
    return !VD->getNameAsString().empty();

  return false;
}

void AVarBoundsInfo::insertDeclaredBounds(BoundsKey BK, ABounds *B) {
  if (B != nullptr) {
    // If there is already bounds information, release it.
    removeBounds(BK);
    BInfo[BK][Declared] = B;
    BoundsInferStats.DeclaredBounds.insert(BK);
  } else {
    // Set bounds to be invalid.
    InvalidBounds.insert(BK);
    BoundsInferStats.DeclaredButNotHandled.insert(BK);
  }
}

void AVarBoundsInfo::insertDeclaredBounds(clang::Decl *D, ABounds *B) {
  assert(isValidBoundVariable(D) && "Declaration not a valid bounds variable");
  BoundsKey BK;
  tryGetVariable(D, BK);
  insertDeclaredBounds(BK, B);
}

bool AVarBoundsInfo::tryGetVariable(clang::Decl *D, BoundsKey &R) {
  bool RetVal = false;
  if (isValidBoundVariable(D)) {
    RetVal = true;
    if (ParmVarDecl *PD = dyn_cast<ParmVarDecl>(D)) {
      if (PD->getParentFunctionOrMethod()) {
        R = getVariable(PD);
      } else {
        RetVal = false;
      }
    } else if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      R = getVariable(VD);
    } else if (FieldDecl *FD = dyn_cast<FieldDecl>(D)) {
      R = getVariable(FD);
    } else if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
      R = getVariable(FD);
    } else {
      assert(false && "Invalid Declaration\n");
    }
    return RetVal;
  }
  return RetVal;
}

bool AVarBoundsInfo::tryGetVariable(clang::Expr *E, const ASTContext &C,
                                    BoundsKey &Res) {
  if (E != nullptr) {
    E = E->IgnoreParenCasts();

    // Get the BoundsKey for the constant value if the expression is a constant
    // integer expression.
    Optional<llvm::APSInt> OptConsVal = E->getIntegerConstantExpr(C);
    if (E->getType()->isArithmeticType() && OptConsVal.hasValue()) {
      Res = getVarKey(*OptConsVal);
      return true;
    }

    // For declarations or struct member references, get the bounds key for the
    // references variables or field.
    if (auto *DRE = dyn_cast<DeclRefExpr>(E))
      return tryGetVariable(DRE->getDecl(), Res);
    if (auto *ME = dyn_cast<MemberExpr>(E))
      return tryGetVariable(ME->getMemberDecl(), Res);
  }
  return false;
}

// Merging bounds B with the present bounds of key L at the same priority P
// Returns true if we update the bounds for L (with B)
bool AVarBoundsInfo::mergeBounds(BoundsKey L, BoundsPriority P, ABounds *B) {
  if (B->getLowerBoundKey() == InvalidLowerBoundKey &&
      LowerBounds.find(L) != LowerBounds.end())
    B->setLowerBoundKey(LowerBounds[L]);
  bool RetVal = false;
  if (BInfo.find(L) != BInfo.end() && BInfo[L].find(P) != BInfo[L].end()) {
    // If previous computed bounds are not same? Then release the old bounds.
    if (!BInfo[L][P]->areSame(B, this)) {
      InvalidBounds.insert(L);
      // TODO: Should we keep bounds for other priorities?
      removeBounds(L);
    }
  } else {
    BInfo[L][P] = B;
    RetVal = true;
  }
  return RetVal;
}

bool AVarBoundsInfo::removeBounds(BoundsKey L, BoundsPriority P) {
  bool RetVal = false;
  if (BInfo.find(L) != BInfo.end()) {
    auto &PriBInfo = BInfo[L];
    if (P == Invalid) {
      // Delete bounds for all priorities.
      for (auto &T : PriBInfo) {
        delete (T.second);
      }
      BInfo.erase(L);
      RetVal = true;
    } else {
      // Delete bounds for only the given priority.
      if (PriBInfo.find(P) != PriBInfo.end()) {
        delete (PriBInfo[P]);
        PriBInfo.erase(P);
        RetVal = true;
      }
      // If there are no other bounds then remove the key.
      if (BInfo[L].empty()) {
        BInfo.erase(L);
        RetVal = true;
      }
    }
  }
  return RetVal;
}

bool AVarBoundsInfo::replaceBounds(BoundsKey L, BoundsPriority P, ABounds *B) {
  removeBounds(L);
  return mergeBounds(L, P, B);
}

ABounds *AVarBoundsInfo::getBounds(BoundsKey L, BoundsPriority ReqP,
                                   BoundsPriority *RetP) {
  if (InvalidBounds.find(L) == InvalidBounds.end() &&
      BInfo.find(L) != BInfo.end()) {
    auto &PriBInfo = BInfo[L];
    if (ReqP == Invalid) {
      // Fetch bounds by priority i.e., give the highest priority bounds.
      for (BoundsPriority P : PrioList) {
        if (PriBInfo.find(P) != PriBInfo.end()) {
          if (RetP != nullptr)
            *RetP = P;
          return PriBInfo[P];
        }
      }
      assert(false && "Bounds present but has invalid priority.");
    } else if (PriBInfo.find(ReqP) != PriBInfo.end()) {
      return PriBInfo[ReqP];
    }
  }
  return nullptr;
}

void AVarBoundsInfo::updatePotentialCountBounds(
    BoundsKey BK, const std::set<BoundsKey> &CntBK) {
  PotBoundsInfo.addPotentialBounds(BK, CntBK);
}

void AVarBoundsInfo::updatePotentialCountPOneBounds(
    BoundsKey BK, const std::set<BoundsKey> &CntBK) {
  PotBoundsInfo.addPotentialBoundsPOne(BK, CntBK);
}

void AVarBoundsInfo::insertVariable(clang::Decl *D) {
  BoundsKey Tmp;
  tryGetVariable(D, Tmp);
}

BoundsKey AVarBoundsInfo::getVariable(clang::VarDecl *VD) {
  assert(isValidBoundVariable(VD) && "Not a valid bound declaration.");
  PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(VD, VD->getASTContext());
  if (!hasVarKey(PSL)) {
    BoundsKey NK = ++BCount;
    insertVarKey(PSL, NK);
    const ProgramVarScope *PVS = nullptr;
    if (VD->hasGlobalStorage()) {
      PVS = GlobalScope::getGlobalScope();
    } else {
      FunctionDecl *FD =
          dyn_cast<FunctionDecl>(VD->getParentFunctionOrMethod());
      if (FD != nullptr) {
        PVS = FunctionScope::getFunctionScope(FD->getNameAsString(),
                                              FD->isStatic());
      }
    }
    assert(PVS != nullptr && "Context not null");
    auto *PVar =
        ProgramVar::createNewProgramVar(NK, VD->getNameAsString(), PVS);
    insertProgramVar(NK, PVar);
    if (isPtrOrArrayType(VD->getType())) {
      PointerBoundsKey.insert(NK);
      // Global variables cannot be given range bounds because it is not
      // possible to initialize a duplicated pointer variable with the same
      // value as the original.
      // TODO: Followup issue: Implementing the rewriting here would be easy,
      //       but it would also require change the compiler to recognize
      //       dynamic bounds casts are constant expressions, which doesn't
      //       sound too hard.
      if (!VD->isLocalVarDeclOrParm())
        markIneligibleForFreshLowerBound(NK);
    }
  }
  return getVarKey(PSL);
}

BoundsKey AVarBoundsInfo::getVariable(clang::ParmVarDecl *PVD) {
  assert(isValidBoundVariable(PVD) && "Not a valid bound declaration.");
  FunctionDecl *FD = dyn_cast<FunctionDecl>(PVD->getDeclContext());
  unsigned int ParamIdx = getParameterIndex(PVD, FD);
  auto Psl = PersistentSourceLoc::mkPSL(FD, FD->getASTContext());
  std::string FileName = Psl.getFileName();
  auto ParamKey = std::make_tuple(FD->getNameAsString(), FileName,
                                  FD->isStatic(), ParamIdx);
  if (ParamDeclVarMap.left().find(ParamKey) == ParamDeclVarMap.left().end()) {
    BoundsKey NK = ++BCount;
    const FunctionParamScope *FPS = FunctionParamScope::getFunctionParamScope(
        FD->getNameAsString(), FD->isStatic());
    std::string ParamName = PVD->getNameAsString();

    if (ParamName.empty()) {
      // The parameter declaration doesn't have a name. Try to get the
      // corresponding function definition, and then the parameter declaration
      // in that function. Use the name of that parameter.
      // TODO: I think the declaration merging code written by kyle does a good
      //       job of handling missing/inconsistent parameter names. Can I just
      //       use that?
      if (auto *FnDef = FD->getDefinition()) {
        if (FnDef->getNumParams() >= ParamIdx) {
          if (auto *DefPVD = FnDef->getParamDecl(ParamIdx)) {
            ParamName = DefPVD->getNameAsString();
          }
        }
      }
      // If we still couldn't find a name, then we make one up using the
      // parameter index.
      // TODO: Is this better than leaving the name string empty? There are
      //       situations where an identifier can be omitted without error, but
      //       using an undeclared identifier is an error.
      if (ParamName.empty())
        ParamName = "NONAMEPARAM_" + std::to_string(ParamIdx);
    }

    auto *PVar = ProgramVar::createNewProgramVar(NK, ParamName, FPS);
    insertProgramVar(NK, PVar);
    insertParamKey(ParamKey, NK);
    if (isPtrOrArrayType(PVD->getType())) {
      PointerBoundsKey.insert(NK);
      // We do not give range bounds to parameters with an array type. Doing
      // this causes the local variable duplicate definition to have an array
      // type, but pointer arithmetic on constant size arrays is not allowed.
      // TODO: Follow up issue: Can we add some special logic in rewriting to
      //       emit the duplicate definition with an _Array_pointer type?
      if (isArrayType(PVD->getType()))
        markIneligibleForFreshLowerBound(NK);
    }
  }
  return ParamDeclVarMap.left().at(ParamKey);
}

BoundsKey AVarBoundsInfo::getVariable(clang::FunctionDecl *FD) {
  assert(isValidBoundVariable(FD) && "Not a valid bound declaration.");
  auto Psl = PersistentSourceLoc::mkPSL(FD, FD->getASTContext());
  std::string FileName = Psl.getFileName();
  auto FuncKey =
      std::make_tuple(FD->getNameAsString(), FileName, FD->isStatic());
  if (FuncDeclVarMap.left().find(FuncKey) == FuncDeclVarMap.left().end()) {
    BoundsKey NK = ++BCount;
    const FunctionParamScope *FPS = FunctionParamScope::getFunctionParamScope(
        FD->getNameAsString(), FD->isStatic());

    auto *PVar =
        ProgramVar::createNewProgramVar(NK, FD->getNameAsString(), FPS);
    insertProgramVar(NK, PVar);
    FuncDeclVarMap.insert(FuncKey, NK);
    if (isPtrOrArrayType(FD->getReturnType())) {
      PointerBoundsKey.insert(NK);
      // Fresh lower bounds are not inserted for function returns. I don't see a
      // way around this limitation.
      markIneligibleForFreshLowerBound(NK);
    }
  }
  return FuncDeclVarMap.left().at(FuncKey);
}

BoundsKey AVarBoundsInfo::getVariable(clang::FieldDecl *FD) {
  assert(isValidBoundVariable(FD) && "Not a valid bound declaration.");
  PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(FD, FD->getASTContext());
  if (!hasVarKey(PSL)) {
    BoundsKey NK = ++BCount;
    insertVarKey(PSL, NK);
    std::string StName = FD->getParent()->getNameAsString();
    const StructScope *SS = StructScope::getStructScope(StName);
    auto *PVar = ProgramVar::createNewProgramVar(NK, FD->getNameAsString(), SS);
    insertProgramVar(NK, PVar);
    if (isPtrOrArrayType(FD->getType())) {
      PointerBoundsKey.insert(NK);
      // Fields are not rewritten with range bounds because we would need to
      // duplicate the field and update all structure initializations to
      // properly set the new field.
      // TODO: Followup issue: Add the duplicate declaration as a new field in
      //       the struct and then also update all struct initializer to include
      //       the new field.
      markIneligibleForFreshLowerBound(NK);
    }
  }
  return getVarKey(PSL);
}

BoundsKey AVarBoundsInfo::getRandomBKey() {
  BoundsKey Ret = ++BCount;
  TmpBoundsKey.insert(Ret);
  return Ret;
}

bool AVarBoundsInfo::handleAssignment(clang::Expr *L, const CVarSet &LCVars,
                                      const std::set<BoundsKey> &CSLKeys,
                                      clang::Expr *R, const CVarSet &RCVars,
                                      const std::set<BoundsKey> &CSRKeys,
                                      ASTContext *C, ConstraintResolver *CR) {
  bool Ret = false;
  BoundsKey TmpK;
  std::set<BoundsKey> AllLKeys = CSLKeys;
  std::set<BoundsKey> AllRKeys = CSRKeys;
  if (AllLKeys.empty() &&
      (CR->resolveBoundsKey(LCVars, TmpK) || tryGetVariable(L, *C, TmpK))) {
    AllLKeys.insert(TmpK);
  }
  if (AllRKeys.empty() &&
      (CR->resolveBoundsKey(RCVars, TmpK) || tryGetVariable(R, *C, TmpK))) {
    AllRKeys.insert(TmpK);
  }

  for (auto LK : AllLKeys) {
    for (auto RK : AllRKeys) {
      Ret = addAssignment(LK, RK) || Ret;
    }
  }
  return Ret;
}

bool AVarBoundsInfo::handleAssignment(clang::Decl *L, CVarOption LCVars,
                                      clang::Expr *R, const CVarSet &RCVars,
                                      const std::set<BoundsKey> &CSRKeys,
                                      ASTContext *C, ConstraintResolver *CR) {
  BoundsKey LKey, RKey;
  bool Ret = false;
  if (CR->resolveBoundsKey(LCVars, LKey) || tryGetVariable(L, LKey)) {
    std::set<BoundsKey> AllRKeys = CSRKeys;
    if (AllRKeys.empty() &&
        (CR->resolveBoundsKey(RCVars, RKey) || tryGetVariable(R, *C, RKey))) {
      AllRKeys.insert(RKey);
    }
    for (auto RK : AllRKeys) {
      Ret = addAssignment(LKey, RK) || Ret;
    }
  }
  return Ret;
}

bool AVarBoundsInfo::addAssignment(BoundsKey L, BoundsKey R) {
  // If we are adding to function return, do not add bi-directional edges.
  if (isFunctionReturn(L) || isFunctionReturn(R)) {
    // Do not assign edge from return to itself.
    // This is because while inferring bounds of return value, we expect
    // all the variables used in return values to have bounds.
    // So, if we create a edge from return to itself then we create a cyclic
    // dependency and never will be able to find the bounds for the return
    // value.
    if (L != R) {
      ProgVarGraph.addUniqueEdge(R, L);
      LowerBoundGraph.addUniqueEdge(R, L);
    }
  } else {
    ProgVarGraph.addUniqueEdge(R, L);
    LowerBoundGraph.addUniqueEdge(R, L);
    ProgramVar *PV = getProgramVar(R);
    if (!(PV && PV->isNumConstant()))
      ProgVarGraph.addUniqueEdge(L, R);
  }
  return true;
}

bool AVarBoundsInfo::handlePointerAssignment(clang::Expr *L, clang::Expr *R,
                                             ASTContext *C,
                                             ConstraintResolver *CR) {
  if (!isLowerBoundAssignment(L, R))
    recordArithmeticOperation(L, CR);
  return true;
}

void AVarBoundsInfo::mergeBoundsKey(BoundsKey To, BoundsKey From) {
  if (InvalidBounds.find(To) != InvalidBounds.end() ||
      InvalidBounds.find(From) != InvalidBounds.end()) {
    InvalidBounds.insert(To);
    InvalidBounds.insert(From);
  }
}

void AVarBoundsInfo::recordArithmeticOperation(clang::Expr *E,
                                               ConstraintResolver *CR) {
  CVarSet CSet = CR->getExprConstraintVarsSet(E);
  for (auto *CV : CSet) {
    if (CV->hasBoundsKey()) {
      BoundsKey BK = CV->getBoundsKey();
      ArrPointersWithArithmetic.insert(BK);
      LowerBoundGraph.addUniqueEdge(InvalidLowerBoundKey, BK);
    }
  }
}

bool AVarBoundsInfo::needsFreshLowerBound(BoundsKey BK) {
  return NeedFreshLowerBounds.find(BK) != NeedFreshLowerBounds.end();
}

bool AVarBoundsInfo::isEligibleForFreshLowerBound(BoundsKey BK) {
  return IneligibleForFreshLowerBound.find(BK) ==
         IneligibleForFreshLowerBound.end();
}

void AVarBoundsInfo::markIneligibleForFreshLowerBound(BoundsKey BK) {
  IneligibleForFreshLowerBound.insert(BK);
}

bool AVarBoundsInfo::needsFreshLowerBound(ConstraintVariable *CV) {
  if (!CV->hasBoundsKey())
    return false;
  BoundsKey BK = CV->getBoundsKey();
  // A pointer should get range bounds if it is computed by pointer arithmetic
  // and would otherwise need bounds. Some pointers (global variables and struct
  // fields) can't be rewritten to use range bounds (by 3C; Checked C does
  // permit it), so we return false on these.
  return needsFreshLowerBound(BK) && isEligibleForFreshLowerBound(BK) &&
         getBounds(BK) != nullptr;
}

ProgramVar *AVarBoundsInfo::getProgramVar(BoundsKey VK) const {
  if (PVarInfo.find(VK) != PVarInfo.end())
    return PVarInfo.at(VK);
  return nullptr;
}

const ProgramVarScope *AVarBoundsInfo::getProgramVarScope(BoundsKey BK) const{
  if (ProgramVar *Var = getProgramVar(BK))
    return Var->getScope();
  return nullptr;
}

bool AVarBoundsInfo::isInAccessibleScope(BoundsKey From, BoundsKey To) {
  const ProgramVarScope *FromScope = getProgramVarScope(From);
  const ProgramVarScope *ToScope = getProgramVarScope(To);
  return FromScope != nullptr && ToScope != nullptr &&
         (*FromScope == *ToScope || FromScope->isInInnerScope(*ToScope));
}

bool AVarBoundsInfo::scopeCanHaveLowerBound(BoundsKey BK) {
  const ProgramVarScope *BKScope = getProgramVarScope(BK);
  return BKScope != nullptr && !isa<CtxFunctionArgScope>(BKScope) &&
         !isa<CtxStructScope>(BKScope);
}

bool AVarBoundsInfo::hasVarKey(PersistentSourceLoc &PSL) {
  return DeclVarMap.left().find(PSL) != DeclVarMap.left().end();
}

BoundsKey AVarBoundsInfo::getVarKey(PersistentSourceLoc &PSL) {
  assert(hasVarKey(PSL) && "VarKey doesn't exist");
  return DeclVarMap.left().at(PSL);
}

BoundsKey AVarBoundsInfo::getConstKey(uint64_t Value) {
  if (ConstVarKeys.find(Value) == ConstVarKeys.end()) {
    BoundsKey NK = ++BCount;
    ProgramVar *NPV = ProgramVar::createNewConstantVar(NK, Value);
    insertProgramVar(NK, NPV);
    ConstVarKeys[Value] = NK;
  }
  return ConstVarKeys[Value];
}

BoundsKey AVarBoundsInfo::getVarKey(llvm::APSInt &API) {
  return getConstKey(API.abs().getZExtValue());
}

void AVarBoundsInfo::insertVarKey(PersistentSourceLoc &PSL, BoundsKey NK) {
  DeclVarMap.insert(PSL, NK);
}

void AVarBoundsInfo::insertParamKey(AVarBoundsInfo::ParamDeclType ParamDecl,
                                    BoundsKey NK) {
  ParamDeclVarMap.insert(ParamDecl, NK);
}

void AVarBoundsInfo::insertProgramVar(BoundsKey NK, ProgramVar *PV) {
  PVarInfo[NK] = PV;
}

void AVarBoundsInfo::performWorkListInference(const AVarGraph &BKGraph,
                                              AvarBoundsInference &BI,
                                              bool FromPB) {

  // BoundsKeys corresponding to array pointers that need bounds. This will seed
  // the initial WorkList, and be used to ensure that only BoundsKeys needing
  // bounds are added to the list in subsequent iterations.
  std::set<BoundsKey> ArrNeededBounds;
  getBoundsNeededArrPointers(ArrNeededBounds);

  std::set<BoundsKey> WorkList(ArrNeededBounds);
  while (!WorkList.empty()) {
    // This set will collect BoundsKeys which are successors of a BoundsKey that
    // was assigned a bound in this iteration. These subset of these that need
    // bounds will be used as the worklist in the next iteration.
    std::set<BoundsKey> NextIterArrs;

    for (BoundsKey CurrArrKey : WorkList) {
      // inferBounds will return true if a bound was found for CurrArrKey. If a
      // bound can be found, queue the successor nodes for bounds inferences in
      // the next iteration of the outer loop.
      if (BI.inferBounds(CurrArrKey, BKGraph, FromPB)) {
        // Get all the successors of the ARR whose bounds we just found.
        // Successor BoundsKeys are added into NextIterArrs without clearing the
        // current contents.
        BKGraph.getSuccessors(CurrArrKey, NextIterArrs);
      }
    }

    // WorkList will be cleared by findIntersection before it is used to store
    // the intersection of ArrNeededBounds and NextIterArrs. If NextIterArrs is
    // empty, then the intersection will also be empty, and the loop will
    // terminate.
    findIntersection(ArrNeededBounds, NextIterArrs, WorkList);
  }

  // From all the sets of bounds computed for various array variables. Intersect
  // them and find the common bound variable.
  BI.convergeInferredBounds();
}

BoundsKey AVarBoundsInfo::getCtxSensCEBoundsKey(const PersistentSourceLoc &PSL,
                                                BoundsKey BK) {
  return CSBKeyHandler.getCtxSensCEBoundsKey(PSL, BK);
}

void AVarBoundsInfo::computeArrPointers(const ProgramInfo *PI) {
  NtArrPointerBoundsKey.clear();
  ArrPointerBoundsKey.clear();

  for (auto BK : PointerBoundsKey) {
    const PointerVariableConstraint *CV = getConstraintVariable(PI, BK);
    if (CV == nullptr)
      continue;

    if (hasArray(CV, PI->getConstraints()))
      ArrPointerBoundsKey.insert(BK);

    // Does this array belong to a valid program variable?
    if (isInSrcArray(CV, PI->getConstraints()))
      InProgramArrPtrBoundsKeys.insert(BK);

    if (hasOnlyNtArray(CV, PI->getConstraints())) {
      NtArrPointerBoundsKey.insert(BK);
      // If the return value is an nt array pointer and there are no declared
      // bounds? Then, we cannot find bounds for this pointer. This avoids
      // placing incorrect bounds on null terminated arrays as discussed in
      // https://github.com/correctcomputation/checkedc-clang/issues/553
      if (CV->getName() == RETVAR && getBounds(BK) == nullptr)
        PointersWithImpossibleBounds.insert(BK);
    }
  }

  // Get all context-sensitive BoundsKey for each of the actual BKs
  // and consider them to be array pointers as well.
  // Since context-sensitive BoundsKey will be immediate children
  // of the regular bounds key, we just get the neighbours (predecessors
  // and successors) of the regular bounds key to get the context-sensitive
  // counterparts.
  std::set<BoundsKey> TmpBKeys;
  for (auto BK : ArrPointerBoundsKey) {
    CtxSensProgVarGraph.getSuccessors(BK, TmpBKeys, true);
    CtxSensProgVarGraph.getPredecessors(BK, TmpBKeys, true);
    RevCtxSensProgVarGraph.getSuccessors(BK, TmpBKeys, true);
    RevCtxSensProgVarGraph.getPredecessors(BK, TmpBKeys, true);
  }

  for (auto TBK : TmpBKeys) {
    if (ProgramVar *TmpPVar = getProgramVar(TBK)) {
      if (isa<CtxFunctionArgScope>(TmpPVar->getScope()))
        ArrPointerBoundsKey.insert(TBK);
      if (isa<CtxStructScope>(TmpPVar->getScope()))
        ArrPointerBoundsKey.insert(TBK);
    }
  }

  // All BoundsKey that have bounds are also array pointers.
  for (auto &T : this->BInfo)
    ArrPointerBoundsKey.insert(T.first);
}

// Find the set of array pointers that need bounds. This is computed as all
// array pointers that do not currently have a bound, have an invalid bound,
// or have an impossible bound.
void AVarBoundsInfo::getBoundsNeededArrPointers(std::set<BoundsKey> &AB) const {
  // Get the ARR pointers that have bounds.
  std::set<BoundsKey> ArrWithBounds;
  for (auto &T : BInfo) {
    ArrWithBounds.insert(T.first);
  }
  // Also add arrays with invalid bounds.
  ArrWithBounds.insert(InvalidBounds.begin(), InvalidBounds.end());
  // Also, add arrays with impossible bounds.
  ArrWithBounds.insert(PointersWithImpossibleBounds.begin(),
                       PointersWithImpossibleBounds.end());

  // Remove the above set of array pointers with bounds from the set of all
  // array pointers to get the set of array pointers that need bounds.
  // i.e., AB = ArrPointerBoundsKey - ArrPtrsWithBounds.
  std::set_difference(ArrPointerBoundsKey.begin(), ArrPointerBoundsKey.end(),
                      ArrWithBounds.begin(), ArrWithBounds.end(),
                      std::inserter(AB, AB.end()));
}

// We first propagate all the bounds information from explicit
// declarations and mallocs.
// For other variables that do not have any choice of bounds,
// we use potential bounds choices (FromPB), these are the variables
// that are upper bounds to an index variable used in an array indexing
// operation.
// For example:
// if (i < n) {
//  ...arr[i]...
// }
// In the above case, we use n as a potential count bounds for arr.
// Note: we only use potential bounds for a variable when none of its
// predecessors have bounds.
void AVarBoundsInfo::performFlowAnalysis(ProgramInfo *PI) {
  auto &PStats = PI->getPerfStats();
  PStats.startArrayBoundsInferenceTime();

  // First get all the pointer vars which are ARRs. Results is stored in the
  // field ArrPointerBoundsKey. This also populates some other sets that seem to
  // only be used for gather statistics.
  computeArrPointers(PI);

  // Keep only highest priority bounds.
  keepHighestPriorityBounds();

  // Remove flow inferred bounds, if exist for all the array pointers.
  for (auto TBK : ArrPointerBoundsKey)
    removeBounds(TBK, FlowInferred);

  std::set<BoundsKey> ArrNeededBounds;
  getBoundsNeededArrPointers(ArrNeededBounds);

  // Now compute the bounds information of all the ARR pointers that need it.
  // We iterate until there are no new array variables whose bounds are found.
  // The expectation is every iteration we will find bounds for at least one
  // array variable.
  bool OuterChanged = !ArrNeededBounds.empty();
  while (OuterChanged) {
    std::set<BoundsKey> TmpArrNeededBounds = ArrNeededBounds;
    // We first infer with using only flow information i.e., without using any
    // potential bounds. Next, we try using potential bounds.
    // TODO: Doing this with a loop feels kind of silly. I should pull the while
    //       loop into a new function that takes a bool parameter and just call
    //       it twice. I'll do this if I can think of a meaningful name for the
    //       new function.
    for (bool FromPB : std::vector<bool>({false, true})) {
      bool InnerChanged = !ArrNeededBounds.empty();
      while (InnerChanged) {
        AvarBoundsInference ABI(this);
        // Regular flow inference (with no edges between callers and callees).
        performWorkListInference(this->ProgVarGraph, ABI, FromPB);

        // Now propagate the bounds information from context-sensitive keys to
        // original keys (i.e., edges from callers to callees are present, but no
        // local edges).
        performWorkListInference(this->CtxSensProgVarGraph, ABI, FromPB);

        // Now clear all inferred bounds so that context-sensitive nodes do not
        // interfere with each other.
        ABI.clearInferredBounds();

        // Now propagate the bounds information from normal keys to
        // context-sensitive keys.
        performWorkListInference(this->RevCtxSensProgVarGraph, ABI, FromPB);

        // Get array variables that still need bounds.
        std::set<BoundsKey> ArrNeededBoundsNew;
        getBoundsNeededArrPointers(ArrNeededBoundsNew);

        // Did we find bounds for new array variables?
        InnerChanged = (ArrNeededBounds != ArrNeededBoundsNew);
        if (ArrNeededBounds.size() == ArrNeededBoundsNew.size()) {
          assert(!InnerChanged && "New arrays needed bounds after inference");
        }
        assert(ArrNeededBoundsNew.size() <= ArrNeededBounds.size() &&
               "We should always have less number of arrays whose bounds needs "
               "to be inferred after each round.");
        ArrNeededBounds = ArrNeededBoundsNew;
      }
    }
    OuterChanged = (TmpArrNeededBounds != ArrNeededBounds);
  }

  PStats.endArrayBoundsInferenceTime();
}


bool AVarBoundsInfo::keepHighestPriorityBounds() {
  bool HasChanged = false;
  for (auto BK : ArrPointerBoundsKey) {
    bool FoundBounds = false;
    for (BoundsPriority P : PrioList) {
      if (FoundBounds) {
        // We already found bounds. So delete these bounds.
        HasChanged = removeBounds(BK, P) || HasChanged;
      } else if (getBounds(BK, P) != nullptr) {
        FoundBounds = true;
      }
    }
  }
  return HasChanged;
}

void AVarBoundsInfo::dumpBounds() {
  llvm::errs() << "Current Array Bounds: \n";
  for (auto BK : ArrPointerBoundsKey) {
    ProgramVar *PV = getProgramVar(BK);
    ABounds *B = getBounds(BK);
    std::string Name = PV ? PV->verboseStr() : "TMP";
    std::string Bounds = B ? B->mkString(this) : "NO_BOUNDS";
    llvm::errs() << Name << " " << Bounds << "\n";
  }
  llvm::errs() << "\n";
}

void AVarBoundsInfo::dumpAVarGraph(const std::string &DFPath) {
  auto DumpGraph = [DFPath](AVarGraph &G, std::string N) {
    std::error_code Err;
    llvm::raw_fd_ostream DotFile(N + "_" + DFPath, Err);
    llvm::WriteGraph(DotFile, G);
    DotFile.close();
  };
  DumpGraph(ProgVarGraph, "ProgVar");
  DumpGraph(CtxSensProgVarGraph, "CtxSen");
  DumpGraph(RevCtxSensProgVarGraph, "RevCtxSen");
  DumpGraph(LowerBoundGraph, "Invalid");
}

bool AVarBoundsInfo::isFunctionReturn(BoundsKey BK) {
  return (FuncDeclVarMap.right().find(BK) != FuncDeclVarMap.right().end());
}

void AVarBoundsInfo::printStats(llvm::raw_ostream &O, const CVarSet &SrcCVarSet,
                                bool JsonFormat) const {
  std::set<BoundsKey> InSrcBKeys;
  for (auto *C : SrcCVarSet) {
    if (C->isForValidDecl() && C->hasBoundsKey())
      InSrcBKeys.insert(C->getBoundsKey());
  }

  std::set<BoundsKey> NTArraysReqBnds;
  for (auto NTBK : NtArrPointerBoundsKey) {
    ProgVarGraph.visitBreadthFirst(NTBK, [this, NTBK, &NTArraysReqBnds](BoundsKey BK) {
      if (NtArrPointerBoundsKey.find(BK) == NtArrPointerBoundsKey.end() &&
        ArrPointerBoundsKey.find(BK) != ArrPointerBoundsKey.end())
        NTArraysReqBnds.insert(NTBK);
    });
  }

  std::set<BoundsKey> NTArrayReqNoBounds;
  std::set_difference(
      NtArrPointerBoundsKey.begin(), NtArrPointerBoundsKey.end(),
      NTArraysReqBnds.begin(), NTArraysReqBnds.end(),
      std::inserter(NTArrayReqNoBounds, NTArrayReqNoBounds.begin()));

  std::set<BoundsKey> InSrcArrBKeys;
  findIntersection(InProgramArrPtrBoundsKeys, InSrcBKeys, InSrcArrBKeys);
  std::set<BoundsKey> Tmp;
  if (!JsonFormat) {
    findIntersection(ArrPointerBoundsKey, InSrcArrBKeys, Tmp);
    O << "NumPointersNeedBounds:" << Tmp.size() << ",\n";
    findIntersection(NTArrayReqNoBounds, InSrcArrBKeys, Tmp);
    O << "NumNTNoBounds:" << Tmp.size() << ",\n";
    O << "Details:\n";
    findIntersection(InvalidBounds, InSrcArrBKeys, Tmp);
    O << "Invalid:" << Tmp.size() << "\n,BoundsFound:\n";
    BoundsInferStats.print(O, &InSrcArrBKeys, JsonFormat);
  } else {
    findIntersection(ArrPointerBoundsKey, InSrcArrBKeys, Tmp);
    O << "{\"NumPointersNeedBounds\":" << Tmp.size() << ",";
    findIntersection(NTArrayReqNoBounds, InSrcArrBKeys, Tmp);
    O << "\"NumNTNoBounds\":" << Tmp.size() << ",";
    O << "\"Details\":{";
    findIntersection(InvalidBounds, InSrcArrBKeys, Tmp);
    O << "\"Invalid\":" << Tmp.size() << ",\"BoundsFound\":{";
    BoundsInferStats.print(O, &InSrcArrBKeys, JsonFormat);
    O << "}";
    O << "}";
    O << "}";
  }
}

bool AVarBoundsInfo::areSameProgramVar(BoundsKey B1, BoundsKey B2) {
  if (B1 != B2) {
    ProgramVar *P1 = getProgramVar(B1);
    ProgramVar *P2 = getProgramVar(B2);
    return P1->isNumConstant() && P2->isNumConstant() &&
           P1->getConstantVal() == P2->getConstantVal();
  }
  return B1 == B2;
}

bool AVarBoundsInfo::isFuncParamBoundsKey(BoundsKey BK, unsigned &PIdx) {
  auto &ParmBkeyToPSL = ParamDeclVarMap.right();
  if (ParmBkeyToPSL.find(BK) != ParmBkeyToPSL.end()) {
    auto &ParmTup = ParmBkeyToPSL.at(BK);
    PIdx = std::get<3>(ParmTup);
    return true;
  }
  return false;
}

std::set<BoundsKey> AVarBoundsInfo::getCtxSensFieldBoundsKey(Expr *E,
                                                             ASTContext *C,
                                                             ProgramInfo &I) {
  std::set<BoundsKey> Ret;
  if (MemberExpr *ME = dyn_cast_or_null<MemberExpr>(E->IgnoreParenCasts())) {
    BoundsKey NewBK;
    if (CSBKeyHandler.tryGetMECSKey(ME, C, I, NewBK))
      Ret.insert(NewBK);
  }
  return Ret;
}

// Adds declared bounds for all constant sized arrays. This needs to happen
// after constraint solving because the bounds for a _Nt_checked array and a
// _Checked array are different even if they are written with the same length.
// The Checked C bounds for a _Nt_checked array do not include the null
// terminator, but the length as written in the source code does.
void AVarBoundsInfo::addConstantArrayBounds(ProgramInfo &I) {
  for (auto VarEntry : I.getVarMap()) {
    if (auto *VarPCV = dyn_cast<PVConstraint>(VarEntry.second)) {
      if (VarPCV->hasBoundsKey() && VarPCV->isConstantArr()) {
        // Lookup the declared size of the array. This is known because it is
        // written in the source and was stored during constraint generation.
        unsigned int ConstantCount = VarPCV->getConstantArrSize();

        // Check if this array solved to NTARR. If it did, subtract one from the
        // length to account for the null terminator.
        const EnvironmentMap &Env = I.getConstraints().getVariables();
        if (VarPCV->isNtConstantArr(Env)) {
          assert("Size zero constant array should not solve to NTARR" &&
                 ConstantCount != 0);
          ConstantCount--;
        }

        // Insert this as a declared constant count bound for the constraint
        // variable.
        BoundsKey CBKey = getConstKey(ConstantCount);
        ABounds *NB = new CountBound(CBKey);
        insertDeclaredBounds(VarPCV->getBoundsKey(), NB);
      }
    }
  }
}

PVConstraint *AVarBoundsInfo::getConstraintVariable(const ProgramInfo *PI,
                                                    BoundsKey BK) const {
  // Regular variables.
  const auto &VariableMap = DeclVarMap.right();
  if (VariableMap.find(BK) != VariableMap.end()) {
    const PersistentSourceLoc &PSL = VariableMap.at(BK);
    return dyn_cast<PVConstraint>(PI->getVarMap().at(PSL));
  }

  // Function parameters
  const auto &ParamMap = ParamDeclVarMap.right();
  if (ParamMap.find(BK) != ParamMap.end()) {
    auto &ParmTup = ParamMap.at(BK);
    std::string FuncName = std::get<0>(ParmTup);
    std::string FileName = std::get<1>(ParmTup);
    bool IsStatic = std::get<2>(ParmTup);
    unsigned ParmNum = std::get<3>(ParmTup);

    FVConstraint *FV = PI->getFuncConstraint(FuncName, FileName, IsStatic);
    return FV->getExternalParam(ParmNum);
  }

  // Function returns.
  const auto &ReturnMap = FuncDeclVarMap.right();
  if (ReturnMap.find(BK) != ReturnMap.end()) {
    auto &FuncRet = ReturnMap.at(BK);
    std::string FuncName = std::get<0>(FuncRet);
    std::string FileName = std::get<1>(FuncRet);
    bool IsStatic = std::get<2>(FuncRet);

    FVConstraint *FV = PI->getFuncConstraint(FuncName, FileName, IsStatic);
    return FV->getExternalReturn();
  }
  return nullptr;
}
