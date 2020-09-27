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
  RealWildPtrsWithReasons.clear();
  PtrSourceMap.clear();
  AllWildPtrs.clear();
  TotalNonDirectWildPointers.clear();
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

float ConstraintsInfo::getPtrAffectedScore(const CVars &AllKeys) {
  float TS = 0.0;
  for (auto CK : AllKeys) {
    TS += (1.0 / GetRCVars(CK).size());
  }
  return TS;
}

void ConstraintsInfo::print_stats(llvm::raw_ostream &O) {
    O << "{\"WildPtrInfo\":{";
    O << "\"InDirectWildPtrNum\":" << TotalNonDirectWildPointers.size() << ",";
    O << "\"InSrcInDirectWildPtrNum\":" <<
        InSrcNonDirectWildPointers.size() << ",";
    O << "\"DirectWildPtrs\":{";
    O << "\"Num\":" << AllWildPtrs.size() << ",";
    O << "\"InSrcNum\":" << InSrcWildPtrs.size() << ",";
    O << "\"Reasons\":[";

    std::map<std::string, std::set<ConstraintKey>> RsnBasedWildCKeys;
    for (auto &PtrR : RealWildPtrsWithReasons) {
      if (AllWildPtrs.find(PtrR.first) != AllWildPtrs.end()) {
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
      findIntersection(InSrcWildPtrs, T.second, TmpKeys);
      O << "\"InSrcNum\":" << TmpKeys.size() << ",";
      CVars InDWild, Tmp;
      InDWild = getWildAffectedCKeys(T.second);
      findIntersection(InDWild, InSrcNonDirectWildPointers, Tmp);
      O << "\"TotalIndirect\":" << InDWild.size() << ",";
      O << "\"InSrcIndirect\":" << Tmp.size() << ",";
      O << "\"InSrcScore\":" << getPtrAffectedScore(Tmp);
      O << "}}";
      AddComma = true;
    }
    O << "]";
    O << "}";
    O << "}}";
}

void ConstraintsInfo::print_per_ptr_stats(llvm::raw_ostream &O, Constraints &CS) {
  O << "{\"PerPtrStats\":[";
  bool AddComma = false;
  for (auto &T : AllWildPtrs) {
    if (AddComma) {
      O << ",\n";
    }
    O << "{\"PtrNum\":" << T << ", ";
    O << "\"Name\":\"" << CS.getVar(T)->getStr() << "\", ";
    O << "\"InSrc\":" << std::to_string(InSrcWildPtrs.find(T) != InSrcWildPtrs.end()) << ", ";
    CVars InDWild, Tmp;
    InDWild = getWildAffectedCKeys({T});
    findIntersection(InDWild, InSrcNonDirectWildPointers, Tmp);
    O << "\"TotalIndirect\":" << InDWild.size() << ",";
    O << "\"InSrcIndirect\":" << Tmp.size() << ",";
    O << "\"InSrcScore\":" << getPtrAffectedScore(Tmp);
    O << "}";
    AddComma = true;
  }
  O << "]";
  O << "}";
}