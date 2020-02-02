//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Tool options that are visible to all the components.
//===----------------------------------------------------------------------===//

#ifndef _CCGLOBALOPTIONS_H
#define _CCGLOBALOPTIONS_H

#include "llvm/Support/CommandLine.h"

#ifdef CCCONVSTANDALONE

extern llvm::cl::opt<bool> Verbose;
extern llvm::cl::opt<bool> DumpIntermediate;
extern llvm::cl::opt<bool> handleVARARGS;
extern llvm::cl::opt<bool> mergeMultipleFuncDecls;
extern llvm::cl::opt<bool> enablePropThruIType;
extern llvm::cl::opt<bool> considerAllocUnsafe;
extern llvm::cl::opt<std::string> BaseDir;
extern llvm::cl::opt<bool> allTypes;
#else

extern bool Verbose;
extern bool DumpIntermediate;
extern bool handleVARARGS;
extern bool mergeMultipleFuncDecls;
extern bool enablePropThruIType;
extern bool considerAllocUnsafe;
extern bool allTypes;
extern std::string BaseDir;
#endif

#endif //_CCGLOBALOPTIONS_H
