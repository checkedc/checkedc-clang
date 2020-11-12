//=--3CDiagnostics.cpp--------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of _3CDiagnostics methods
//===----------------------------------------------------------------------===//

#ifdef INTERACTIVE3C
#include "3CDiagnostics.h"

namespace clang {
namespace clangd {

#define DEFAULT_PTRSIZE 4

void _3CDiagnostics::ClearAllDiags() {
  std::lock_guard<std::mutex> Lock(DiagMutex);
  AllFileDiagnostics.clear();
}

static bool IsValidSourceFile(ConstraintsInfo &CCRes, std::string &FilePath) {
  return CCRes.ValidSourceFiles.find(FilePath) != CCRes.ValidSourceFiles.end();
}

bool _3CDiagnostics::PopulateDiagsFromConstraintsInfo(ConstraintsInfo &Line) {
  std::lock_guard<std::mutex> Lock(DiagMutex);
  std::set<ConstraintKey> ProcessedCKeys;
  ProcessedCKeys.clear();
  auto GetLocRange = [](uint32_t Line, uint32_t ColNoS,
                        uint32_t ColNoE) -> Range {
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

  for (auto &WReason : Line.RootWildAtomsWithReason) {
    if (Line.AtomSourceMap.find(WReason.first) != Line.AtomSourceMap.end()) {
      auto *PsInfo = Line.AtomSourceMap[WReason.first];
      std::string FilePath = PsInfo->getFileName();
      // If this is not a file in a project? Then ignore.
      if (!IsValidSourceFile(Line, FilePath))
        continue;

      ProcessedCKeys.insert(WReason.first);

      Diag NewDiag;
      NewDiag.Range = GetLocRange(PsInfo->getLineNo(), PsInfo->getColSNo(),
                                  PsInfo->getColENo());
      NewDiag.Source = Diag::_3CMain;
      NewDiag.Severity = DiagnosticsEngine::Level::Error;
      NewDiag.code = std::to_string(WReason.first);
      NewDiag.Message =
          "Pointer is wild because of:" + WReason.second.getWildPtrReason();

      // Create notes for the information about root cause.
      PersistentSourceLoc SL = WReason.second.getLocation();
      if (SL.valid()) {
        Note DiagNote;
        DiagNote.AbsFile = SL.getFileName();
        DiagNote.Range =
            GetLocRange(SL.getLineNo(), SL.getColSNo(), SL.getColENo());
        DiagNote.Message = "Go here to know the root cause for this.";
        NewDiag.Notes.push_back(DiagNote);
      }
      AllFileDiagnostics[FilePath].push_back(NewDiag);
    }
  }

  // For non-direct wild pointers..update the reason and diag information.
  for (auto NonWildCk : Line.TotalNonDirectWildAtoms) {
    if (ProcessedCKeys.find(NonWildCk) == ProcessedCKeys.end()) {
      ProcessedCKeys.insert(NonWildCk);
      if (Line.AtomSourceMap.find(NonWildCk) != Line.AtomSourceMap.end()) {
        auto *PsInfo = Line.AtomSourceMap[NonWildCk];
        std::string FilePath = PsInfo->getFileName();
        // If this is not a file in a project? Then ignore.
        if (!IsValidSourceFile(Line, FilePath))
          continue;

        ProcessedCKeys.insert(NonWildCk);
        Diag NewDiag;
        NewDiag.Range = GetLocRange(PsInfo->getLineNo(), PsInfo->getColSNo(),
                                    PsInfo->getColENo());

        NewDiag.code = std::to_string(NonWildCk);
        NewDiag.Source = Diag::_3CSec;
        NewDiag.Severity = DiagnosticsEngine::Level::Warning;
        NewDiag.Message = "Pointer is wild because it transitively "
                          "depends on other pointer(s)";

        // find the pointer group
        CVars &DirectWildPtrs = Line.GetRCVars(NonWildCk);

        unsigned MaxPtrReasons = 4;
        for (auto tC : DirectWildPtrs) {
          Note DiagNote;

          if (Line.AtomSourceMap.find(tC) != Line.AtomSourceMap.end()) {
            PsInfo = Line.AtomSourceMap[tC];
            FilePath = PsInfo->getFileName();
            DiagNote.AbsFile = FilePath;
            DiagNote.Range = GetLocRange(
                PsInfo->getLineNo(), PsInfo->getColSNo(), PsInfo->getColENo());
            MaxPtrReasons--;
            DiagNote.Message =
                Line.RootWildAtomsWithReason[tC].getWildPtrReason();
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

} // namespace clangd
} // namespace clang
#endif