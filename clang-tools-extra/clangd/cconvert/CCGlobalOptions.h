//=--CCGlobalOptions.h--------------------------------------------*- C++-*-===//
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

#ifndef _CCGLOBALOPTIONS_H
#define _CCGLOBALOPTIONS_H

#include "llvm/Support/CommandLine.h"

#ifdef CCCONVSTANDALONE

extern llvm::cl::opt<bool> Verbose;
extern llvm::cl::opt<bool> DumpIntermediate;
extern llvm::cl::opt<bool> HandleVARARGS;
extern llvm::cl::opt<bool> SeperateMultipleFuncDecls;
extern llvm::cl::opt<bool> EnablePropThruIType;
extern llvm::cl::opt<bool> ConsiderAllocUnsafe;
extern llvm::cl::opt<std::string> BaseDir;
extern llvm::cl::opt<bool> AllTypes;
extern llvm::cl::opt<bool> AddCheckedRegions;
#else

extern bool Verbose;
extern bool DumpIntermediate;
extern bool HandleVARARGS;
extern bool SeperateMultipleFuncDecls;
extern bool EnablePropThruIType;
extern bool ConsiderAllocUnsafe;
extern bool AllTypes;
extern std::string BaseDir;
extern bool AddCheckedRegions;
#endif

#endif //_CCGLOBALOPTIONS_H
