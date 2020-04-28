//=--CConvertDiagnostics.cpp--------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of CConvertDiagnostics methods
//===----------------------------------------------------------------------===//

#ifdef INTERACTIVECCCONV
#include "CConvertDiagnostics.h"

namespace clang {
namespace clangd {

#define DEFAULT_PTRSIZE 4

void CConvertDiagnostics::ClearAllDiags() {
  std::lock_guard<std::mutex> lock(DiagMutex);
  AllFileDiagnostics.clear();
}

static bool IsValidSourceFile(DisjointSet &CCRes, std::string &filePath) {
  return CCRes.ValidSourceFiles.find(filePath) != CCRes.ValidSourceFiles.end();
}


bool CConvertDiagnostics::PopulateDiagsFromDisjointSet(DisjointSet &CCRes) {
  std::lock_guard<std::mutex> lock(DiagMutex);
  std::set<ConstraintKey> processedCKeys;
  processedCKeys.clear();
  auto GetLocRange = [](uint32_t line, uint32_t colNo) -> Range {
    Range nRange;
    line--;
    nRange.start.line = line;
    nRange.end.line = line;
    nRange.start.character = colNo;
    nRange.end.character = colNo + DEFAULT_PTRSIZE;
    return nRange;
  };

  for (auto &wReason: CCRes.RealWildPtrsWithReasons) {
    if (CCRes.PtrSourceMap.find(wReason.first) != CCRes.PtrSourceMap.end()) {
      auto *psInfo = CCRes.PtrSourceMap[wReason.first];
      std::string filePath = psInfo->getFileName();
      // If this is not a file in a project? Then ignore.
      if (!IsValidSourceFile(CCRes, filePath))
        continue;

      processedCKeys.insert(wReason.first);

      Diag newDiag;
      newDiag.Range = GetLocRange(psInfo->getLineNo(), psInfo->getColNo());
      newDiag.Source = Diag::CConvMain;
      newDiag.Severity = DiagnosticsEngine::Level::Error;
      newDiag.code = std::to_string(wReason.first);
      newDiag.Message = "Pointer is wild because of:" +
                        wReason.second.WildPtrReason;

      // Create notes for the information about root cause.
      if (wReason.second.IsValid) {
        Note diagNote;
        diagNote.AbsFile = wReason.second.SourceFileName;
        diagNote.Range = GetLocRange(wReason.second.LineNo,
                                     wReason.second.ColStart);
        diagNote.Message = "Go here to know the root cause for this.";
        newDiag.Notes.push_back(diagNote);
      }
      AllFileDiagnostics[filePath].push_back(newDiag);
    }
  }

  // For non-direct wild pointers..update the reason and diag information.
  for (auto nonWildCK: CCRes.TotalNonDirectWildPointers) {
    if (processedCKeys.find(nonWildCK) == processedCKeys.end()) {
      processedCKeys.insert(nonWildCK);
      if (CCRes.PtrSourceMap.find(nonWildCK) != CCRes.PtrSourceMap.end()) {
        auto *psInfo = CCRes.PtrSourceMap[nonWildCK];
        std::string filePath = psInfo->getFileName();
        // If this is not a file in a project? Then ignore.
        if (!IsValidSourceFile(CCRes, filePath))
          continue;

        processedCKeys.insert(nonWildCK);
        Diag newDiag;
        newDiag.Range = GetLocRange(psInfo->getLineNo(), psInfo->getColNo());

        newDiag.code = std::to_string(nonWildCK);
        newDiag.Source = Diag::CConvSec;
        newDiag.Severity = DiagnosticsEngine::Level::Warning;
        newDiag.Message = "Pointer is wild because it transitively "
                          "depends on other pointer(s)";

        // find the pointer group
        auto directWildPtrKey = CCRes.GetLeader(nonWildCK);
        auto &ptrGroup = CCRes.GetGroup(directWildPtrKey);
        CVars directWildPtrs;
        directWildPtrs.clear();
        std::set_intersection(ptrGroup.begin(), ptrGroup.end(),
                              CCRes.AllWildPtrs.begin(),
                              CCRes.AllWildPtrs.end(),
                              std::inserter(directWildPtrs,
                                            directWildPtrs.begin()));

        unsigned maxPtrReasons = 4;
        for (auto tC : directWildPtrs) {
          Note diagNote;

          if (CCRes.PtrSourceMap.find(tC) != CCRes.PtrSourceMap.end()) {
            psInfo = CCRes.PtrSourceMap[tC];
            filePath = psInfo->getFileName();
            diagNote.AbsFile = filePath;
            diagNote.Range = GetLocRange(psInfo->getLineNo(), psInfo->getColNo());
            maxPtrReasons--;
            diagNote.Message = CCRes.RealWildPtrsWithReasons[tC].WildPtrReason;
            if (maxPtrReasons <= 1)
              diagNote.Message += " (others)";
            newDiag.Notes.push_back(diagNote);
            if (maxPtrReasons <= 1)
              break;
          }
        }
        AllFileDiagnostics[filePath].push_back(newDiag);
      }
    }
  }


  return true;
}

}
}
#endif