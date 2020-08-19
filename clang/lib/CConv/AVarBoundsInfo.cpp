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

#include "clang/CConv/AVarBoundsInfo.h"
#include "clang/CConv/ProgramInfo.h"

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
    O << "\"Declared\":" << Tmp.size() << "\n";
    O << "}";
  }
}

bool AVarBoundsInfo::isValidBoundVariable(clang::Decl *D) {
  if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
    return !VD->getNameAsString().empty();
  }
  if (isa<ParmVarDecl>(D)) {
    // All parameters are valid bound variables.
    return true;
  }
  if(FieldDecl *FD = dyn_cast<FieldDecl>(D)) {
    return !FD->getNameAsString().empty();
  }
  return false;
}

void AVarBoundsInfo::insertDeclaredBounds(clang::Decl *D, ABounds *B) {
  assert(isValidBoundVariable(D) && "Declaration not a valid bounds variable");
  BoundsKey BK;
  tryGetVariable(D, BK);
  if (B != nullptr) {
    // If there is already bounds information, release it.
    if (BInfo.find(BK) != BInfo.end()) {
      delete (BInfo[BK]);
    }
    BInfo[BK] = B;
    BoundsInferStats.DeclaredBounds.insert(BK);
  } else {
    // Set bounds to be invalid.
    InvalidBounds.insert(BK);
  }
}

bool AVarBoundsInfo::tryGetVariable(clang::Decl *D, BoundsKey &R) {
  if (isValidBoundVariable(D)) {
    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      R = getVariable(VD);
    }
    if (ParmVarDecl *PD = dyn_cast<ParmVarDecl>(D)) {
      R = getVariable(PD);
    }
    if (FieldDecl *FD = dyn_cast<FieldDecl>(D)) {
      R = getVariable(FD);
    }
    return true;
  }
  return false;
}

bool AVarBoundsInfo::tryGetVariable(clang::Expr *E,
                                    const ASTContext &C,
                                    BoundsKey &Res) {
  llvm::APSInt ConsVal;
  bool Ret = false;
  if (E != nullptr) {
    E = E->IgnoreParenCasts();
    if (E->getType()->isArithmeticType() &&
        E->isIntegerConstantExpr(ConsVal, C)) {
      Res = getVarKey(ConsVal);
      Ret = true;
    } else if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E)) {
      auto *D = DRE->getDecl();
      Ret = tryGetVariable(D, Res);
      if (!Ret) {
        assert(false && "Invalid declaration found inside bounds expression");
      }
    } else if (MemberExpr *ME = dyn_cast<MemberExpr>(E)) {
      return tryGetVariable(ME->getMemberDecl(), Res);
    }
    else {
      // assert(false && "Variable inside bounds declaration is an expression");
    }
  }
  return Ret;
}

bool AVarBoundsInfo::mergeBounds(BoundsKey L, ABounds *B) {
  bool RetVal = false;
  if (BInfo.find(L) != BInfo.end()) {
    // If previous computed bounds are not same? Then release the old bounds.
    if (!BInfo[L]->areSame(B)) {
      InvalidBounds.insert(L);
      delete (BInfo[L]);
      BInfo.erase(L);
    }
  } else {
    BInfo[L] = B;
    RetVal = true;
  }
  return RetVal;
}

bool AVarBoundsInfo::removeBounds(BoundsKey L) {
  bool RetVal = false;
  if (BInfo.find(L) != BInfo.end()) {
    delete (BInfo[L]);
    BInfo.erase(L);
    RetVal = true;
  }
  return RetVal;
}

bool AVarBoundsInfo::replaceBounds(BoundsKey L, ABounds *B) {
  removeBounds(L);
  return mergeBounds(L, B);
}

ABounds *AVarBoundsInfo::getBounds(BoundsKey L) {
  if (InvalidBounds.find(L) == InvalidBounds.end() &&
      BInfo.find(L) != BInfo.end()) {
    return BInfo[L];
  }
  return nullptr;
}

