//=--CConvInteractiveData.cpp-------------------------------------*- C++-*-===//
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

#include "clang/CConv/CConvInteractiveData.h"

void ConstraintsInfo::Clear() {
  RootWildAtomsWithReason.clear();
  AtomSourceMap.clear();
  AllWildAtoms.clear();
  TotalNonDirectWildAtoms.clear();
  ValidSourceFiles.clear();
  RCMap.clear();
  SrcWMap.clear();
}

CVars &ConstraintsInfo::GetRCVars(ConstraintKey Ckey) {
  return RCMap[Ckey];
}

CVars &ConstraintsInfo::GetSrcCVars(ConstraintKey Ckey) {
  return SrcWMap[Ckey];
}

CVars
ConstraintsInfo::getWildAffectedCKeys(const CVars &DWKeys) {
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

float ConstraintsInfo::getPtrAffectedScore
    (const std::set<ConstraintVariable *> CVs) {
  float TS = 0.0;
  for (auto *CV : CVs)
    TS += (1.0 / PtrRCMap[CV].size());
  return TS;
}

void ConstraintsInfo::printStats(llvm::raw_ostream &O) {
    O << "{\"WildPtrInfo\":{";
    O << "\"InDirectWildPtrNum\":" << TotalNonDirectWildAtoms.size() << ",";
    O << "\"InSrcInDirectWildPtrNum\":" <<
      InSrcNonDirectWildAtoms.size() << ",";
    O << "\"DirectWildPtrs\":{";
    O << "\"Num\":" << AllWildAtoms.size() << ",";
    O << "\"InSrcNum\":" << InSrcWildAtoms.size() << ",";
    O << "\"Reasons\":[";

    std::map<std::string, std::set<ConstraintKey>> RsnBasedWildCKeys;
    for (auto &PtrR : RootWildAtomsWithReason) {
      if (AllWildAtoms.find(PtrR.first) != AllWildAtoms.end()) {
        RsnBasedWildCKeys[PtrR.second.WildPtrReason].insert(PtrR.first);
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

void ConstraintsInfo::printPerAtomStats(llvm::raw_ostream &O, Constraints &CS) {
  O << "{\"PerAtomStats\":[";
  bool AddComma = false;
  for (auto &T : AllWildAtoms) {
    if (AddComma) {
      O << ",\n";
    }
    O << "{\"PtrNum\":" << T << ", ";
    O << "\"Name\":\"" << CS.getVar(T)->getStr() << "\", ";
    O << "\"InSrc\":" << std::to_string(InSrcWildAtoms.find(T) != InSrcWildAtoms.end()) << ", ";
    CVars InDWild, Tmp;
    InDWild = getWildAffectedCKeys({T});
    findIntersection(InDWild, InSrcNonDirectWildAtoms, Tmp);
    O << "\"TotalIndirect\":" << InDWild.size() << ",";
    O << "\"InSrcIndirect\":" << Tmp.size() << ",";
    O << "\"InSrcScore\":" << getAtomAffectedScore(Tmp);
    O << "}";
    AddComma = true;
  }
  O << "]";
  O << "}";
}

void ConstraintsInfo::printPerPtrStats(llvm::raw_ostream &O, Constraints &CS) {
  O << "{\"PerPtrStats\":[";
  bool AddComma = false;
  for (auto &T : AllWildAtoms) {
    if (AddComma)
      O << ",\n";
    O << "{\"PtrNum\":" << T << ", ";
    O << "\"Name\":\"" << CS.getVar(T)->getStr() << "\", ";
    O << "\"InSrc\":" << std::to_string(InSrcWildAtoms.find(T) != InSrcWildAtoms.end()) << ", ";
    std::set<ConstraintVariable *> InDWild = PtrSrcWMap[T];
    O << "\"TotalIndirect\":" << InDWild.size() << ",";
    O << "\"Score\":" << getPtrAffectedScore(InDWild);
    O << "}";
    AddComma = true;
  }
  O << "]";
  O << "}";
}

int ConstraintsInfo::getNumPtrsAffected(ConstraintKey CK) {
  return PtrSrcWMap[CK].size();
}
