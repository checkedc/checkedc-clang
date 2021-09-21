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
#include "clang/3C/CtxSensAVarBounds.h"
#include "clang/3C/AVarBoundsInfo.h"
#include "clang/3C/ConstraintResolver.h"
#include "clang/3C/ProgramInfo.h"
#include <sstream>

// This visitor computes a string representation of a structure member access
// which can be used as key for context sensitive access.
// For example: for this: arr[i]->st1->st, we will get "arr","st1", "st".
// We will ignore array indexing.
class StructAccessVisitor : public RecursiveASTVisitor<StructAccessVisitor> {
public:
  explicit StructAccessVisitor(ASTContext *Ctx) : C(Ctx), StructAccessStr() {}
  virtual ~StructAccessVisitor() { StructAccessStr.clear(); }

  bool isGlobal() const { return IsGlobal; }

  void processVarDecl(VarDecl *VD) {
    assert("StructAccessVisitor visiting null VarDecl" && VD != nullptr);
    if (VD->getType()->isPointerType() || VD->getType()->isStructureType()) {
      // If VD is a ParmVarDecl isLocalVarDecl will return false
      // FIXME: This method can be called multiple times, and each time it will
      //        set the value of IsGlobal. This could be a problem if it is
      //        called on a global and then a local variable. e.g.:
      //        ( 0 ? global_struct_var : local_struct_var).array
      //        IsGlobal might be true or false depending on the order it
      //        encounters the variables. This would also change the order of
      //        entries in StructAccessStr.
      IsGlobal = !VD->isLocalVarDecl();
      StructAccessStr.insert(StructAccessStr.begin(), VD->getNameAsString());
    }
  }

  bool VisitDeclRefExpr(DeclRefExpr *DRE) {
    if (auto *VD = dyn_cast_or_null<VarDecl>(DRE->getDecl()))
      processVarDecl(VD);
    return true;
  }

  bool VisitMemberExpr(MemberExpr *ME) {
    std::string MAccess =
        getSourceText(ME->getMemberDecl()->getSourceRange(), *C);
    StructAccessStr.insert(StructAccessStr.begin(), MAccess);
    return true;
  }

  // This gives us a string serves as a key for a struct member access.
  std::string getStructAccessKey() const {
    std::string Ret = "";
    for (auto CurrStr : StructAccessStr) {
      Ret = CurrStr + ":" + Ret;
    }
    return Ret;
  }

private:
  ASTContext *C;
  bool IsGlobal = false;
  std::vector<std::string> StructAccessStr;
};

void CtxSensitiveBoundsKeyHandler::insertCtxSensBoundsKey(
    ProgramVar *OldPV, BoundsKey NK, const ProgramVarScope *NPS) {
  ProgramVar *NKVar = OldPV->makeCopy(NK);
  NKVar->setScope(NPS);
  ABI->insertProgramVar(NK, NKVar);
  ABI->RevCtxSensProgVarGraph.addUniqueEdge(OldPV->getKey(), NKVar->getKey());
  ABI->CtxSensProgVarGraph.addUniqueEdge(NKVar->getKey(), OldPV->getKey());
}