bool AVarBoundsInfo::updatePotentialCountBounds(BoundsKey BK,
                                                std::set<BoundsKey> &CntBK) {
  bool RetVal = false;
  if (!CntBK.empty()) {
    auto &TmpK = PotentialCntBounds[BK];
    TmpK.insert(CntBK.begin(), CntBK.end());
    RetVal = true;
  }
  return RetVal;
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
    ProgramVarScope *PVS = nullptr;
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
    auto *PVar = new ProgramVar(NK, VD->getNameAsString(), PVS);
    insertProgramVar(NK, PVar);
    if (VD->getType()->isPointerType())
      PointerBoundsKey.insert(NK);
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
  assert(ParamIdx >= 0 && "Unable to find parameter.");
  if (ParamDeclVarMap.left().find(ParamKey) == ParamDeclVarMap.left().end()) {
    BoundsKey NK = ++BCount;
    FunctionParamScope *FPS =
        FunctionParamScope::getFunctionParamScope(FD->getNameAsString(),
                                                  FD->isStatic());
    std::string ParamName = PVD->getNameAsString();
    // If this is a parameter without name!?
    // Just get the name from argument number.
    if (ParamName.empty())
      ParamName = "NONAMEPARAM_" + std::to_string(ParamIdx);

    auto *PVar = new ProgramVar(NK, ParamName, FPS);
    insertProgramVar(NK, PVar);
    insertParamKey(ParamKey, NK);
    if (PVD->getType()->isPointerType())
      PointerBoundsKey.insert(NK);
  }
  return ParamDeclVarMap.left().at(ParamKey);
}

BoundsKey AVarBoundsInfo::getVariable(clang::FieldDecl *FD) {
  assert(isValidBoundVariable(FD) && "Not a valid bound declaration.");
  PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(FD, FD->getASTContext());
  if (!hasVarKey(PSL)) {
    BoundsKey NK = ++BCount;
    insertVarKey(PSL, NK);
    std::string StName = FD->getParent()->getNameAsString();
    StructScope *SS = StructScope::getStructScope(StName);
    auto *PVar = new ProgramVar(NK, FD->getNameAsString(), SS);
    insertProgramVar(NK, PVar);
    if (FD->getType()->isPointerType())
      PointerBoundsKey.insert(NK);
  }
  return getVarKey(PSL);
}

BoundsKey AVarBoundsInfo::getRandomBKey() {
  BoundsKey Ret = ++BCount;
  TmpBoundsKey.insert(Ret);
  return Ret;
}

bool AVarBoundsInfo::addAssignment(clang::Decl *L, clang::Decl *R) {
  BoundsKey BL, BR;
  if (tryGetVariable(L, BL) && tryGetVariable(R, BR)) {
    return addAssignment(BL, BR);
  }
  return false;
}

bool AVarBoundsInfo::addAssignment(clang::DeclRefExpr *L,
                                   clang::DeclRefExpr *R) {
  return addAssignment(L->getDecl(), R->getDecl());
}

bool AVarBoundsInfo::addAssignment(BoundsKey L, BoundsKey R) {
  ProgVarGraph.addEdge(L, R);
  ProgVarGraph.addEdge(R, L);
  return true;
}

ProgramVar *AVarBoundsInfo::getProgramVar(BoundsKey VK) {
  ProgramVar *Ret = nullptr;
  if (PVarInfo.find(VK) != PVarInfo.end()) {
    Ret = PVarInfo[VK];
  }
  return Ret;
}

void AVarBoundsInfo::brainTransplant(BoundsKey NewBK, BoundsKey OldBK) {
  // Here, we use the ProgramVar of NewBK and use it for OldBK.
  if (NewBK != OldBK) {
    ProgramVar *NewPVar = getProgramVar(NewBK);
    insertProgramVar(OldBK, NewPVar);
  }
}

bool AVarBoundsInfo::hasVarKey(PersistentSourceLoc &PSL) {
  return DeclVarMap.find(PSL) != DeclVarMap.end();
}

BoundsKey AVarBoundsInfo::getVarKey(PersistentSourceLoc &PSL) {
  assert (hasVarKey(PSL) && "VarKey doesn't exist");
  return DeclVarMap.left().at(PSL);
}

