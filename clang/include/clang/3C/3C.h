//=--3C.h---------------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The main interface for invoking 3C tool.
// This provides various methods that can be used to access different
// aspects of the 3C tool.
//
//===----------------------------------------------------------------------===//

#ifndef _3C_H
#define _3C_H

#include "3CInteractiveData.h"
#include "ConstraintVariables.h"
#include "PersistentSourceLoc.h"
#include "ProgramInfo.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include <mutex>

// Options used to initialize 3C tool.
struct _3COptions {
  bool DumpIntermediate;

  bool Verbose;

  std::string OutputPostfix;

  std::string ConstraintOutputJson;

  bool DumpStats;

  std::string StatsOutputJson;

  std::string WildPtrInfoJson;

  std::string PerPtrInfoJson;

  std::vector<std::string> AllocatorFunctions;

  bool HandleVARARGS;

  bool EnablePropThruIType;

  std::string BaseDir;

  bool EnableAllTypes;

  bool AddCheckedRegions;

  bool DisableCCTypeChecker;

  bool WarnRootCause;

  bool WarnAllRootCause;

#ifdef FIVE_C
  bool RemoveItypes;
  bool ForceItypes;
#endif
};

// The main interface exposed by the 3C to interact with the tool.
class _3CInterface {

public:
  ProgramInfo GlobalProgramInfo;
  // Mutex for this interface.
  std::mutex InterfaceMutex;

  _3CInterface(const struct _3COptions &CCopt,
               const std::vector<std::string> &SourceFileList,
               clang::tooling::CompilationDatabase *CompDB);

  // Constraint Building.

  // Build initial constraints.
  bool BuildInitialConstraints();

  // Constraint Solving. The flag: ComputeInterimState requests to compute
  // interim constraint solver state.
  bool SolveConstraints(bool ComputeInterimState = false);

  // Interactivity.

  // Get all the WILD pointers and corresponding reason why they became WILD.
  ConstraintsInfo &GetWILDPtrsInfo();

  // Given a constraint key make the corresponding constraint var
  // to be non-WILD.
  bool MakeSinglePtrNonWild(ConstraintKey targetPtr);

  // Make the provided pointer non-WILD and also make all the
  // pointers, which are wild because of the same reason, as non-wild
  // as well.
  bool InvalidateWildReasonGlobally(ConstraintKey PtrKey);

  // Rewriting.

  // Write all converted versions of the files in the source file list
  // to disk
  bool WriteAllConvertedFilesToDisk();
  // Write the current converted state of the provided file.
  bool WriteConvertedFileToDisk(const std::string &FilePath);

private:
  // Are constraints already built?
  bool ConstraintsBuilt;
  void InvalidateAllConstraintsWithReason(Constraint *ConstraintToRemove);
};

#endif // _3C_H
