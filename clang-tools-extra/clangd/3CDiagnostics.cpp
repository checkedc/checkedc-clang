//===--- 3CDiagnostics.cpp - 3C Diagnostics Functions -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifdef LSP3C
#include "3CDiagnostics.h"
#include "support/Logger.h"
#define DEF_PTR_SIZE 5

namespace clang {
namespace clangd {

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
  int i=0;
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
      nRange.end.character = ColNoS + DEF_PTR_SIZE;
    }
    return nRange;
  };


  for (auto &WReason : Line.RootWildAtomsWithReason) {
    if (Line.AtomSourceMap.find(WReason.first) != Line.AtomSourceMap.end()) {
      auto PsInfo = Line.AtomSourceMap[WReason.first];
      std::string FilePath = PsInfo.getFileName();
      // If this is not a file in a project? Then ignore.
      if (!IsValidSourceFile(Line, FilePath))
        continue;

      ProcessedCKeys.insert(WReason.first);

      Diag NewDiag;
      NewDiag.Range = GetLocRange(PsInfo.getLineNo(), PsInfo.getColSNo()-1,
                                  PsInfo.getColENo());
      NewDiag.Source = Diag::Main3C;
      NewDiag.Severity = DiagnosticsEngine::Level::Error;
      NewDiag.code = std::to_string(WReason.first);
      NewDiag.Message =
          "Pointer is wild because of :" + WReason.second.getReason();

      // Create notes for the information about root cause.
      PersistentSourceLoc SL = WReason.second.getLocation();
      if (SL.valid()) {
        Note DiagNote;
        DiagNote.AbsFile = SL.getFileName();
        DiagNote.Range =
            GetLocRange(SL.getLineNo(), SL.getColSNo(), SL.getColENo());
        DiagNote.Message = WReason.second.getReason();
        NewDiag.Notes.push_back(DiagNote);
      }
      AllFileDiagnostics[FilePath].push_back(NewDiag);
      i++;

    }
  }
  log(std::to_string(i).c_str());

  return true;
}
}
}
#endif