BoundsKey AVarBoundsInfo::getConstKey(uint64_t value) {
  if (ConstVarKeys.find(value) == ConstVarKeys.end()) {
    BoundsKey NK = ++BCount;
    ConstVarKeys[value] = NK;
    std::string ConsString = std::to_string(value);
    ProgramVar *NPV = new ProgramVar(NK, ConsString,
                                     GlobalScope::getGlobalScope(), true);
    insertProgramVar(NK, NPV);
  }
  return ConstVarKeys[value];
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
  if (getProgramVar(NK) != nullptr) {
    // Free the already created variable.
    auto *E = PVarInfo[NK];
    delete (E);
  }
  PVarInfo[NK] = PV;
}

bool hasArray(CVarSet &CSet, Constraints &CS) {
  auto &E = CS.getVariables();
  for (auto *CK : CSet) {
    if (PVConstraint *PV = dyn_cast<PVConstraint>(CK)) {
      if ((PV->hasArr(E, 0) || PV->hasNtArr(E, 0)) &&
          PV->isTopCvarUnsizedArr()) {
        return true;
      }
    }
  }
  return false;
}

bool isInSrcArray(CVarSet &CSet, Constraints &CS) {
  auto &E = CS.getVariables();
  for (auto *CK : CSet) {
    if (PVConstraint *PV = dyn_cast<PVConstraint>(CK)) {
      if ((PV->hasArr(E, 0) || PV->hasNtArr(E, 0)) &&
          PV->isTopCvarUnsizedArr() && PV->isForValidDecl()) {
        return true;
      }
    }
  }
  return false;
}

// This class picks variables that are in the same scope as the provided scope.
class ScopeVisitor {
public:
  ScopeVisitor(ProgramVarScope *S, std::set<ProgramVar *> &R,
               std::map<BoundsKey, ProgramVar *> &VarM,
               std::set<BoundsKey> &P): TS(S), Res(R), VM(VarM)
               , PtrAtoms(P) { }
  void vistBoundsKey(BoundsKey V) const {
    // If the variable is non-pointer?
    if (VM.find(V) != VM.end() && PtrAtoms.find(V) == PtrAtoms.end()) {
      auto *S = VM[V];
      // If the variable is constant or in the same scope?
      if (S->IsNumConstant() ||
          (*(TS) == *(S->getScope()))) {
        Res.insert(S);
      }
    }
  }

  void filterOutBKeys(std::set<BoundsKey> &Src) {
    for (auto BK : Src) {
      // If the variable non-pointer?
      if (PtrAtoms.find(BK) == PtrAtoms.end()) {
        auto *S = VM[BK];
        // If the variable is constant or in the same scope?
        if (S->IsNumConstant() || (*(TS) == *(S->getScope()))) {
          Res.insert(S);
        }
      }
    }
  }
  ProgramVarScope *TS;
  std::set<ProgramVar *> &Res;
  std::map<BoundsKey, ProgramVar *> &VM;
  std::set<BoundsKey> &PtrAtoms;
};

bool AvarBoundsInference::intersectBounds(std::set<ProgramVar *> &ProgVars,
                                          ABounds::BoundsKind BK,
                                          std::set<ABounds *> &CurrB) {
  std::set<ABounds *> CommonNewBounds;
  for (auto *PVar : ProgVars) {
    ABounds *NewB = nullptr;
    if (BK == ABounds::CountBoundKind) {
      NewB = new CountBound(PVar->getKey());
    } else if (BK == ABounds::ByteBoundKind) {
      NewB = new ByteBound(PVar->getKey());
    } else {
      continue;
    }
    assert(NewB != nullptr && "New Bounds cannot be nullptr");
    if (CurrB.empty()) {
      CommonNewBounds.insert(NewB);
    } else {
      bool found = false;
      for (auto *OB : CurrB) {
        if (OB->areSame(NewB)) {
          found = true;
          CommonNewBounds.insert(NewB);
          break;
        }
      }
      if (!found) {
        delete (NewB);
      }
    }
  }

  for (auto *D : CurrB) {
    delete(D);
  }
  CurrB.clear();
  CurrB.insert(CommonNewBounds.begin(), CommonNewBounds.end());
  return !CurrB.empty();
}

