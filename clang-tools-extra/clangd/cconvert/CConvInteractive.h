//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// The main file that controls the interaction between clangd and cconv
//===----------------------------------------------------------------------===/

#ifndef CLANG_TOOLS_EXTRA_CLANGD_CCONVERT_CCONVINTERACTIVE_H
#define CLANG_TOOLS_EXTRA_CLANGD_CCONVERT_CCONVINTERACTIVE_H

#include "ConstraintVariables.h"
#include "PersistentSourceLoc.h"
#include "clang/Tooling/CommonOptionsParser.h"

struct WildPointerInferenceInfo {
  std::string sourceFileName = "";
  std::string wildPtrReason = "";
  bool isValid = false;
  unsigned lineNo = 0;
  unsigned colStart = 0;
};

class DisjointSet {
public:
  DisjointSet() {

  }
  void clear();
  void addElements(ConstraintKey, ConstraintKey);

  std::map<ConstraintKey, ConstraintKey> leaders;
  std::map<ConstraintKey, CVars> groups;
  std::map<ConstraintKey, struct WildPointerInferenceInfo> realWildPtrsWithReasons;
  CVars allWildPtrs;
  std::map<ConstraintKey, PersistentSourceLoc*> PtrSourceMap;
};


struct CConvertOptions {
  bool DumpIntermediate;

  bool Verbose;

  bool mergeMultipleFuncDecls;

  std::string OutputPostfix;

  std::string ConstraintOutputJson;

  bool DumpStats;

  bool handleVARARGS;

  bool enablePropThruIType;

  bool considerAllocUnsafe;

  std::string BaseDir;
};

DisjointSet& getWILDPtrsInfo();

// make the provided pointer non-wild
bool makeSinglePtrNonWild(ConstraintKey targetPtr);

// make the provided pointer non-WILD and also make all the
// pointers, which are wild because of the same reason, as non-wild
// as well
bool invalidateWildReasonGlobally(ConstraintKey targetPtr);

bool initializeCConvert(clang::tooling::CommonOptionsParser &OptionsParser,
                        struct CConvertOptions &options);
bool buildInitialConstraints();

bool writeConvertedFileToDisk(std::string filePath);


#endif //CLANG_TOOLS_EXTRA_CLANGD_CCONVERT_CCONVINTERACTIVE_H
