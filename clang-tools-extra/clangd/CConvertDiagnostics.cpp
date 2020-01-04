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
#define CCONVSOURCE "CConv_RealWild"
#define CCONVSECSOURCE "CConv_AffWild"
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
  std::set<ConstraintKey> processedCKeys;
  processedCKeys.clear();
  for (auto &wReason: CCRes.realWildPtrsWithReasons) {
    if (CCRes.PtrSourceMap.find(wReason.first) != CCRes.PtrSourceMap.end()) {
      auto *psInfo = CCRes.PtrSourceMap[wReason.first];
      std::string filePath = psInfo->getFileName();
      int line = psInfo->getLineNo()-1;
      int colNo = psInfo->getColNo();
      Diag newDiag;
      newDiag.code = wReason.first;
      processedCKeys.insert(newDiag.code);
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

  // for non-direct wild pointers..update the reason and diag information.
  for (auto nonWildCK: CCRes.totalNonDirectWildPointers) {
    if (processedCKeys.find(nonWildCK) == processedCKeys.end()) {
      if (CCRes.PtrSourceMap.find(nonWildCK) != CCRes.PtrSourceMap.end()) {
        auto *psInfo = CCRes.PtrSourceMap[nonWildCK];
        std::string filePath = psInfo->getFileName();
        int line = psInfo->getLineNo() - 1;
        int colNo = psInfo->getColNo();

        Diag newDiag;
        newDiag.code = nonWildCK;
        processedCKeys.insert(newDiag.code);
        newDiag.source = CCONVSECSOURCE;
        newDiag.Severity = DiagnosticsEngine::Level::Warning;
        newDiag.Range.start.line = line;
        newDiag.Range.end.line = line;
        newDiag.Range.start.character = colNo;
        newDiag.Range.end.character = colNo + DEFAULT_PTRSIZE;
        newDiag.Message = "Pointer is wild because it transitively depends on other pointer(s)";

        // find the pointer group
        auto directWildPtrKey = CCRes.leaders[nonWildCK];
        auto &ptrGroup = CCRes.groups[directWildPtrKey];
        CVars directWildPtrs;
        directWildPtrs.clear();
        std::set_intersection(ptrGroup.begin(), ptrGroup.end(),
                              CCRes.allWildPtrs.begin(), CCRes.allWildPtrs.end(),
                              std::inserter(directWildPtrs, directWildPtrs.begin()));

        unsigned maxPtrReasons = 3;
        for (auto tC : directWildPtrs) {
          DiagnosticRelatedInformation diagRelInfo;

          if (CCRes.PtrSourceMap.find(tC) != CCRes.PtrSourceMap.end()) {
            psInfo = CCRes.PtrSourceMap[tC];
            filePath = psInfo->getFileName();
            line = psInfo->getLineNo() - 1;
            colNo = psInfo->getColNo();

            auto duri = URIForFile::fromURI(URI::createFile(filePath), "");
            if (duri)
              diagRelInfo.location.uri = std::move(*duri);
            diagRelInfo.location.range.start.line = line;
            diagRelInfo.location.range.start.character = colNo;
            diagRelInfo.location.range.end.character = colNo + DEFAULT_PTRSIZE;
            diagRelInfo.location.range.end.line = line;
            maxPtrReasons--;
            diagRelInfo.message = CCRes.realWildPtrsWithReasons[tC].wildPtrReason;
            if (maxPtrReasons <= 1)
              diagRelInfo.message += " (others)";
            newDiag.DiagRelInfo.push_back(diagRelInfo);
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
