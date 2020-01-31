//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// This contains the implementation of the methods in ArrayBoundsInformation.
//===----------------------------------------------------------------------===//

#include "ArrayBoundsInformation.h"
#include "ProgramInfo.h"
#include "Utils.h"

ConstraintKey ArrayBoundsInformation::getTopLevelConstraintVar(Decl *decl) {
  std::set<ConstraintVariable *> defsCVar = Info.getVariable(decl, &(decl->getASTContext()), true);
  for (auto constraintVar: defsCVar) {
    if (PVConstraint *PV = dyn_cast<PVConstraint>(constraintVar)) {
      auto &cVars = PV->getCvars();
      if (cVars.size() > 0) {
        return *(cVars.begin());
      }
    }
  }
  assert (false && "Invalid declaration variable requested.");
}

bool ArrayBoundsInformation::addBoundsInformation(FieldDecl *arrFD, FieldDecl *lenFD) {
  ConstraintKey arrCKey = getTopLevelConstraintVar(arrFD);
  std::string boundsString = lenFD->getNameAsString();
  return BoundsInfo[arrCKey].insert(std::make_pair(BoundsKind::LocalFieldBound, boundsString)).second;
}

bool ArrayBoundsInformation::addBoundsInformation(FieldDecl *arrFD, Expr *expr) {
  ConstraintKey arrCKey = getTopLevelConstraintVar(arrFD);
  auto boundInfo = getExprBoundsInfo(arrFD, expr);
  if (boundInfo.first != ArrayBoundsInformation::BoundsKind::InvalidKind)
    return BoundsInfo[arrCKey].insert(boundInfo).second;
  return false;
}

bool ArrayBoundsInformation::addBoundsInformation(FieldDecl *arrFD, BOUNDSINFOTYPE binfo) {
  ConstraintKey arrCKey = getTopLevelConstraintVar(arrFD);
  return BoundsInfo[arrCKey].insert(binfo).second;
}

bool ArrayBoundsInformation::addBoundsInformation(ParmVarDecl *arrFD, ParmVarDecl *lenFD) {
  ConstraintKey arrCKey = getTopLevelConstraintVar(arrFD);
  std::string boundsString = lenFD->getNameAsString();
  return BoundsInfo[arrCKey].insert(std::make_pair(BoundsKind::LocalParamBound, boundsString)).second;
}

bool ArrayBoundsInformation::addBoundsInformation(ParmVarDecl *arrFD, BOUNDSINFOTYPE binfo) {
  ConstraintKey arrCKey = getTopLevelConstraintVar(arrFD);
  return BoundsInfo[arrCKey].insert(binfo).second;
}

bool ArrayBoundsInformation::addBoundsInformation(VarDecl *arrFD, VarDecl *lenFD) {
  ConstraintKey arrCKey = getTopLevelConstraintVar(arrFD);
  std::string boundsString = lenFD->getNameAsString();
  return BoundsInfo[arrCKey].insert(std::make_pair(BoundsKind::LocalVarBound, boundsString)).second;
}

bool ArrayBoundsInformation::addBoundsInformation(VarDecl *arrFD, BOUNDSINFOTYPE binfo) {
  ConstraintKey arrCKey = getTopLevelConstraintVar(arrFD);
  return BoundsInfo[arrCKey].insert(binfo).second;
}

bool ArrayBoundsInformation::addBoundsInformation(VarDecl *arrFD, Expr *expr) {
  ConstraintKey arrCKey = getTopLevelConstraintVar(arrFD);
  auto boundInfo = getExprBoundsInfo(nullptr, expr);
  if (boundInfo.first != ArrayBoundsInformation::BoundsKind::InvalidKind)
    return BoundsInfo[arrCKey].insert(boundInfo).second;
  return false;
}

bool ArrayBoundsInformation::removeBoundsInformation(Decl *decl) {
  ConstraintKey arrCKey = getTopLevelConstraintVar(decl);
  if (BoundsInfo.find(arrCKey) != BoundsInfo.end()) {
    BoundsInfo.erase(arrCKey);
    return true;
  }
  return false;
}

bool ArrayBoundsInformation::hasBoundsInformation(Decl *decl) {
  ConstraintKey arrCKey = getTopLevelConstraintVar(decl);
  return BoundsInfo.find(arrCKey) != BoundsInfo.end();
}

ArrayBoundsInformation::BOUNDSINFOTYPE ArrayBoundsInformation::getBoundsInformation(Decl *decl) {
  assert(hasBoundsInformation(decl) && "Has no bounds information for the decl");
  ConstraintKey arrCKey = getTopLevelConstraintVar(decl);
  return *(BoundsInfo[arrCKey].begin());
}
bool ArrayBoundsInformation::isValidBoundKindForField(ArrayBoundsInformation::BoundsKind targetBoundKind) {
  return targetBoundKind != ArrayBoundsInformation::BoundsKind::LocalParamBound &&
         targetBoundKind != ArrayBoundsInformation::BoundsKind::LocalVarBound &&
         targetBoundKind != ArrayBoundsInformation::BoundsKind::InvalidKind;
}

