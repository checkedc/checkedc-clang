//=--CtxSensAVarBounds.cpp----------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the implementation of various methods in
// CtxSensAVarBounds.h
//
//===----------------------------------------------------------------------===//

#include <sstream>
#include "clang/3C/AVarBoundsInfo.h"
#include "clang/3C/CtxSensAVarBounds.h"
#include "clang/3C/ConstraintResolver.h"
#include "clang/3C/ProgramInfo.h"


// This visitor computes a string representation of a structure member access
// which can be used as key for context sensitive access.
// For example: for this: arr[i]->st1->st, we will get "arr","st1", "st".
// We will ignore array indexing.
class StructAccessVisitor : public RecursiveASTVisitor<StructAccessVisitor> {
public:

  std::vector<std::string> StructAccessStr;
  bool IsGlobal = false;

  explicit StructAccessVisitor(ASTContext *Ctx) : C(Ctx) {
    StructAccessStr.clear();
  }
  virtual ~StructAccessVisitor() {
    StructAccessStr.clear();
  }

  void processVarDecl(VarDecl *VD) {
    if (VD != nullptr && (VD->getType()->isPointerType() ||
                          VD->getType()->isStructureType())) {
      IsGlobal = !VD->isLocalVarDecl();
      StructAccessStr.insert(StructAccessStr.begin(), VD->getNameAsString());
    }
  }

  bool VisitDeclRefExpr(DeclRefExpr *DRE) {
    ParmVarDecl *PD = dyn_cast_or_null<ParmVarDecl>(DRE->getDecl());
    if (PD != nullptr && (PD->getType()->isPointerType() ||
                          PD->getType()->isStructureType())) {
      StructAccessStr.insert(StructAccessStr.begin(), PD->getNameAsString());
    } else {
      VarDecl *VD = dyn_cast_or_null<VarDecl>(DRE->getDecl());
      processVarDecl(VD);
    }
    return true;
  }

  bool VisitMemberExpr(MemberExpr *ME) {
    std::string MAccess = getSourceText(ME->getMemberDecl()->getSourceRange(), *C);
    StructAccessStr.insert(StructAccessStr.begin(), MAccess);
    return true;
  }

  // This gives us a string serves as a key for a struct member access.
  std::string getStructAccessKey() {
    std::string Ret = "";
    for (auto CurrStr : StructAccessStr) {
      Ret = CurrStr + ":" + Ret;
    }
    return Ret;
  }

private:
  ASTContext *C;
};

void
CtxSensitiveBoundsKeyHandler::
insertCtxSensBoundsKey(ProgramVar *OldPV,
                       BoundsKey NK,
                       const ProgramVarScope *NPS) {
  ProgramVar *NKVar = OldPV->makeCopy(NK);
  NKVar->setScope(NPS);
  ABI->insertProgramVar(NK, NKVar);
  ABI->RevCtxSensProgVarGraph.addUniqueEdge(OldPV->getKey(), NKVar->getKey());
  ABI->CtxSensProgVarGraph.addUniqueEdge(NKVar->getKey(), OldPV->getKey());
}

void
CtxSensitiveBoundsKeyHandler::
createCtxSensBoundsKey(BoundsKey OK,
                       const ProgramVarScope *NPS,
                       std::map<BoundsKey, BoundsKey> &CBMap) {
  ProgramVar *CKVar = ABI->getProgramVar(OK);
  if (CBMap.find(OK) == CBMap.end()) {
    BoundsKey NK = ++(ABI->BCount);
    insertCtxSensBoundsKey(CKVar, NK, NPS);
    CBMap[OK] = NK;
    // Next duplicate the Bounds information.
    BoundsPriority TP = Invalid;
    ABounds *CKBounds = ABI->getBounds(OK, Invalid, &TP);
    if (CKBounds != nullptr) {
      BoundsKey NBK = CKBounds->getBKey();
      if (CBMap.find(NBK) == CBMap.end()) {
        BoundsKey TmpBK = ++(ABI->BCount);
        CBMap[NBK] = TmpBK;
        insertCtxSensBoundsKey(CKVar, TmpBK, NPS);
      }
      CKBounds = CKBounds->makeCopy(CBMap[NBK]);
      ABI->replaceBounds(NK, TP, CKBounds);
    }
  }
}

