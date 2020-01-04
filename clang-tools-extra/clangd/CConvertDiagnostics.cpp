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
      newDiag.Message = "Pointer is wild because of:" + wReason.second.wildPtrReason;
      if (wReason.second.isValid) {
        DiagnosticRelatedInformation diagRelInfo;
        auto duri = URIForFile::fromURI(URI::createFile(wReason.second.sourceFileName), "");
        if (duri)
          diagRelInfo.location.uri = std::move(*duri);
        int rootCauseLineNum = wReason.second.lineNo - 1;
        int rootCauseColNum = wReason.second.colStart;
        diagRelInfo.location.range.start.line = rootCauseLineNum;
        diagRelInfo.location.range.start.character = rootCauseColNum;
        diagRelInfo.location.range.end.character = rootCauseColNum + DEFAULT_PTRSIZE;
        diagRelInfo.location.range.end.line = rootCauseLineNum;
        diagRelInfo.message = "Go here to know the root cause for this.";
        newDiag.DiagRelInfo.push_back(diagRelInfo);
      }
      AllFileDiagnostics[filePath].push_back(newDiag);
    }
  }
  return true;
}

}
}