ArrayBoundsInformation::BOUNDSINFOTYPE ArrayBoundsInformation::combineBoundsInfo(
                                                            FieldDecl *srcField,
                                                            ArrayBoundsInformation::BOUNDSINFOTYPE &bounds1,
                                                            ArrayBoundsInformation::BOUNDSINFOTYPE &bounds2,
                                                            std::string op) {
  auto invalidBoundRet = std::make_pair(ArrayBoundsInformation::BoundsKind::InvalidKind, "");
  ArrayBoundsInformation::BoundsKind targetKind = BoundsKind ::InvalidKind;
  if(bounds1.first != ArrayBoundsInformation::BoundsKind::InvalidKind &&
    bounds2.first != ArrayBoundsInformation::BoundsKind::InvalidKind) {
    targetKind = bounds1.first;

    if(bounds1.first != bounds2.first) {
      targetKind = BoundsKind ::InvalidKind;
      if (bounds1.first == ArrayBoundsInformation::BoundsKind::ConstantBound) {
        targetKind = bounds2.first;
      }
      if (bounds2.first == ArrayBoundsInformation::BoundsKind::ConstantBound) {
        targetKind = bounds1.first;
      }
    }
  }

  if (targetKind != BoundsKind::InvalidKind && (srcField == nullptr || isValidBoundKindForField(targetKind))) {
    return std::make_pair(targetKind,
                          "(" + bounds1.second + " " + op + " " + bounds2.second + ")");
  }
  return invalidBoundRet;
}

ArrayBoundsInformation::BOUNDSINFOTYPE ArrayBoundsInformation::getExprBoundsInfo(
  FieldDecl *srcField,
  Expr *expr) {
  expr = removeAuxillaryCasts(expr);
  auto invalidBoundRet = std::make_pair(ArrayBoundsInformation::BoundsKind::InvalidKind, "");
  if (BinaryOperator *BO = dyn_cast<BinaryOperator>(expr)) {
    Expr *LHS = BO->getLHS();
    Expr *RHS = BO->getRHS();

    auto lhsInfo = getExprBoundsInfo(srcField, LHS);
    auto rhsInfo = getExprBoundsInfo(srcField, RHS);
    return combineBoundsInfo(srcField, lhsInfo, rhsInfo, BO->getOpcodeStr().str());

  } else if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(expr)) {
    if (DeclaratorDecl *DD = dyn_cast<DeclaratorDecl>(DRE->getDecl())) {
      if (FieldDecl *FD = dyn_cast<FieldDecl>(DD)) {
        if (srcField != nullptr && FD->getParent() == srcField->getParent())
          return std::make_pair(ArrayBoundsInformation::BoundsKind::LocalFieldBound, FD->getNameAsString());

        return invalidBoundRet;
      }
      if (ParmVarDecl *PVD = dyn_cast<ParmVarDecl>(DD)) {
        if (srcField == nullptr)
          return std::make_pair(ArrayBoundsInformation::BoundsKind::LocalParamBound, PVD->getNameAsString());

        return invalidBoundRet;
      } else if (VarDecl *VD = dyn_cast<VarDecl>(DD)) {
        if (VD->hasGlobalStorage())
          return std::make_pair(ArrayBoundsInformation::BoundsKind::ConstantBound, VD->getNameAsString());

        if (!VD->hasGlobalStorage() && srcField == nullptr)
          return std::make_pair(ArrayBoundsInformation::BoundsKind::LocalVarBound, VD->getNameAsString());

        return invalidBoundRet;
      }
    }
  } else if (IntegerLiteral *IL = dyn_cast<IntegerLiteral>(expr)) {
    std::string boundsInfo = "" + std::to_string(IL->getValue().getZExtValue());
    return std::make_pair(ArrayBoundsInformation::BoundsKind::ConstantBound, boundsInfo);
  } else if (MemberExpr *DRE = dyn_cast<MemberExpr>(expr)) {
    if (FieldDecl *FD = dyn_cast<FieldDecl>(DRE->getMemberDecl())) {
      if (srcField != nullptr && FD->getParent() == srcField->getParent())
        return std::make_pair(ArrayBoundsInformation::BoundsKind::LocalFieldBound, FD->getNameAsString());

      return invalidBoundRet;
    }
  } else if(UnaryExprOrTypeTraitExpr *UETE = dyn_cast<UnaryExprOrTypeTraitExpr>(expr)) {
    if (UETE->getKind() == UETT_SizeOf) {
      std::string tmpString;
      llvm::raw_string_ostream rawStr(tmpString);
      UETE->printPretty(rawStr, nullptr, PrintingPolicy(LangOptions()));
      return std::make_pair(ArrayBoundsInformation::BoundsKind::ConstantBound, rawStr.str());
    }
  } else if (StringLiteral *SL = dyn_cast<StringLiteral>(expr)) {
    std::string boundsInfo = "" + std::to_string(SL->getLength());
    return std::make_pair(ArrayBoundsInformation::BoundsKind::ConstantBound, boundsInfo);
  }
  expr->dump();
  assert(false && "Unable to handle expression type");
}
