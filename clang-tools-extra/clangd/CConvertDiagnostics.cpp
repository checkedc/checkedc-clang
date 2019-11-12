//===--- CConvertDiagnostics.cpp -----------------------------------------*- C++-*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "CConvertDiagnostics.h"

namespace clang {
namespace clangd {
#define CCONVSOURCE "CConv"
#define DEFAULT_PTRSIZE 4
void getCConvertDiagnostics(PathRef File, std::vector<Diag> &diagVector) {
  Diag toInsert;
  toInsert.Message = "CheckedCConvert first diag";
  toInsert.Severity = DiagnosticsEngine::Level::Error;
  toInsert.Range.start.character = 9;
  toInsert.Range.end.character = 25;
  toInsert.Range.start.line = 29;
  toInsert.Range.end.line = 29;
  toInsert.source = CCONVSOURCE;
  toInsert.code = 123;
  diagVector.push_back(toInsert);
}

bool getPtrIDFromDiagMessage(const Diagnostic &diagMsg, unsigned long &ptrID) {
  if (diagMsg.source.rfind(CCONVSOURCE, 0) == 0) {
    ptrID = diagMsg.code;
    return true;
  }
  return false;
}

void CConvertDiagnostics::clearAllDiags() {
  AllFileDiagnostics.clear();
}

bool CConvertDiagnostics::populateDiagsFromDisjointSet(DisjointSet &CCRes) {
  for (auto &wReason: CCRes.realWildPtrsWithReasons) {
    if (CCRes.PtrSourceMap.find(wReason.first) != CCRes.PtrSourceMap.end()) {
      auto *psInfo = CCRes.PtrSourceMap[wReason.first];
      std::string filePath = psInfo->getFileName();
      int line = psInfo->getLineNo()-1;
      int colNo = psInfo->getColNo();
      Diag newDiag;
      newDiag.code = wReason.first;
      newDiag.source = CCONVSOURCE;
      newDiag.Severity = DiagnosticsEngine::Level::Error;
      newDiag.Range.start.line = line;
      newDiag.Range.end.line = line;
      newDiag.Range.start.character = colNo;
      newDiag.Range.end.character = colNo + DEFAULT_PTRSIZE;
      newDiag.Message = "Pointer is wild because of:" + wReason.second;
      AllFileDiagnostics[filePath].push_back(newDiag);
    }
  }
  return true;
}

}
}