// Here, we create a new BoundsKey for every BoundsKey var that is related to
// any ConstraintVariable in CSet and store the information by the
// corresponding call expression (CE).
void
CtxSensitiveBoundsKeyHandler::contextualizeCVar(CallExpr *CE,
                                                const CVarSet &CSet,
                                                ASTContext *C) {
  for (auto *CV : CSet) {
    // If this is a FV Constraint the contextualize its returns and
    // parameters.
    if (FVConstraint *FV = dyn_cast_or_null<FVConstraint>(CV)) {
      contextualizeCVar(CE, {FV->getExternalReturn()}, C);
      for (unsigned i = 0; i < FV->numParams(); i++) {
        contextualizeCVar(CE, {FV->getExternalParam(i)}, C);
      }
    }

    if (PVConstraint *PV = dyn_cast_or_null<PVConstraint>(CV)) {
      if (PV->hasBoundsKey()) {
        // First duplicate the bounds key.
        BoundsKey CK = PV->getBoundsKey();
        PersistentSourceLoc CEPSL = PersistentSourceLoc::mkPSL(CE, *C);
        ProgramVar *CKVar = ABI->getProgramVar(CK);

        // Create a context sensitive scope.
        const CtxFunctionArgScope *CFAS = nullptr;
        if (auto *FPS =
          dyn_cast_or_null<FunctionParamScope>(CKVar->getScope())) {
          CFAS = CtxFunctionArgScope::getCtxFunctionParamScope(FPS, CEPSL);
        }

        auto PSL = PersistentSourceLoc::mkPSL(CE, *C);
        auto &BKeyMap = CSBoundsKey[PSL];
        createCtxSensBoundsKey(CK, CFAS, BKeyMap);
      }
    }
  }
}

CtxStKeyMap *CtxSensitiveBoundsKeyHandler::getCtxStKeyMap(MemberExpr *ME,
                                                          ASTContext *C) {
  StructAccessVisitor SKV(C);
  SKV.TraverseStmt(ME->getBase()->getExprStmt());
  CtxStKeyMap *MECSMap = getCtxStKeyMap(SKV.IsGlobal);
  return MECSMap;
}

CtxStKeyMap *CtxSensitiveBoundsKeyHandler::getCtxStKeyMap(bool IsGlobal) {
  CtxStKeyMap *MECSMap = nullptr;
  if (IsGlobal) {
    MECSMap = &GlobalMEBoundsKey;
  } else {
    MECSMap = &LocalMEBoundsKey;
  }
  return MECSMap;
}

std::string CtxSensitiveBoundsKeyHandler::getCtxStructKey(MemberExpr *ME,
                                                          ASTContext *C) {
  StructAccessVisitor SKV(C);
  SKV.TraverseStmt(ME->getBase()->getExprStmt());
  return SKV.getStructAccessKey();
}
bool
CtxSensitiveBoundsKeyHandler::tryGetFieldCSKey(FieldDecl *FD,
                                               CtxStKeyMap *CSK,
                                               const std::string &AK,
                                               ASTContext *C,
                                               ProgramInfo &I,
                                               BoundsKey &CSKey) {
  bool RetVal = false;
  if (CSK->find(AK) != CSK->end()) {
    CVarOption CV = I.getVariable(FD, C);
    BoundsKey OrigK;
    if (CV.hasValue() && CV.getValue().hasBoundsKey()) {
      OrigK = CV.getValue().getBoundsKey();
    } else {
      OrigK = ABI->getVariable(FD);
    }
    auto &BKeyMap = (*CSK)[AK];
    if (BKeyMap.find(OrigK) != BKeyMap.end()) {
      CSKey = BKeyMap[OrigK];
      RetVal = true;
    }
  }
  return RetVal;
}

bool CtxSensitiveBoundsKeyHandler::tryGetMECSKey(MemberExpr *ME,
                                                 ASTContext *C,
                                                 ProgramInfo &I,
                                                 BoundsKey &CSKey) {
  bool RetVal = false;
  FieldDecl *FD = dyn_cast_or_null<FieldDecl>(ME->getMemberDecl());
  if (FD != nullptr) {
    // Check which map to insert?
    CtxStKeyMap *MECSMap = getCtxStKeyMap(ME, C);
    std::string AK = getCtxStructKey(ME, C);
    RetVal = tryGetFieldCSKey(FD, MECSMap, AK, C, I, CSKey);
  }
  return RetVal;
}

