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

unsigned
ConstraintsInfo::getNumWildAffectedCKeys(const CVars &DWKeys) {
  CVars IndirectWKeys;
  for (auto CK : DWKeys) {
    auto &TK = GetSrcCVars(CK);
    IndirectWKeys.insert(TK.begin(), TK.end());
  }
  return IndirectWKeys.size();
}

void ConstraintsInfo::print_stats(llvm::raw_ostream &O) {
    O << "{\"WildPtrInfo\":{";
    O << "\"InDirectWildPtrNum\":" << TotalNonDirectWildPointers.size() << ",";
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
      TmpKeys.clear();
      std::set_intersection(InSrcWildPtrs.begin(), InSrcWildPtrs.end(),
                            T.second.begin(), T.second.end(),
                            std::inserter(TmpKeys, TmpKeys.begin()));
      O << "\"InSrcNum\":" << TmpKeys.size() << ",";
      O << "\"TotalIndirect\":" << getNumWildAffectedCKeys(T.second);
      O << "}}";
      AddComma = true;
    }
    O << "]";
    O << "}";
    O << "}}";
}