bool AvarBoundsInference::inferPossibleBounds(BoundsKey K, ABounds *SB,
                                              std::set<ABounds *> &EB) {
  bool RetVal = false;
  if (SB != nullptr) {
    auto &VarG = BI->ProgVarGraph;
    auto *Kvar = BI->getProgramVar(K);
    bool ValidB = false;
    auto BKind = SB->getKind();
    BoundsKey SBKey;
    if (CountBound *CB = dyn_cast<CountBound>(SB)) {
      ValidB = true;
      SBKey = CB->getCountVar();
    } else if (ByteBound *BB = dyn_cast<ByteBound>(SB)) {
      ValidB = true;
      SBKey = BB->getByteVar();
    }

    std::set<ProgramVar *> PotentialB;
    // If we can handle the bounds?
    if (ValidB) {
      // First, find all the in-scope variable to which the SBKey flow to.
      auto *SBVar = BI->getProgramVar(SBKey);
      if (SBVar->IsNumConstant()) {
        PotentialB.insert(SBVar);
      } else {
        // Find all the in scope variables reachable from the current
        // bounds variable.
        ScopeVisitor TV(Kvar->getScope(), PotentialB, BI->PVarInfo,
                        BI->PointerBoundsKey);
        VarG.visitBreadthFirst(SBKey, [&TV](BoundsKey BK) {
          TV.vistBoundsKey(BK);
        });
      }

      // Are there are other in-scope variables where the bounds variable
      // has been assigned to?
      if (!PotentialB.empty())
        RetVal = intersectBounds(PotentialB, BKind, EB);
    }
  }

  return RetVal;
}

bool AvarBoundsInference::getRelevantBounds(std::set<BoundsKey> &RBKeys,
                                            std::set<ABounds *> &ResBounds) {
  std::set<BoundsKey> IncomingArrs;
  std::set<BoundsKey> TmpIncomingKeys;

  // First, get all the related bounds keys that are arrays.
  findIntersection(RBKeys, BI->ArrPointerBoundsKey, IncomingArrs);
  // Also get all the temporary bounds keys.
  findIntersection(RBKeys, BI->TmpBoundsKey, TmpIncomingKeys);

  // Consider the tmp keys as incoming arrays.
  IncomingArrs.insert(TmpIncomingKeys.begin(), TmpIncomingKeys.end());

  // Next, try to get their bounds.
  bool ValidB = true;
  for (auto PrevBKey : IncomingArrs) {
    auto *PrevBounds = BI->getBounds(PrevBKey);
    // Does the parent arr has bounds?
    if (PrevBounds != nullptr)
      ResBounds.insert(PrevBounds);
  }
  return ValidB;
}