void
CtxSensitiveBoundsKeyHandler::contextualizeStructRecord(ProgramInfo &I,
                                                        ASTContext *C,
                                                        const RecordDecl *RD,
                                                        const std::string &AK,
                                                        std::map<BoundsKey,
                                                        BoundsKey> &BKMap,
                                                        bool IsGlobal) {
  // Create context-sensitive keys for all fields.
  for (auto *CFD : RD->fields()) {
    // There is no context-sensitive key already created for this?
    BoundsKey MEBKey = 0;
    CVarOption CV = I.getVariable(CFD, C);
    if (CV.hasValue() && CV.getValue().hasBoundsKey()) {
      MEBKey = CV.getValue().getBoundsKey();
    } else {
      MEBKey = ABI->getVariable(CFD);
    }
    ProgramVar *SPV = ABI->getProgramVar(MEBKey);

    // Create a context sensitive struct scope.
    const CtxStructScope *CSS = nullptr;
    if (auto *SS =
      dyn_cast_or_null<StructScope>(SPV->getScope())) {
      CSS = CtxStructScope::getCtxStructScope(SS, AK, IsGlobal);
    }
    createCtxSensBoundsKey(MEBKey, CSS, BKMap);
  }
}

void
CtxSensitiveBoundsKeyHandler::contextualizeCVar(MemberExpr *ME,
                                                ASTContext *C,
                                                ProgramInfo &I) {
  FieldDecl *FD = dyn_cast_or_null<FieldDecl>(ME->getMemberDecl());
  if (FD != nullptr) {
    RecordDecl *RD = FD->getParent();
    // If the base decl is not null.
    if (RD != nullptr) {
      // Get structure access key.
      StructAccessVisitor SKV(C);
      SKV.TraverseStmt(ME->getBase()->getExprStmt());
      std::string AK = SKV.getStructAccessKey();
      // Context sensitive struct key map.
      CtxStKeyMap *MECSMap = getCtxStKeyMap(SKV.IsGlobal);
      auto &BKeyMap = (*MECSMap)[AK];
      contextualizeStructRecord(I, C, RD, AK, BKeyMap, SKV.IsGlobal);
    }
  }
}

void
CtxSensitiveBoundsKeyHandler::contextualizeCVar(VarDecl *VD,
                                                ASTContext *C,
                                                ProgramInfo &I) {
  const RecordType *RT =
    dyn_cast_or_null<RecordType>(VD->getType()->getUnqualifiedDesugaredType());
  const RecordDecl *RD = nullptr;
  if (RT != nullptr) {
    RD = RT->getDecl();
  }
  // If the base decl is not null.
  if (RT != nullptr) {
    // Get structure access key.
    StructAccessVisitor SKV(C);
    SKV.processVarDecl(VD);
    // Context sensitive struct key map.
    CtxStKeyMap *MECSMap = getCtxStKeyMap(SKV.IsGlobal);
    std::string AK = SKV.getStructAccessKey();
    auto &BKeyMap = (*MECSMap)[AK];
    contextualizeStructRecord(I, C, RD, AK, BKeyMap, SKV.IsGlobal);
  }
}

BoundsKey
CtxSensitiveBoundsKeyHandler::
getCtxSensCEBoundsKey(const PersistentSourceLoc &PSL,
                      BoundsKey BK) {
  if (CSBoundsKey.find(PSL) != CSBoundsKey.end()) {
    auto &TmpMap = CSBoundsKey[PSL];
    if (TmpMap.find(BK) != TmpMap.end()) {
      return TmpMap[BK];
    }
  }
  return BK;
}

bool
CtxSensitiveBoundsKeyHandler::deriveBoundsKeys(clang::Expr *E,
                                               const CVarSet &CVars,
                                               ASTContext *C,
                                               ConstraintResolver *CR,
                                               std::set<BoundsKey> &AllKeys) {
  BoundsKey TmpK;
  bool Ret = true;
  if (CR != nullptr && CR->containsValidCons(CVars)) {
    for (auto *CV : CVars) {
      if (CV->hasBoundsKey())
        AllKeys.insert(CV->getBoundsKey());
    }
  } else if (CR != nullptr && CR->resolveBoundsKey(CVars, TmpK)) {
    AllKeys.insert(TmpK);
  } else if (C != nullptr && ABI->tryGetVariable(E, *C, TmpK)) {
    AllKeys.insert(TmpK);
  } else {
    Ret = false;
  }
  return Ret;
}

