//=--3CInteractiveData.cpp----------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Data structure methods
//
//===----------------------------------------------------------------------===//

#include "clang/3C/3CInteractiveData.h"

void ConstraintsInfo::Clear() {
  RootWildAtomsWithReason.clear();
  AtomSourceMap.clear();
  AllWildAtoms.clear();
  TotalNonDirectWildAtoms.clear();
  ValidSourceFiles.clear();
  RCMap.clear();
  SrcWMap.clear();
}

CVars &ConstraintsInfo::GetRCVars(ConstraintKey Ckey) { return RCMap[Ckey]; }

CVars &ConstraintsInfo::GetSrcCVars(ConstraintKey Ckey) {
  return SrcWMap[Ckey];
}

CVars ConstraintsInfo::getWildAffectedCKeys(const CVars &DWKeys) {
  CVars IndirectWKeys;
  for (auto CK : DWKeys) {
    auto &TK = GetSrcCVars(CK);
    IndirectWKeys.insert(TK.begin(), TK.end());
  }
  return IndirectWKeys;
}

float ConstraintsInfo::getAtomAffectedScore(const CVars &AllKeys) {
  float TS = 0.0;
  for (auto CK : AllKeys) {
    TS += (1.0 / GetRCVars(CK).size());
  }
  return TS;
}

float ConstraintsInfo::getPtrAffectedScore(
    const std::set<ConstraintVariable *> CVs) {
  float TS = 0.0;
  for (auto *CV : CVs)
    TS += (1.0 / PtrRCMap[CV].size());
  return TS;
}

void ConstraintsInfo::printStats(llvm::raw_ostream &O) {
  O << "{\"WildPtrInfo\":{";
  O << "\"InDirectWildPtrNum\":" << TotalNonDirectWildAtoms.size() << ",";
  O << "\"InSrcInDirectWildPtrNum\":" << InSrcNonDirectWildAtoms.size() << ",";
  O << "\"DirectWildPtrs\":{";
  O << "\"Num\":" << AllWildAtoms.size() << ",";
  O << "\"InSrcNum\":" << InSrcWildAtoms.size() << ",";
  O << "\"Reasons\":[";

  std::map<std::string, std::set<ConstraintKey>> RsnBasedWildCKeys;
  for (auto &PtrR : RootWildAtomsWithReason) {
    if (AllWildAtoms.find(PtrR.first) != AllWildAtoms.end()) {
      RsnBasedWildCKeys[PtrR.second.getWildPtrReason()].insert(PtrR.first);
    }
  }
  bool AddComma = false;
  for (auto &T : RsnBasedWildCKeys) {
    if (AddComma) {
      O << ",\n";
    }
    O << "{\"" << T.first << "\":{";
    O << "\"Num\":" << T.second.size() << ",";
    CVars TmpKeys;
    findIntersection(InSrcWildAtoms, T.second, TmpKeys);
    O << "\"InSrcNum\":" << TmpKeys.size() << ",";
    CVars InDWild, Tmp;
    InDWild = getWildAffectedCKeys(T.second);
    findIntersection(InDWild, InSrcNonDirectWildAtoms, Tmp);
    O << "\"TotalIndirect\":" << InDWild.size() << ",";
    O << "\"InSrcIndirect\":" << Tmp.size() << ",";
    O << "\"InSrcScore\":" << getAtomAffectedScore(Tmp);
    O << "}}";
    AddComma = true;
  }
  O << "]";
  O << "}";
  O << "}}";
}

void ConstraintsInfo::printRootCauseStats(llvm::raw_ostream &O,
                                          Constraints &CS) {
  O << "{\"RootCauseStats\":[";
  bool AddComma = false;
  for (auto &T : AllWildAtoms) {
    if (AddComma)
      O << ",\n";
    printConstraintStats(O, CS, T);
    AddComma = true;
  }
  O << "]}";
}

void ConstraintsInfo::printConstraintStats(llvm::raw_ostream &O,
                                           Constraints &CS,
                                           ConstraintKey Cause) {
  O << "{\"ConstraintKey\":" << Cause << ", ";
  O << "\"Name\":\"" << CS.getVar(Cause)->getStr() << "\", ";
  WildPointerInferenceInfo PtrInfo = RootWildAtomsWithReason.at(Cause);
  O << "\"Reason\":\"" << PtrInfo.getWildPtrReason() << "\", ";
  O << "\"Location\":";
  const PersistentSourceLoc &PSL = PtrInfo.getLocation();
  if (PSL.valid()) {
    O << "\"";
    PSL.print(O);
    O << "\"";
  } else
    O << "null";
  O << ", ";

  std::set<ConstraintKey> AtomsAffected = getWildAffectedCKeys({Cause});
  O << "\"AtomsAffected\":" << AtomsAffected.size() << ", ";
  O << "\"AtomsScore\":" << getAtomAffectedScore(AtomsAffected) << ", ";

  std::set<ConstraintVariable *> PtrsAffected = PtrSrcWMap[Cause];
  O << "\"PtrsAffected\":" << PtrsAffected.size() << ",";
  O << "\"PtrsScore\":" << getPtrAffectedScore(PtrsAffected);
  O << "}";
}

int ConstraintsInfo::getNumPtrsAffected(ConstraintKey CK) {
  return PtrSrcWMap[CK].size();
}
