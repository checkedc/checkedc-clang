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
  std::lock_guard<std::mutex> Lock(DiagMutex);
  AllFileDiagnostics.clear();
}

static bool IsValidSourceFile(DisjointSet &CCRes, std::string &FilePath) {
  return CCRes.ValidSourceFiles.find(FilePath) != CCRes.ValidSourceFiles.end();
}


bool CConvertDiagnostics::PopulateDiagsFromDisjointSet(DisjointSet &CCRes) {
  std::lock_guard<std::mutex> Lock(DiagMutex);
  std::set<ConstraintKey> ProcessedCKeys;
  ProcessedCKeys.clear();
  auto GetLocRange = [](uint32_t Line, uint32_t ColNo) -> Range {
    Range nRange;
    Line--;
    nRange.start.line = Line;
    nRange.end.line = Line;
    nRange.start.character = ColNo;
    nRange.end.character = ColNo + DEFAULT_PTRSIZE;
    return nRange;
  };

  for (auto &WReason : CCRes.RealWildPtrsWithReasons) {
    if (CCRes.PtrSourceMap.find(WReason.first) != CCRes.PtrSourceMap.end()) {
      auto *PsInfo = CCRes.PtrSourceMap[WReason.first];
      std::string FilePath = PsInfo->getFileName();
      // If this is not a file in a project? Then ignore.
      if (!IsValidSourceFile(CCRes, FilePath))
        continue;

      ProcessedCKeys.insert(WReason.first);

      Diag NewDiag;
      NewDiag.Range = GetLocRange(PsInfo->getLineNo(), PsInfo->getColNo());
      NewDiag.Source = Diag::CConvMain;
      NewDiag.Severity = DiagnosticsEngine::Level::Error;
      NewDiag.code = std::to_string(WReason.first);
      NewDiag.Message = "Pointer is wild because of:" +
                        WReason.second.WildPtrReason;

      // Create notes for the information about root cause.
      if (WReason.second.IsValid) {
        Note DiagNote;
        DiagNote.AbsFile = WReason.second.SourceFileName;
        DiagNote.Range =
            GetLocRange(WReason.second.LineNo, WReason.second.ColStart);
        DiagNote.Message = "Go here to know the root cause for this.";
        NewDiag.Notes.push_back(DiagNote);
      }
      AllFileDiagnostics[FilePath].push_back(NewDiag);
    }
  }

  // For non-direct wild pointers..update the reason and diag information.
  for (auto NonWildCk : CCRes.TotalNonDirectWildPointers) {
    if (ProcessedCKeys.find(NonWildCk) == ProcessedCKeys.end()) {
      ProcessedCKeys.insert(NonWildCk);
      if (CCRes.PtrSourceMap.find(NonWildCk) != CCRes.PtrSourceMap.end()) {
        auto *PsInfo = CCRes.PtrSourceMap[NonWildCk];
        std::string FilePath = PsInfo->getFileName();
        // If this is not a file in a project? Then ignore.
        if (!IsValidSourceFile(CCRes, FilePath))
          continue;

        ProcessedCKeys.insert(NonWildCk);
        Diag NewDiag;
        NewDiag.Range = GetLocRange(PsInfo->getLineNo(), PsInfo->getColNo());

        NewDiag.code = std::to_string(NonWildCk);
        NewDiag.Source = Diag::CConvSec;
        NewDiag.Severity = DiagnosticsEngine::Level::Warning;
        NewDiag.Message = "Pointer is wild because it transitively "
                          "depends on other pointer(s)";

        // find the pointer group
        auto DirectWildPtrKey = CCRes.GetLeader(NonWildCk);
        auto &PtrGroup = CCRes.GetGroup(DirectWildPtrKey);
        CVars DirectWildPtrs;
        DirectWildPtrs.clear();
        std::set_intersection(
            PtrGroup.begin(), PtrGroup.end(),
                              CCRes.AllWildPtrs.begin(),
                              CCRes.AllWildPtrs.end(),
                              std::inserter(DirectWildPtrs,
                          DirectWildPtrs.begin()));

        unsigned MaxPtrReasons = 4;
        for (auto tC : DirectWildPtrs) {
          Note DiagNote;

          if (CCRes.PtrSourceMap.find(tC) != CCRes.PtrSourceMap.end()) {
            PsInfo = CCRes.PtrSourceMap[tC];
            FilePath = PsInfo->getFileName();
            DiagNote.AbsFile = FilePath;
            DiagNote.Range =
                GetLocRange(PsInfo->getLineNo(), PsInfo->getColNo());
            MaxPtrReasons--;
            DiagNote.Message = CCRes.RealWildPtrsWithReasons[tC].WildPtrReason;
            if (MaxPtrReasons <= 1)
              DiagNote.Message += " (others)";
            NewDiag.Notes.push_back(DiagNote);
            if (MaxPtrReasons <= 1)
              break;
          }
        }
        AllFileDiagnostics[FilePath].push_back(NewDiag);
      }
    }
  }


  return true;
}

}
}
#endif