bool
CtxSensitiveBoundsKeyHandler::
handleContextSensitiveAssignment(const PersistentSourceLoc &PSL,
                                 clang::Decl *L,
                                 ConstraintVariable *LCVar,
                                 clang::Expr *R,
                                 CVarSet &RCVars,
                                 const std::set<BoundsKey> &CSRKeys,
                                 ASTContext *C,
                                 ConstraintResolver *CR) {
  bool Ret = false;
  std::set<BoundsKey> AllRBKeys, AllLBKeys, TmpBKeys;
  AllRBKeys = CSRKeys;

  // Try getting context sensitive bounds keys for L and R.
  if (AllRBKeys.empty()) {
    deriveBoundsKeys(R, RCVars, C, CR, TmpBKeys);
    for (auto CBKey : TmpBKeys) {
      AllRBKeys.insert(getCtxSensCEBoundsKey(PSL, CBKey));
    }
  }
  TmpBKeys.clear();
  deriveBoundsKeys(nullptr, {LCVar}, C, CR, TmpBKeys);
  for (auto CBKey : TmpBKeys) {
    AllLBKeys.insert(getCtxSensCEBoundsKey(PSL, CBKey));
  }

  // Add assignment between context sensitive bounds keys.
  for (auto LK : AllLBKeys) {
    for (auto RK : AllRBKeys) {
      Ret = ABI->addAssignment(LK, RK) || Ret;
    }
  }
  return Ret;
}

bool ContextSensitiveBoundsKeyVisitor::VisitCallExpr(CallExpr *CE) {
  if (FunctionDecl *FD = dyn_cast_or_null<FunctionDecl>(CE->getCalleeDecl())) {
    // Contextualize the function return value and parameters at
    // this call-site.
    CVarOption COpt = Info.getVariable(FD, Context);
    if (COpt.hasValue()) {
      auto &CSBHandler = Info.getABoundsInfo().getCtxSensBoundsHandler();
      CSBHandler.contextualizeCVar(CE, {&COpt.getValue()}, Context);
    }

  }
  return true;
}

bool ContextSensitiveBoundsKeyVisitor::VisitMemberExpr(MemberExpr *ME) {
  // Make the struct member dereference context-sensitive.
  auto &CSBHandler = Info.getABoundsInfo().getCtxSensBoundsHandler();
  CSBHandler.contextualizeCVar(ME, Context, Info);
  return true;
}

bool ContextSensitiveBoundsKeyVisitor::VisitDeclStmt(DeclStmt *DS) {
  // Make the structure variable initializations context-sensitive.
  auto &ABInfo = Info.getABoundsInfo();
  auto &CSBHandler = Info.getABoundsInfo().getCtxSensBoundsHandler();
  for (const auto &D : DS->decls()) {
    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      Expr *InitE = VD->getInit();
      if (InitE != nullptr && VD->getType()->isStructureType()) {
        InitListExpr *ILE = dyn_cast_or_null<InitListExpr>(InitE);
        if (ILE != nullptr) {
          // Create context-sensitive fields for the structure variable.
          StructAccessVisitor SAV(Context);
          SAV.processVarDecl(VD);
          CSBHandler.contextualizeCVar(VD, Context, Info);

          // Handle assignment of expressions in initialization list
          // to various fields of the structure variable.
          const RecordDecl *Definition =
            ILE->getType()->getAsStructureType()->getDecl()->getDefinition();
          auto *CSKeyMap = CSBHandler.getCtxStKeyMap(SAV.IsGlobal);
          unsigned int initIdx = 0;
          const auto fields = Definition->fields();
          for (auto it = fields.begin();
               initIdx < ILE->getNumInits() && it != fields.end();
               initIdx++, it++) {
            Expr *InitExpr = ILE->getInit(initIdx);
            BoundsKey FKey;
            // Handle assignment to context-sensitive field key.
            if (CSBHandler.tryGetFieldCSKey(*it, CSKeyMap, SAV.getStructAccessKey(),
                                            Context, Info, FKey)) {

              auto InitCVs = CR->getExprConstraintVars(InitExpr);
              ABInfo.handleAssignment(nullptr, {}, {FKey}, InitExpr,
                                      InitCVs.first, InitCVs.second, Context, CR);
            }
          }
        }
      }
    }
  }
  return true;
}