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
#include "clang/3C/ProgramInfo.h"
#include "clang/3C/ConstraintResolver.h"

std::vector<BoundsPriority>
    AVarBoundsInfo::PrioList {Declared, Allocator, FlowInferred, Heuristics};

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
  if (isa<ParmVarDecl>(D) || isa<FunctionDecl>(D)) {
    // All parameters and return values are valid bound variables.
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
    removeBounds(BK);
    BInfo[BK][Declared] = B;
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
    if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
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

bool AVarBoundsInfo::mergeBounds(BoundsKey L, BoundsPriority P, ABounds *B) {
  bool RetVal = false;
  if (BInfo.find(L) != BInfo.end() && BInfo[L].find(P) != BInfo[L].end()) {
    // If previous computed bounds are not same? Then release the old bounds.
    if (!BInfo[L][P]->areSame(B)) {
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
    auto *PVar = ProgramVar::createNewProgramVar(NK, VD->getNameAsString(), PVS);
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
  if (ParamDeclVarMap.left().find(ParamKey) == ParamDeclVarMap.left().end()) {
    BoundsKey NK = ++BCount;
    const FunctionParamScope *FPS =
          FunctionParamScope::getFunctionParamScope(FD->getNameAsString(),
                                                  FD->isStatic());
    std::string ParamName = PVD->getNameAsString();
    // If this is a parameter without name!?
    // Just get the name from argument number.
    if (ParamName.empty())
      ParamName = "NONAMEPARAM_" + std::to_string(ParamIdx);

    auto *PVar = ProgramVar::createNewProgramVar(NK, ParamName, FPS);
    insertProgramVar(NK, PVar);
    insertParamKey(ParamKey, NK);
    if (PVD->getType()->isPointerType())
      PointerBoundsKey.insert(NK);
  }
  return ParamDeclVarMap.left().at(ParamKey);
}

BoundsKey AVarBoundsInfo::getVariable(clang::FunctionDecl *FD) {
  assert(isValidBoundVariable(FD) && "Not a valid bound declaration.");
  auto Psl = PersistentSourceLoc::mkPSL(FD, FD->getASTContext());
  std::string FileName = Psl.getFileName();
  auto FuncKey = std::make_tuple(FD->getNameAsString(), FileName,
                                 FD->isStatic());
  if (FuncDeclVarMap.left().find(FuncKey) == FuncDeclVarMap.left().end()) {
    BoundsKey NK = ++BCount;
    const FunctionParamScope *FPS =
          FunctionParamScope::getFunctionParamScope(FD->getNameAsString(),
                                                  FD->isStatic());

    auto *PVar = ProgramVar::createNewProgramVar(NK, FD->getNameAsString(), FPS);
    insertProgramVar(NK, PVar);
    FuncDeclVarMap.insert(FuncKey, NK);
    if (FD->getReturnType()->isPointerType())
      PointerBoundsKey.insert(NK);
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

bool AVarBoundsInfo::handleAssignment(clang::Expr *L, const CVarSet &LCVars,
                                      clang::Expr *R, const CVarSet &RCVars,
                                      ASTContext *C, ConstraintResolver *CR) {
  BoundsKey LKey, RKey;
  if ((CR->resolveBoundsKey(LCVars, LKey) ||
      tryGetVariable(L, *C, LKey)) &&
      (CR->resolveBoundsKey(RCVars, RKey) ||
       tryGetVariable(R, *C, RKey))) {
    return addAssignment(LKey, RKey);
  }
  return false;
}

bool AVarBoundsInfo::handleAssignment(clang::Decl *L, CVarOption LCVars,
                                      clang::Expr *R, const CVarSet &RCVars,
                                      ASTContext *C, ConstraintResolver *CR) {
  BoundsKey LKey, RKey;
  if ((CR->resolveBoundsKey(LCVars, LKey) ||
      tryGetVariable(L, LKey)) &&
      (CR->resolveBoundsKey(RCVars, RKey) ||
          tryGetVariable(R, *C, RKey))) {
    return addAssignment(LKey, RKey);
  }
  return false;
}

bool AVarBoundsInfo::handleContextSensitiveAssignment(CallExpr *CE,
                                                      clang::Decl *L,
                                                      ConstraintVariable *LCVar,
                                                      clang::Expr *R,
                                                      CVarSet &RCVars,
                                                      ASTContext *C,
                                                      ConstraintResolver *CR) {
  // If these are pointer variable then directly get the context-sensitive
  // bounds key.
  if(CR->containsValidCons({LCVar}) && CR->containsValidCons(RCVars)) {
    for (auto *RT : RCVars) {
      if (LCVar->hasBoundsKey() && RT->hasBoundsKey()) {
        BoundsKey NewL = getContextSensitiveBoundsKey(CE, LCVar->getBoundsKey());
        BoundsKey NewR = getContextSensitiveBoundsKey(CE, RT->getBoundsKey());
        addAssignment(NewL, NewR);
      }
    }
  } else {
    // This is the assignment of regular variables.
    BoundsKey LKey, RKey;
    if ((CR->resolveBoundsKey(*LCVar, LKey) ||
        tryGetVariable(L, LKey)) &&
        (CR->resolveBoundsKey(RCVars, RKey) ||
            tryGetVariable(R, *C, RKey))) {
      BoundsKey NewL = getContextSensitiveBoundsKey(CE, LKey);
      BoundsKey NewR = getContextSensitiveBoundsKey(CE, RKey);
      addAssignment(NewL, NewR);
    }
  }
  return true;
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
    if (L != R)
      ProgVarGraph.addEdge(R, L);
  } else {
    ProgVarGraph.addEdge(L, R);
    ProgVarGraph.addEdge(R, L);
  }
  return true;
}

// Visitor to collect all the variables that are used during the life-time
// of the visitor.
// This class also has a flag that gets set when a variable is observed
// more than once.
class CollectDeclsVisitor : public RecursiveASTVisitor<CollectDeclsVisitor> {
public:

  std::set<VarDecl*> ObservedDecls;
  std::set<std::string> StructAccess;

  explicit CollectDeclsVisitor(ASTContext *Ctx) : C(Ctx) {
    ObservedDecls.clear();
    StructAccess.clear();
  }
  virtual ~CollectDeclsVisitor() {
    ObservedDecls.clear();
  }

  bool VisitDeclRefExpr(DeclRefExpr *DRE) {
    VarDecl *VD = dyn_cast_or_null<VarDecl>(DRE->getDecl());
    if (VD != nullptr) {
      ObservedDecls.insert(VD);
    }
    return true;
  }

  // For a->b; We need to get `a->b`
  bool VisitMemberExpr(MemberExpr *ME) {
    std::string MAccess = getSourceText(ME->getSourceRange(), *C);
    if (!MAccess.empty()) {
      StructAccess.insert(MAccess);
    }
    return false;
  }

private:
  ASTContext *C;
};

bool
AVarBoundsInfo::handlePointerAssignment(clang::Stmt *St, clang::Expr *L,
                                        clang::Expr *R,
                                        ASTContext *C,
                                        ConstraintResolver *CR) {
  CollectDeclsVisitor LVarVis(C);
  LVarVis.TraverseStmt(L->getExprStmt());

  CollectDeclsVisitor RVarVis(C);
  RVarVis.TraverseStmt(R->getExprStmt());

  std::set<VarDecl *> CommonVars;
  std::set<std::string> CommonStVars;
  findIntersection(LVarVis.ObservedDecls, RVarVis.ObservedDecls, CommonVars);
  findIntersection(LVarVis.StructAccess, RVarVis.StructAccess, CommonStVars);

  if (!CommonVars.empty() || CommonStVars.empty()) {
    for (auto *LHSCVar : CR->getExprConstraintVars(L)) {
      if (LHSCVar->hasBoundsKey())
        ArrPointerBoundsKey.insert(LHSCVar->getBoundsKey());
    }
  }
  return true;
}

void
AVarBoundsInfo::recordArithmeticOperation(clang::Expr *E,
                                          ConstraintResolver *CR) {
  CVarSet CSet = CR->getExprConstraintVars(E);
  for (auto *CV : CSet) {
    if (CV->hasBoundsKey())
      ArrPointersWithArithmetic.insert(CV->getBoundsKey());
  }
}

bool AVarBoundsInfo::hasPointerArithmetic(BoundsKey BK) {
  return ArrPointersWithArithmetic.find(BK) != ArrPointersWithArithmetic.end();
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
  return DeclVarMap.left().find(PSL) != DeclVarMap.left().end();
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
    ProgramVar *NPV =
              ProgramVar::createNewProgramVar(NK,
                                              ConsString,
                                              GlobalScope::getGlobalScope(),
                                        true);
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
  PVarInfo[NK] = PV;
}

bool hasArray(ConstraintVariable *CK, Constraints &CS) {
  auto &E = CS.getVariables();
  if (PVConstraint *PV = dyn_cast<PVConstraint>(CK)) {
    if ((PV->hasArr(E, 0) || PV->hasNtArr(E, 0)) &&
        PV->isTopCvarUnsizedArr()) {
      return true;
    }
  }
  return false;
}

bool isInSrcArray(ConstraintVariable *CK, Constraints &CS) {
  auto &E = CS.getVariables();
  if (PVConstraint *PV = dyn_cast<PVConstraint>(CK)) {
    if ((PV->hasArr(E, 0) || PV->hasNtArr(E, 0)) &&
        PV->isTopCvarUnsizedArr() && PV->isForValidDecl()) {
      return true;
    }
  }
  return false;
}

// This class picks variables that are in the same scope as the provided scope.
class ScopeVisitor {
public:
  ScopeVisitor(const ProgramVarScope *S, std::set<ProgramVar *> &R,
               std::map<BoundsKey, ProgramVar *> &VarM,
               std::set<BoundsKey> &P): TS(S), Res(R), VM(VarM)
               , PtrAtoms(P) { }
  void visitBoundsKey(BoundsKey V) const {
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
  const ProgramVarScope *TS;
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

void
AvarBoundsInference::
    mergeReachableProgramVars(std::set<ProgramVar *> &AllVars) {
  if (AllVars.size() > 1) {
    ProgramVar *BVar = nullptr;
    // We want to merge all bounds vars. We give preference to
    // non-constants if there are multiple non-constant variables,
    // we give up.
    for (auto *TmpB : AllVars) {
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
      } else if (!TmpB->IsNumConstant() && BVar->getKey() != TmpB->getKey()) {
        // If they are different variables?
        BVar = nullptr;
        break;
      }
    }
    AllVars.clear();
    if (BVar)
      AllVars.insert(BVar);
  }
}

bool AvarBoundsInference::inferPossibleBounds(BoundsKey K, ABounds *SB,
                                              AVarGraph &BKGraph,
                                              std::set<ABounds *> &EB) {
  bool RetVal = false;
  if (SB != nullptr) {
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
      }
      // Find all the in scope variables reachable from the current
      // bounds variable.
      ScopeVisitor TV(Kvar->getScope(), PotentialB, BI->PVarInfo,
                      BI->PointerBoundsKey);
      BKGraph.visitBreadthFirst(SBKey, [&TV](BoundsKey BK) {
        TV.visitBoundsKey(BK);
      });

      if (*Kvar->getScope() == *SBVar->getScope()) {
        PotentialB.insert(SBVar);
      }

      mergeReachableProgramVars(PotentialB);

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
  // Try to get the bounds of all RBKeys.
  bool ValidB = true;
  for (auto PrevBKey : RBKeys) {
    // If this pointer is used in pointer arithmetic then there
    // are no relevant bounds for this pointer.
    if (BI->hasPointerArithmetic(PrevBKey)) {
      continue;
    }
    auto *PrevBounds = BI->getBounds(PrevBKey);
    // Does the parent arr has bounds?
    if (PrevBounds != nullptr)
      ResBounds.insert(PrevBounds);
  }
  return ValidB;
}

bool AvarBoundsInference::predictBounds(BoundsKey K,
                                        std::set<BoundsKey> &Neighbours,
                                        AVarGraph &BKGraph,
                                        ABounds **KB) {
  std::set<ABounds *> ResBounds;
  std::set<ABounds *> KBounds;
  *KB = nullptr;
  bool IsValid = false;
  // Get all the relevant bounds from the neighbour ARRs
  if (getRelevantBounds(Neighbours, ResBounds)) {
    bool IsFuncRet = BI->isFunctionReturn(K);
    // For function returns, we want all the predecessors to have bounds.
    if (IsFuncRet && ResBounds.size() != Neighbours.size()) {
      return IsValid;
    }
    if (!ResBounds.empty()) {
      IsValid = true;
      // Find the intersection?
      for (auto *B : ResBounds) {
        //TODO: check this
        // This is stricter version i.e., there should be at least one common
        // bounds information from an incoming ARR.
        // Should we follow same for all the pointers?

        // For function returns we should have atleast one common bound
        // from all the return values.
        if (!inferPossibleBounds(K, B, BKGraph, KBounds) && IsFuncRet) {
          IsValid = false;
          break;
        }
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
bool AvarBoundsInference::inferBounds(BoundsKey K, AVarGraph &BKGraph, bool FromPB) {
  bool IsChanged = false;

  if (BI->InvalidBounds.find(K) == BI->InvalidBounds.end()) {
    ABounds *KB = nullptr;
    // Infer from potential bounds?
    if (FromPB) {
      auto &PotBDs = BI->PotentialCntBounds;
      if (PotBDs.find(K) != PotBDs.end()) {
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
            BKGraph.visitBreadthFirst(TK, [&TV](BoundsKey BK) {
              TV.visitBoundsKey(BK);
            });
          } else {
            PotentialB.insert(TKVar);
          }
        }
        ProgramVar *BVar = nullptr;
        mergeReachableProgramVars(PotentialB);
        if (!PotentialB.empty())
          BVar = *(PotentialB.begin());
        if (BVar != nullptr)
          KB = new CountBound(BVar->getKey());
      }
    } else {
      // Infer from the flow-graph.
      std::set<BoundsKey> TmpBkeys;
      // Try to predict bounds from predecessors.
      BKGraph.getPredecessors(K, TmpBkeys);
      if (!predictBounds(K, TmpBkeys, BKGraph, &KB)) {
        KB = nullptr;
      }
    }

    if (KB != nullptr) {
      BI->replaceBounds(K, FlowInferred, KB);
      IsChanged = true;
    }
  }
  return IsChanged;
}

bool AVarBoundsInfo::performWorkListInference(std::set<BoundsKey> &ArrNeededBounds,
                                              AVarGraph &BKGraph,
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
      if (BI.inferBounds(CurrArrKey, BKGraph, FromPB)) {
        // Record the stats.
        BoundsInferStats.DataflowMatch.insert(CurrArrKey);
        // We found the bounds.
        ArrNeededBounds.erase(CurrArrKey);
        RetVal = true;
        Changed = true;
        // Get all the successors of the ARR whose bounds we just found.
        BKGraph.getSuccessors(CurrArrKey, NextIterArrs);
      }
    }
    if (Changed) {
      findIntersection(ArrNeededBounds, NextIterArrs, WorkList);
    }
  }
  return RetVal;
}

void
AVarBoundsInfo::insertCtxSensBoundsKey(ProgramVar *OldPV,
                                       BoundsKey NK,
                                       const CtxFunctionArgScope *CFAS) {
  ProgramVar *NKVar = OldPV->makeCopy(NK);
  NKVar->setScope(CFAS);
  insertProgramVar(NK, NKVar);
  ProgVarGraph.addEdge(OldPV->getKey(), NKVar->getKey());
  CtxSensProgVarGraph.addEdge(NKVar->getKey(), OldPV->getKey());
}

// Here, we create a new BoundsKey for every BoundsKey var that is related to
// any ConstraintVariable in CSet and store the information by the
// corresponding call expression (CE).
bool
AVarBoundsInfo::contextualizeCVar(CallExpr *CE, const CVarSet &CSet,
                                  ASTContext *C) {
  for (auto *CV : CSet) {
    // If this is a FV Constraint the contextualize its returns and
    // parameters.
    if (FVConstraint *FV = dyn_cast_or_null<FVConstraint>(CV)) {
      contextualizeCVar(CE, {FV->getReturnVar()}, C);
      for (unsigned i = 0; i < FV->numParams(); i++) {
        contextualizeCVar(CE, {FV->getParamVar(i)}, C);
      }
    }

    if (PVConstraint *PV = dyn_cast_or_null<PVConstraint>(CV)) {
      if (PV->hasBoundsKey()) {
        // First duplicate the bounds key.
        BoundsKey CK = PV->getBoundsKey();
        PersistentSourceLoc CEPSL = PersistentSourceLoc::mkPSL(CE, *C);
        ProgramVar *CKVar = getProgramVar(CK);

        // Create a context sensitive scope.
        const CtxFunctionArgScope *CFAS = nullptr;
        if (auto *FPS =
          dyn_cast_or_null<FunctionParamScope>(CKVar->getScope())) {
          CFAS = CtxFunctionArgScope::getCtxFunctionParamScope(FPS, CEPSL);
        }

        auto &BKeyMap = CSBoundsKey[CE];
        if (BKeyMap.find(CK) == BKeyMap.end()) {
          BoundsKey NK = ++BCount;
          insertCtxSensBoundsKey(CKVar, NK, CFAS);
          BKeyMap[CK] = NK;
          // Next duplicate the Bounds information.
          BoundsPriority TP = Invalid;
          ABounds *CKBounds = getBounds(CK, Invalid, &TP);
          if (CKBounds != nullptr) {
            BoundsKey NBK = CKBounds->getBKey();
            ProgramVar *NBKVar = getProgramVar(CK);
            if (BKeyMap.find(NBK) == BKeyMap.end()) {
              BoundsKey TmpBK = ++BCount;
              BKeyMap[NBK] = TmpBK;
              insertCtxSensBoundsKey(NBKVar, TmpBK, CFAS);
            }
            CKBounds = CKBounds->makeCopy(BKeyMap[NBK]);
            replaceBounds(NK, TP, CKBounds);
          }
        }
      }
    }
  }
  return true;
}

void AVarBoundsInfo::resetContextSensitiveBoundsKey() {
  CSBoundsKey.clear();
}

BoundsKey AVarBoundsInfo::getContextSensitiveBoundsKey(CallExpr *CE,
                                                       BoundsKey BK) {
  if (CSBoundsKey.find(CE) != CSBoundsKey.end()) {
    auto &TmpMap = CSBoundsKey[CE];
    if (TmpMap.find(BK) != TmpMap.end()) {
      return TmpMap[BK];
    }
  }
  return BK;
}

void AVarBoundsInfo::computerArrPointers(ProgramInfo *PI,
                                         std::set<BoundsKey> &ArrPointers) {
  auto &CS = PI->getConstraints();
  for (auto Bkey : PointerBoundsKey) {
    // Regular variables.
    auto &BkeyToPSL = DeclVarMap.right();
    if (BkeyToPSL.find(Bkey) != BkeyToPSL.end()) {
      auto &PSL = BkeyToPSL.at(Bkey);
      if (hasArray(PI->getVarMap().at(PSL), CS)) {
        ArrPointers.insert(Bkey);
      }
      // Does this array belongs to a valid program variable?
      if (isInSrcArray(PI->getVarMap().at(PSL), CS)) {
        InProgramArrPtrBoundsKeys.insert(Bkey);
      }
      continue;
    }

    // Function parameters
    auto &ParmBkeyToPSL = ParamDeclVarMap.right();
    if (ParmBkeyToPSL.find(Bkey) != ParmBkeyToPSL.end()) {
      auto &ParmTup = ParmBkeyToPSL.at(Bkey);
      std::string FuncName = std::get<0>(ParmTup);
      std::string FileName = std::get<1>(ParmTup);
      bool IsStatic = std::get<2>(ParmTup);
      unsigned ParmNum = std::get<3>(ParmTup);
      FVConstraint *FV = nullptr;
      if (IsStatic || !PI->getExtFuncDefnConstraint(FuncName)) {
        FV = PI->getStaticFuncConstraint(FuncName, FileName);
      } else {
        FV = PI->getExtFuncDefnConstraint(FuncName);
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
    // Function returns.
    auto &FuncKeyToPSL = FuncDeclVarMap.right();
    if (FuncKeyToPSL.find(Bkey) != FuncKeyToPSL.end()) {
      auto &FuncRet = FuncKeyToPSL.at(Bkey);
      std::string FuncName = std::get<0>(FuncRet);
      std::string FileName = std::get<1>(FuncRet);
      bool IsStatic = std::get<2>(FuncRet);
      const FVConstraint *FV = nullptr;
      std::set<FVConstraint *> Tmp;
      Tmp.clear();
      if (IsStatic || !PI->getExtFuncDefnConstraint(FuncName)) {
        Tmp.insert(PI->getStaticFuncConstraint(FuncName, FileName));
        FV = getOnly(Tmp);
      } else {
        Tmp.insert(PI->getExtFuncDefnConstraint(FuncName));
        FV = getOnly(Tmp);
      }

      if (hasArray(FV->getReturnVar(), CS)) {
        ArrPointers.insert(Bkey);
      }
      // Does this array belongs to a valid program variable?
      if (isInSrcArray(FV->getReturnVar(), CS)) {
        InProgramArrPtrBoundsKeys.insert(Bkey);
      }
      continue;
    }
  }

  // Get all context-sensitive BoundsKey for each of the actual BKs
  // and consider them to be array pointers as well.
  // Since context-sensitive BoundsKey will be immediate children
  // of the regular bounds key, we just get the neighbours (predecessors
  // and successors) of the regular bounds key to get the context-sensitive
  // counterparts.
  std::set<BoundsKey> CtxSensBKeys;
  CtxSensBKeys.clear();
  std::set<BoundsKey> TmpBKeys, TmpBKeysF;
  for (auto BK : ArrPointers) {
    TmpBKeys.clear();
    ProgVarGraph.getPredecessors(BK, TmpBKeys);
    TmpBKeysF.insert(TmpBKeys.begin(), TmpBKeys.end());
    TmpBKeys.clear();
    ProgVarGraph.getSuccessors(BK, TmpBKeys);
    TmpBKeysF.insert(TmpBKeys.begin(), TmpBKeys.end());
    for (auto TBK : TmpBKeysF) {
      ProgramVar *TmpPVar = getProgramVar(TBK);
      if (TmpPVar != nullptr) {
        if (isa<CtxFunctionArgScope>(TmpPVar->getScope())) {
          CtxSensBKeys.insert(TBK);
        }
      }
    }
  }

  ArrPointers.insert(CtxSensBKeys.begin(), CtxSensBKeys.end());
}

bool AVarBoundsInfo::performFlowAnalysis(ProgramInfo *PI) {
  bool RetVal = false;
  // First get all the pointer vars which are ARRs
  std::set<BoundsKey> ArrPointers;
  computerArrPointers(PI, ArrPointers);

  // Repopulate array bounds key.
  ArrPointerBoundsKey.clear();
  ArrPointerBoundsKey.insert(ArrPointers.begin(), ArrPointers.end());

  // Keep only highest priority bounds.
  // Any thing changed? which means bounds of a variable changed
  // Which means we need to recompute the flow based bounds for
  // all arrays that have flow based bounds.
  if (keepHighestPriorityBounds(ArrPointerBoundsKey)) {
    // Remove flow inferred bounds, if exist for all the array pointers.
    for (auto TBK : ArrPointerBoundsKey)
      removeBounds(TBK, FlowInferred);
  }

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
    Changed = performWorkListInference(ArrNeededBounds, this->ProgVarGraph) || Changed;
    // Use potential length variables.
    Changed = performWorkListInference(ArrNeededBounds, this->ProgVarGraph, true) || Changed;
    // Try solving using context-sensitive BoundsKey.
    Changed = performWorkListInference(ArrNeededBounds, this->CtxSensProgVarGraph) || Changed;
    Changed = performWorkListInference(ArrNeededBounds, this->CtxSensProgVarGraph, true) || Changed;
  }

  return RetVal;
}

bool AVarBoundsInfo::keepHighestPriorityBounds(std::set<BoundsKey> &ArrPtrs) {
  bool FoundBounds = false;
  bool HasChanged = false;
  for (auto BK : ArrPtrs) {
    FoundBounds = false;
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

void AVarBoundsInfo::dumpAVarGraph(const std::string &DFPath) {
  std::error_code Err;
  llvm::raw_fd_ostream DotFile(DFPath, Err);
  llvm::WriteGraph(DotFile, ProgVarGraph);
  DotFile.close();
}

bool AVarBoundsInfo::isFunctionReturn(BoundsKey BK) {
  return (FuncDeclVarMap.right().find(BK) != FuncDeclVarMap.right().end());
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

ContextSensitiveBoundsKeyVisitor::ContextSensitiveBoundsKeyVisitor(ASTContext *C,
                                                                   ProgramInfo &I,
                                                  ConstraintResolver *CResolver)
: Context(C), Info(I), CR(CResolver) {
  Info.getABoundsInfo().resetContextSensitiveBoundsKey();
}

ContextSensitiveBoundsKeyVisitor::~ContextSensitiveBoundsKeyVisitor() {
  // Reset the context sensitive bounds.
  // This is to ensure that we store pointers to the AST objects
  // when we are with in the corresponding compilation unit.
  Info.getABoundsInfo().resetContextSensitiveBoundsKey();
}

bool ContextSensitiveBoundsKeyVisitor::VisitCallExpr(CallExpr *CE) {
  if (FunctionDecl *FD = dyn_cast_or_null<FunctionDecl>(CE->getCalleeDecl())) {
    // Contextualize the function at this call-site.
    CVarOption COpt = Info.getVariable(FD, Context);
    if (COpt.hasValue())
      Info.getABoundsInfo().contextualizeCVar(CE, {&COpt.getValue()}, Context);
    
  }
  return true;
}