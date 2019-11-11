//===--- CConvertDiagnostics.h -CConvertDisagnostics code ----------------*- C++-*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_CCONVERTDIAGNOSTICS_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_CCONVERTDIAGNOSTICS_H

#include <set>
#include "Diagnostics.h"
#include "cconvert/CConvInteractive.h"

namespace clang {
namespace clangd {

void getCConvertDiagnostics(PathRef File, std::vector<Diag> &diagVector);
bool getPtrIDFromDiagMessage(const Diagnostic &diagMsg, unsigned long &ptrID);

class CConvertDiagnostics {
public:

  std::map<std::string, std::vector<Diag>> AllFileDiagnostics;

  bool populateDiagsFromDisjointSet(DisjointSet &CCRes);

  void clearAllDiags();

};
}
}
#endif //LLVM_CLANG_TOOLS_EXTRA_CLANGD_
