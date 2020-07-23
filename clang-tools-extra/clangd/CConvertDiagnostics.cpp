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

static bool IsValidSourceFile(ConstraintsInfo &CCRes, std::string &FilePath) {
  return CCRes.ValidSourceFiles.find(FilePath) != CCRes.ValidSourceFiles.end();
}


bool CConvertDiagnostics::PopulateDiagsFromConstraintsInfo(ConstraintsInfo &Line) {
  std::lock_guard<std::mutex> Lock(DiagMutex);
  std::set<ConstraintKey> ProcessedCKeys;
  ProcessedCKeys.clear();
  auto GetLocRange = [](uint32_t Line, uint32_t ColNoS, uint32_t ColNoE) -> Range {
    Range nRange;
    Line--;
    nRange.start.line = Line;
    nRange.end.line = Line;
    nRange.start.character = ColNoS;
    if (ColNoE > 0) {
      nRange.end.character = ColNoE;
    } else {
      nRange.end.character = ColNoS + DEFAULT_PTRSIZE;
    }
    return nRange;
  };

  for (auto &WReason : Line.RealWildPtrsWithReasons) {
    if (Line.PtrSourceMap.find(WReason.first) != Line.PtrSourceMap.end()) {
      auto *PsInfo = Line.PtrSourceMap[WReason.first];
      std::string FilePath = PsInfo->getFileName();
      // If this is not a file in a project? Then ignore.
      if (!IsValidSourceFile(Line, FilePath))
        continue;

      ProcessedCKeys.insert(WReason.first);

      Diag NewDiag;
      NewDiag.Range = GetLocRange(PsInfo->getLineNo(), PsInfo->getColSNo(),
                                  PsInfo->getColENo());
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
            GetLocRange(WReason.second.LineNo, WReason.second.ColStartS,
                        WReason.second.ColStartE);
        DiagNote.Message = "Go here to know the root cause for this.";
        NewDiag.Notes.push_back(DiagNote);
      }
      AllFileDiagnostics[FilePath].push_back(NewDiag);
    }
  }

  // For non-direct wild pointers..update the reason and diag information.
  for (auto NonWildCk : Line.TotalNonDirectWildPointers) {
    if (ProcessedCKeys.find(NonWildCk) == ProcessedCKeys.end()) {
      ProcessedCKeys.insert(NonWildCk);
      if (Line.PtrSourceMap.find(NonWildCk) != Line.PtrSourceMap.end()) {
        auto *PsInfo = Line.PtrSourceMap[NonWildCk];
        std::string FilePath = PsInfo->getFileName();
        // If this is not a file in a project? Then ignore.
        if (!IsValidSourceFile(Line, FilePath))
          continue;

        ProcessedCKeys.insert(NonWildCk);
        Diag NewDiag;
        NewDiag.Range = GetLocRange(PsInfo->getLineNo(), PsInfo->getColSNo(),
                                    PsInfo->getColENo());

        NewDiag.code = std::to_string(NonWildCk);
        NewDiag.Source = Diag::CConvSec;
        NewDiag.Severity = DiagnosticsEngine::Level::Warning;
        NewDiag.Message = "Pointer is wild because it transitively "
                          "depends on other pointer(s)";

        // find the pointer group
        CVars &DirectWildPtrs = Line.GetRCVars(NonWildCk);

        unsigned MaxPtrReasons = 4;
        for (auto tC : DirectWildPtrs) {
          Note DiagNote;

          if (Line.PtrSourceMap.find(tC) != Line.PtrSourceMap.end()) {
            PsInfo = Line.PtrSourceMap[tC];
            FilePath = PsInfo->getFileName();
            DiagNote.AbsFile = FilePath;
            DiagNote.Range =
                GetLocRange(PsInfo->getLineNo(), PsInfo->getColSNo(),
                            PsInfo->getColENo());
            MaxPtrReasons--;
            DiagNote.Message = Line.RealWildPtrsWithReasons[tC].WildPtrReason;
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