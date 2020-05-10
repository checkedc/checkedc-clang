//=--CConvStandalone.cpp------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// CConv tool
//
//===----------------------------------------------------------------------===//
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetSelect.h"

#include "clang/CConv/CConv.h"

using namespace clang::driver;
using namespace clang::tooling;
using namespace clang;
using namespace llvm;
static cl::OptionCategory ConvertCategory("cconv options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::extrahelp MoreHelp("");

static cl::opt<bool> OptDumpIntermediate("dump-intermediate",
                                      cl::desc("Dump intermediate "
                                               "information"),
                                      cl::init(false),
                                      cl::cat(ConvertCategory));

static cl::opt<bool> OptVerbose("verbose", cl::desc("Print verbose "
                                                 "information"),
                             cl::init(false), cl::cat(ConvertCategory));

static cl::opt<bool>
    OptSeperateMultipleFuncDecls("seperatefds",
                              cl::desc("Do not merge multiple "
                                       "declarations of functions."),
                              cl::init(false),
                              cl::cat(ConvertCategory));

static cl::opt<std::string>
    OptOutputPostfix("output-postfix",
                  cl::desc("Postfix to add to the names of rewritten "
                           "files, if not supplied writes to STDOUT"),
                  cl::init("-"), cl::cat(ConvertCategory));

static cl::opt<std::string>
    OptConstraintOutputJson("constraint-output",
                         cl::desc("Path to the file where all the analysis "
                                  "information will be dumped as json"),
                         cl::init("constraint_output.json"),
                         cl::cat(ConvertCategory));

static cl::opt<bool> OptDumpStats("dump-stats", cl::desc("Dump statistics"),
                               cl::init(false),
                               cl::cat(ConvertCategory));

static cl::opt<bool> OptHandleVARARGS("handle-varargs",
                                   cl::desc("Enable handling of varargs "
                                            "in a "
                                            "sound manner"),
                                   cl::init(false),
                                   cl::cat(ConvertCategory));

static cl::opt<bool> OptEnablePropThruIType("enable-itypeprop",
                                         cl::desc("Enable propagation of "
                                                  "constraints through ityped "
                                                  "parameters/returns."),
                                         cl::init(false),
                                         cl::cat(ConvertCategory));

static cl::opt<bool>
    OptConsiderAllocUnsafe("alloc-unsafe",
                        cl::desc("Consider the allocators "
                                 "(i.e., malloc/calloc) as unsafe."),
                        cl::init(false),
                        cl::cat(ConvertCategory));
static cl::opt<bool> OptAllTypes("alltypes",
                              cl::desc("Consider all Checked C types for "
                                       "conversion"),
                              cl::init(false),
                              cl::cat(ConvertCategory));

static cl::opt<bool> OptAddCheckedRegions("addcr", cl::desc("Add Checked "
                                                         "Regions"),
                                       cl::init(false),
                                       cl::cat(ConvertCategory));

static cl::opt<std::string>
    OptBaseDir("base-dir",
            cl::desc("Base directory for the code we're translating"),
            cl::init(""), cl::cat(ConvertCategory));

int main(int argc, const char **argv) {
  sys::PrintStackTraceOnErrorSignal(argv[0]);

  // Adjust argv to always enable fignore-checked-pointers flag.
  const char **TmpArgv = (const char **)calloc(argc + 2, sizeof(char*));
  TmpArgv[0] = argv[0];
  TmpArgv[1] = "-extra-arg-before=-fignore-checkedc-pointers";
  memcpy(TmpArgv+2, argv+1, sizeof(char*) * (argc-1));
  argc++;

  CommonOptionsParser OptionsParser(argc,
                                    (const char**)(TmpArgv),
                                    ConvertCategory);
  // Setup options.
  struct CConvertOptions CcOptions;
  CcOptions.BaseDir = OptBaseDir.getValue();
  CcOptions.ConsiderAllocUnsafe = OptConsiderAllocUnsafe;
  CcOptions.EnablePropThruIType = OptEnablePropThruIType;
  CcOptions.HandleVARARGS = OptHandleVARARGS;
  CcOptions.DumpStats = OptDumpStats;
  CcOptions.OutputPostfix = OptOutputPostfix.getValue();
  CcOptions.Verbose = OptVerbose;
  CcOptions.DumpIntermediate = OptDumpIntermediate;
  CcOptions.ConstraintOutputJson = OptConstraintOutputJson.getValue();
  CcOptions.SeperateMultipleFuncDecls = OptSeperateMultipleFuncDecls;
  CcOptions.AddCheckedRegions = OptAddCheckedRegions;
  CcOptions.EnableAllTypes = OptAllTypes;

  // Create CConv Interface.
  CConvInterface CCInterface(CcOptions,
                             OptionsParser.getSourcePathList(),
                             &(OptionsParser.getCompilations()));

  if (OptVerbose)
    errs() << "Calling Library to building Constraints.\n";
  // First build constraints.
  if (!CCInterface.BuildInitialConstraints()) {
    errs() << "Failure occurred while trying to build constraints. Exiting.\n";
    return 1;
  }

  if (OptVerbose) {
    errs() << "Finished Building Constraints.\n";
    errs() << "Trying to solve Constraints.\n";
  }

  // Next solve the constraints.
  if (!CCInterface.SolveConstraints()) {
    errs() << "Failure occurred while trying to solve constraints. Exiting.\n";
    return 1;
  }

  if (OptVerbose) {
    errs() << "Finished solving constraints.\n";
    errs() << "Trying to rewrite the converted files back.\n";
  }

  // Write all the converted files back.
  if (!CCInterface.WriteAllConvertedFilesToDisk()) {
    errs() << "Failure occurred while trying to rewrite converted files back."
              "Exiting.\n";
    return 1;
  }

  return 0;
}