bool AvarBoundsInference::predictBounds(BoundsKey K,
                                        std::set<BoundsKey> &Neighbours,
                                        ABounds **KB) {
  std::set<ABounds *> ResBounds;
  std::set<ABounds *> KBounds;
  *KB = nullptr;
  bool IsValid = true;
  // Get all the relevant bounds from the neighbour ARRs
  if (getRelevantBounds(Neighbours, ResBounds)) {
    if (!ResBounds.empty()) {
      // Find the intersection?
      for (auto *B : ResBounds) {
        inferPossibleBounds(K, B, KBounds);
        //TODO: check this
        // This is stricter version i.e., there should be at least one common
        // bounds information from an incoming ARR.
        /*if (!inferPossibleBounds(K, B, KBounds)) {
          ValidB = false;
          break;
        }*/
      }
      // If we converge to single bounds information? We found the bounds.
      if (KBounds.size() == 1) {
        *KB = *KBounds.begin();
        KBounds.clear();
      } else {
        IsValid = false;
        // TODO: Should we give up when we have multiple bounds?
        for (auto *T : KBounds) {
          delete(T);
        }
      }
    }
  }
  return IsValid;
}
bool AvarBoundsInference::inferBounds(BoundsKey K, bool FromPB) {
  bool IsChanged = false;

  if (BI->InvalidBounds.find(K) == BI->InvalidBounds.end()) {
    ABounds *KB = nullptr;
    // Infer from potential bounds?
    if (FromPB) {
      auto &PotBDs = BI->PotentialCntBounds;
      if (PotBDs.find(K) != PotBDs.end()) {
        auto &VarG = BI->ProgVarGraph;
        ProgramVar *Kvar = BI->getProgramVar(K);
        std::set<ProgramVar *> PotentialB;
        PotentialB.clear();
        for (auto TK : PotBDs[K]) {
          ProgramVar *TKVar = BI->getProgramVar(TK);
          // If the count var is of different scope? Then try to find variables
          // that are in scope.
          if (*Kvar->getScope() != *TKVar->getScope()) {
            // Find all the in scope variables reachable from the current
            // bounds variable.
            ScopeVisitor TV(Kvar->getScope(), PotentialB, BI->PVarInfo,
                            BI->PointerBoundsKey);
            VarG.visitBreadthFirst(TK, [&TV](BoundsKey BK) {
              TV.vistBoundsKey(BK);
            });
          } else {
            PotentialB.insert(TKVar);
          }
        }
        if (!PotentialB.empty()) {
          ProgramVar *BVar = nullptr;
          // We want to merge all bounds vars. We give preference to
          // non-constants if there are multiple non-constant variables,
          // we give up.
          for (auto *TmpB : PotentialB) {
            if (BVar == nullptr) {
              BVar = TmpB;
            } else if (BVar->IsNumConstant()) {
              if (!TmpB->IsNumConstant()) {
                // We give preference to non-constant lengths.
                BVar = TmpB;
              } else if (BVar->getKey() != TmpB->getKey()) {
                // If both are different constants?
                BVar = nullptr;
                break;
              }
            } else if (!TmpB->IsNumConstant() &&
                       BVar->getKey() != TmpB->getKey()) {
              // If they are different variables?
              BVar = nullptr;
              break;
            }
          }
          if (BVar != nullptr) {
            KB = new CountBound(BVar->getKey());
          }
        }
      }
    } else {
      // Infer from the flow-graph.
      std::set<BoundsKey> TmpBkeys;
      // Try to predict bounds from successors.
      BI->ProgVarGraph.getSuccessors(K, TmpBkeys);
      if (!predictBounds(K, TmpBkeys, &KB)) {
        KB = nullptr;
      }
    }

    if (KB != nullptr) {
      BI->replaceBounds(K, KB);
      IsChanged = true;
    }
  }
  return IsChanged;
}

