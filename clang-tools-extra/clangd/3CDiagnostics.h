//=--3CDiagnostics.h----------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Class that handles 3C Diagnostic messages.
//===----------------------------------------------------------------------===//

#ifdef INTERACTIVE3C
#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_3CDIAGNOSTICS_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_3CDIAGNOSTICS_H

#include "Diagnostics.h"
#include "clang/3C/3C.h"
#include <set>

namespace clang {
namespace clangd {

// Class that represents diagnostics messages specific to 3C.
class _3CDiagnostics {
public:
  std::mutex DiagMutex;

  // GUARDED by DiagMutex
  // Populate diagnostics from the given disjoint set.
  bool PopulateDiagsFromConstraintsInfo(ConstraintsInfo &Line);
  // GUARDED by DiagMutex
  // Clear diagnostics of all files.
  void ClearAllDiags();
  std::map<std::string, std::vector<Diag>> &GetAllFilesDiagnostics() {
    return AllFileDiagnostics;
  }

private:
  // Diagnostics of all files.
  std::map<std::string, std::vector<Diag>> AllFileDiagnostics;
};
} // namespace clangd
} // namespace clang
#endif // LLVM_CLANG_TOOLS_EXTRA_CLANGD_
#endif