void CtxSensitiveBoundsKeyHandler::createCtxSensBoundsKey(
    BoundsKey OK, const ProgramVarScope *NPS,
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
void CtxSensitiveBoundsKeyHandler::contextualizeCVar(CallExpr *CE,
                                                     const CVarSet &CSet,
                                                     ASTContext *C) {
  for (auto *CV : CSet) {
    // If this is a FV Constraint then contextualize its returns and
    // parameters.
    if (FVConstraint *FV = dyn_cast_or_null<FVConstraint>(CV)) {
      contextualizeCVar(CE, {FV->getExternalReturn()}, C);
      for (unsigned I = 0; I < FV->numParams(); I++) {
        contextualizeCVar(CE, {FV->getExternalParam(I)}, C);
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
  CtxStKeyMap *MECSMap = getCtxStKeyMap(SKV.isGlobal());
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
bool CtxSensitiveBoundsKeyHandler::tryGetFieldCSKey(
    FieldDecl *FD, CtxStKeyMap *CSK, const std::string &AK, ASTContext *C,
    ProgramInfo &I, BoundsKey &CSKey) {
  bool RetVal = false;
  if (ABI->isValidBoundVariable(FD) && CSK->find(AK) != CSK->end()) {
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

bool CtxSensitiveBoundsKeyHandler::tryGetMECSKey(MemberExpr *ME, ASTContext *C,
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

void CtxSensitiveBoundsKeyHandler::contextualizeStructRecord(
    ProgramInfo &I, ASTContext *C, const RecordDecl *RD, const std::string &AK,
    std::map<BoundsKey, BoundsKey> &BKMap, bool IsGlobal) {
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
    if (auto *SS = dyn_cast_or_null<StructScope>(SPV->getScope())) {
      CSS = CtxStructScope::getCtxStructScope(SS, AK, IsGlobal);
    }
    createCtxSensBoundsKey(MEBKey, CSS, BKMap);
  }
}

void CtxSensitiveBoundsKeyHandler::contextualizeCVar(RecordDecl *RD,
                                                     std::string AccessKey,
                                                     bool IsGlobal,
                                                     ASTContext *C,
                                                     ProgramInfo &I) {
  std::string FileName = PersistentSourceLoc::mkPSL(RD, *C).getFileName();
  if (canWrite(FileName)) {
    // Context sensitive struct key map.
    CtxStKeyMap *MECSMap = getCtxStKeyMap(IsGlobal);
    auto &BKeyMap = (*MECSMap)[AccessKey];
    contextualizeStructRecord(I, C, RD, AccessKey, BKeyMap, IsGlobal);
  }
}

void CtxSensitiveBoundsKeyHandler::contextualizeCVar(MemberExpr *ME,
                                                     ASTContext *C,
                                                     ProgramInfo &I) {
  FieldDecl *FD = dyn_cast_or_null<FieldDecl>(ME->getMemberDecl());
  if (RecordDecl *RD = FD != nullptr ? FD->getParent() : nullptr) {
    // Get structure access key.
    StructAccessVisitor SKV(C);
    SKV.TraverseStmt(ME->getBase()->getExprStmt());
    contextualizeCVar(RD, SKV.getStructAccessKey(), SKV.isGlobal(), C, I);
  }
}

void CtxSensitiveBoundsKeyHandler::contextualizeCVar(VarDecl *VD, ASTContext *C,
                                                     ProgramInfo &I) {
  const auto *RT = dyn_cast_or_null<RecordType>(
    VD->getType()->getUnqualifiedDesugaredType());
  if (RecordDecl *RD = RT != nullptr ? RT->getDecl() : nullptr) {
    // Get structure access key.
    StructAccessVisitor SKV(C);
    SKV.processVarDecl(VD);
    contextualizeCVar(RD, SKV.getStructAccessKey(), SKV.isGlobal(), C, I);
  }
}

BoundsKey CtxSensitiveBoundsKeyHandler::getCtxSensCEBoundsKey(
    const PersistentSourceLoc &PSL, BoundsKey BK) {
  if (CSBoundsKey.find(PSL) != CSBoundsKey.end()) {
    auto &TmpMap = CSBoundsKey[PSL];
    if (TmpMap.find(BK) != TmpMap.end()) {
      return TmpMap[BK];
    }
  }
  return BK;
}

bool CtxSensitiveBoundsKeyHandler::deriveBoundsKeys(
    clang::Expr *E, const CVarSet &CVars, ASTContext *C, ConstraintResolver *CR,
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

bool CtxSensitiveBoundsKeyHandler::handleContextSensitiveAssignment(
    const PersistentSourceLoc &PSL, clang::Decl *L, ConstraintVariable *LCVar,
    clang::Expr *R, CVarSet &RCVars, const std::set<BoundsKey> &CSRKeys,
    ASTContext *C, ConstraintResolver *CR) {
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
          auto *CSKeyMap = CSBHandler.getCtxStKeyMap(SAV.isGlobal());
          unsigned int InitIdx = 0;
          const auto Fields = Definition->fields();
          for (auto It = Fields.begin();
               InitIdx < ILE->getNumInits() && It != Fields.end();
               InitIdx++, It++) {
            Expr *InitExpr = ILE->getInit(InitIdx);
            BoundsKey FKey;
            // Handle assignment to context-sensitive field key.
            if (CSBHandler.tryGetFieldCSKey(*It, CSKeyMap,
                                            SAV.getStructAccessKey(), Context,
                                            Info, FKey)) {

              auto InitCVs = CR->getExprConstraintVars(InitExpr);
              ABInfo.handleAssignment(nullptr, {}, {FKey}, InitExpr,
                                      InitCVs.first, InitCVs.second, Context,
                                      CR);
            }
          }
        }
      }
    }
  }
  return true;
}
