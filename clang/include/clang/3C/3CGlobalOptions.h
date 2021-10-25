//=--3CGlobalOptions.h--------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Tool options that are visible to all the components.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_3C_3CGLOBALOPTIONS_H
#define LLVM_CLANG_3C_3CGLOBALOPTIONS_H

#include "llvm/Support/CommandLine.h"

// Options used to initialize 3C tool.
//
// See clang/docs/checkedc/3C/clang-tidy.md#_3c-name-prefix
// NOLINTNEXTLINE(readability-identifier-naming)
struct _3COptions {
  bool DumpIntermediate;
  bool Verbose;
  std::string OutputPostfix;
  std::string OutputDir;
  std::string ConstraintOutputJson;
  bool DumpStats;
  std::string StatsOutputJson;
  std::string WildPtrInfoJson;
  std::string PerWildPtrInfoJson;
  std::vector<std::string> AllocatorFunctions;
  bool HandleVARARGS;
  std::string BaseDir;
  bool AllowSourcesOutsideBaseDir;
  bool AllTypes;
  bool AddCheckedRegions;
  bool EnableCCTypeChecker;
  bool WarnRootCause;
  bool WarnAllRootCause;
  bool DumpUnwritableChanges;
  bool AllowUnwritableChanges;
  bool AllowRewriteFailures;
  bool ItypesForExtern;
  bool InferTypesForUndefs;
  bool DebugSolver;
  bool OnlyGreatestSol;
  bool OnlyLeastSol;
  bool DisableRDs;
  bool DisableFunctionEdges;
  bool DisableInfDecls;
  bool DisableArrH;
  bool DebugArrSolver;
#ifdef FIVE_C
  bool RemoveItypes;
  bool ForceItypes;
#endif
};

// NOLINTNEXTLINE(readability-identifier-naming)
extern struct _3COptions _3COpts;

#endif // LLVM_CLANG_3C_3CGLOBALOPTIONS_H
