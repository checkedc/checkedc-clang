//=--CConvInteractive.h-------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The main file that defines the interaction between clangd and cconv.
//
//===----------------------------------------------------------------------===//

#ifndef _CCONVINTERACTIVE_H
#define _CCONVINTERACTIVE_H

#include "ConstraintVariables.h"
#include "PersistentSourceLoc.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "CConvInteractiveData.h"
#include "ProgramInfo.h"
#include <mutex>

// Options used to initialize CConv tool
struct CConvertOptions {
  bool DumpIntermediate;

  bool Verbose;

  bool SeperateMultipleFuncDecls;

  std::string OutputPostfix;

  std::string ConstraintOutputJson;

  bool DumpStats;

  bool HandleVARARGS;

  bool EnablePropThruIType;

  bool ConsiderAllocUnsafe;

  std::string BaseDir;
};

// The main interface exposed by the CConv to interact with the tool
class CConvInterface {

public:
  ProgramInfo GlobalProgramInfo;
  // Mutex for this interface
  std::mutex InterfaceMutex;

  DisjointSet &GetWILDPtrsInfo();

  // Make only the provided pointer non-wild
  bool MakeSinglePtrNonWild(ConstraintKey targetPtr);

  // Make the provided pointer non-WILD and also make all the
  // pointers, which are wild because of the same reason, as non-wild
  // as well
  bool InvalidateWildReasonGlobally(ConstraintKey targetPtr);

  // Initialize CConv interface
  bool InitializeCConvert(clang::tooling::CommonOptionsParser &OptionsParser,
                          struct CConvertOptions &options);

  // Build initial constraints
  bool BuildInitialConstraints();

  // Write the current converted state of the provided file.
  bool WriteConvertedFileToDisk(const std::string &filePath);

private:
  void ResetAllPointerConstraints();
  void InvalidateAllConstraintsWithReason(Constraint *constraintToRemove);

};


#endif //_CCONVINTERACTIVE_H