bool AVarBoundsInfo::performWorkListInference(std::set<BoundsKey> &ArrNeededBounds,
                                              bool FromPB) {
  bool RetVal = false;
  std::set<BoundsKey> WorkList;
  WorkList.insert(ArrNeededBounds.begin(), ArrNeededBounds.end());
  AvarBoundsInference BI(this);
  std::set<BoundsKey> NextIterArrs;
  bool Changed = true;
  while (Changed) {
    Changed = false;
    NextIterArrs.clear();
    // Are there any ARR atoms that need bounds?
    while (!WorkList.empty()) {
      BoundsKey CurrArrKey = *WorkList.begin();
      // Remove the bounds key from the worklist.
      WorkList.erase(CurrArrKey);
      // Can we find bounds for this Arr?
      if (BI.inferBounds(CurrArrKey, FromPB)) {
        // Record the stats.
        BoundsInferStats.DataflowMatch.insert(CurrArrKey);
        // We found the bounds.
        ArrNeededBounds.erase(CurrArrKey);
        RetVal = true;
        Changed = true;
        // Get all the successors of the ARR whose bounds we just found.
        ProgVarGraph.getSuccessors(CurrArrKey, NextIterArrs);
      }
    }
    if (Changed) {
      findIntersection(ArrNeededBounds, NextIterArrs, WorkList);
    }
  }
  return RetVal;
}
bool AVarBoundsInfo::performFlowAnalysis(ProgramInfo *PI) {
  bool RetVal = false;
  auto &CS = PI->getConstraints();
  // First get all the pointer vars which are ARRs
  std::set<BoundsKey> ArrPointers;
  for (auto Bkey : PointerBoundsKey) {
    if (DeclVarMap.right().find(Bkey) != DeclVarMap.right().end()) {
      auto &PSL = DeclVarMap.right().at(Bkey);
      if (hasArray(PI->getVarMap()[PSL], CS)) {
        ArrPointers.insert(Bkey);
      }
      // Does this array belongs to a valid program variable?
      if (isInSrcArray(PI->getVarMap()[PSL], CS)) {
        InProgramArrPtrBoundsKeys.insert(Bkey);
      }
      continue;
    }
    if (ParamDeclVarMap.right().find(Bkey) != ParamDeclVarMap.right().end()) {
      auto &ParmTup = ParamDeclVarMap.right().at(Bkey);
      std::string FuncName = std::get<0>(ParmTup);
      std::string FileName = std::get<1>(ParmTup);
      bool IsStatic = std::get<2>(ParmTup);
      unsigned ParmNum = std::get<3>(ParmTup);
      FVConstraint *FV = nullptr;
      if (IsStatic || !PI->getExtFuncDefnConstraintSet(FuncName)) {
        FV = getOnly(*(PI->getStaticFuncConstraintSet(FuncName, FileName)));
      } else {
        FV = getOnly(*(PI->getExtFuncDefnConstraintSet(FuncName)));
      }

      if (hasArray(FV->getParamVar(ParmNum), CS)) {
        ArrPointers.insert(Bkey);
      }
      // Does this array belongs to a valid program variable?
      if (isInSrcArray(FV->getParamVar(ParmNum), CS)) {
        InProgramArrPtrBoundsKeys.insert(Bkey);
      }

      continue;
    }
  }

  // Repopulate array bounds key.
  ArrPointerBoundsKey.clear();
  ArrPointerBoundsKey.insert(ArrPointers.begin(), ArrPointers.end());


  // Next, get the ARR pointers that has bounds.

  // These are pointers with bounds.
  std::set<BoundsKey> ArrWithBounds;
  for (auto &T : BInfo) {
    ArrWithBounds.insert(T.first);
  }
  // Also add arrays with invalid bounds.
  ArrWithBounds.insert(InvalidBounds.begin(), InvalidBounds.end());

  // This are the array atoms that need bounds.
  // i.e., ArrNeededBounds = ArrPtrs - ArrPtrsWithBounds.
  std::set<BoundsKey> ArrNeededBounds;
  std::set_difference(ArrPointers.begin(), ArrPointers.end(),
                      ArrWithBounds.begin(), ArrWithBounds.end(),
                      std::inserter(ArrNeededBounds, ArrNeededBounds.end()));

  bool Changed = !ArrNeededBounds.empty();

  // Now compute the bounds information of all the ARR pointers that need it.
  while (Changed) {
    Changed = false;
    // Use flow-graph.
    Changed = performWorkListInference(ArrNeededBounds) || Changed;
    // Use potential length variables.
    Changed = performWorkListInference(ArrNeededBounds, true) || Changed;
  }

  return RetVal;
}

void AVarBoundsInfo::dumpAVarGraph(const std::string &DFPath) {
  std::error_code Err;
  llvm::raw_fd_ostream DotFile(DFPath, Err);
  llvm::WriteGraph(DotFile, ProgVarGraph);
  DotFile.close();
}

void AVarBoundsInfo::print_stats(llvm::raw_ostream &O,
                                 const CVarSet &SrcCVarSet,
                                 bool JsonFormat) const {
  std::set<BoundsKey> InSrcBKeys, InSrcArrBKeys, Tmp;
  for (auto *C : SrcCVarSet) {
    if (C->isForValidDecl() && C->hasBoundsKey())
      InSrcBKeys.insert(C->getBoundsKey());
  }
  findIntersection(InProgramArrPtrBoundsKeys, InSrcBKeys, InSrcArrBKeys);
  if (!JsonFormat) {
    findIntersection(ArrPointerBoundsKey, InSrcArrBKeys, Tmp);
    O << "NumPointersNeedBounds:" << Tmp.size() << ",\n";
    O << "Details:\n";
    findIntersection(InvalidBounds, InSrcArrBKeys, Tmp);
    O << "Invalid:" << Tmp.size() << "\n,BoundsFound:\n";
    BoundsInferStats.print(O, &InSrcArrBKeys, JsonFormat);
  } else {
    findIntersection(ArrPointerBoundsKey, InSrcArrBKeys, Tmp);
    O << "{\"NumPointersNeedBounds\":" << Tmp.size() << ",";
    O << "\"Details\":{";
    findIntersection(InvalidBounds, InSrcArrBKeys, Tmp);
    O << "\"Invalid\":" << Tmp.size() << ",\"BoundsFound\":{";
    BoundsInferStats.print(O, &InSrcArrBKeys, JsonFormat);
    O << "}";
    O << "}";
    O << "}";
  }
}