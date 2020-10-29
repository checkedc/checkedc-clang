//=--3CStandalone.cpp---------------------------------------------*- C++-*-===//
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

#include "clang/3C/3C.h"

using namespace clang::driver;
using namespace clang::tooling;
using namespace clang;
using namespace llvm;
static cl::OptionCategory _3CCategory("cconv options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::extrahelp MoreHelp("");

static cl::opt<bool> OptDumpIntermediate("dump-intermediate",
                                      cl::desc("Dump intermediate "
                                               "information"),
                                      cl::init(false),
                                      cl::cat(_3CCategory));

static cl::opt<bool> OptVerbose("verbose", cl::desc("Print verbose "
                                                 "information"),
                             cl::init(false), cl::cat(_3CCategory));

static cl::opt<std::string>
    OptOutputPostfix("output-postfix",
                  cl::desc("Postfix to add to the names of rewritten "
                           "files, if not supplied writes to STDOUT"),
                  cl::init("-"), cl::cat(_3CCategory));

static cl::opt<std::string>
    OptMalloc("use-malloc",
                     cl::desc("Allows for the usage of user-specified "
                              "versions of function allocators"),
                     cl::init(""), cl::cat(_3CCategory));

static cl::opt<std::string>
    OptConstraintOutputJson("constraint-output",
                         cl::desc("Path to the file where all the analysis "
                                  "information will be dumped as json"),
                         cl::init("constraint_output.json"),
                         cl::cat(_3CCategory));

static cl::opt<std::string>
    OptStatsOutputJson("stats-output",
                            cl::desc("Path to the file where all the stats "
                                     "will be dumped as json"),
                            cl::init("TotalConstraintStats.json"),
                            cl::cat(_3CCategory));
static cl::opt<std::string>
    OptWildPtrInfoJson("wildptrstats-output",
                            cl::desc("Path to the file where all the info "
                                     "related to WILD ptr grouped by reason"
                                     " will be dumped as json"),
                            cl::init("WildPtrStats.json"),
                            cl::cat(_3CCategory));

static cl::opt<std::string>
  OptPerPtrWILDInfoJson("perptrstats-output",
                        cl::desc("Path to the file where all the info "
                                 "related to each WILD ptr will be dumped as json"),
                        cl::init("PerWildPtrStats.json"),
                        cl::cat(_3CCategory));

static cl::opt<bool> OptDumpStats("dump-stats", cl::desc("Dump statistics"),
                               cl::init(false),
                               cl::cat(_3CCategory));

static cl::opt<bool> OptHandleVARARGS("handle-varargs",
                                   cl::desc("Enable handling of varargs "
                                            "in a "
                                            "sound manner"),
                                   cl::init(false),
                                   cl::cat(_3CCategory));

static cl::opt<bool> OptEnablePropThruIType("enable-itypeprop",
                                         cl::desc("Enable propagation of "
                                                  "constraints through ityped "
                                                  "parameters/returns."),
                                         cl::init(false),
                                         cl::cat(_3CCategory));

static cl::opt<bool> OptAllTypes("alltypes",
                              cl::desc("Consider all Checked C types for "
                                       "conversion"),
                              cl::init(false),
                              cl::cat(_3CCategory));

static cl::opt<bool> OptAddCheckedRegions("addcr", cl::desc("Add Checked "
                                                         "Regions"),
                                       cl::init(false),
                                       cl::cat(_3CCategory));

static cl::opt<bool> OptDiableCCTypeChecker("disccty",
                              cl::desc("Do not disable checked c type checker."),
                              cl::init(false),
                              cl::cat(_3CCategory));

static cl::opt<std::string>
    OptBaseDir("base-dir",
            cl::desc("Base directory for the code we're translating"),
            cl::init(""), cl::cat(_3CCategory));

static cl::opt<bool> OptWarnRootCause
    ("warn-root-cause",
    cl::desc("Emit warnings indicating root causes of unchecked pointers."),
    cl::init(false), cl::cat(_3CCategory));

static cl::opt<bool> OptWarnAllRootCause
    ("warn-all-root-cause",
     cl::desc("Emit warnings for all root causes, "
              "even those unlikely to be interesting."),
     cl::init(false), cl::cat(_3CCategory));

#ifdef FIVE_C
static cl::opt<bool> OptRemoveItypes
    ("remove-itypes",
     cl::desc("Remove unneeded interoperation type annotations."),
     cl::init(false), cl::cat(_3CCategory));

static cl::opt<bool> OptForceItypes
    ("force-itypes",
     cl::desc("Use interoperation types instead of regular checked pointers. "),
     cl::init(false), cl::cat(_3CCategory));
#endif

int main(int argc, const char **argv) {
  sys::PrintStackTraceOnErrorSignal(argv[0]);

  // Initialize targets for clang module support.
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmPrinters();
  InitializeAllAsmParsers();

  CommonOptionsParser OptionsParser(argc,
                                    (const char**)(argv),
                                    _3CCategory);
  // Setup options.
  struct _3COptions CcOptions;
  CcOptions.BaseDir = OptBaseDir.getValue();
  CcOptions.EnablePropThruIType = OptEnablePropThruIType;
  CcOptions.HandleVARARGS = OptHandleVARARGS;
  CcOptions.DumpStats = OptDumpStats;
  CcOptions.OutputPostfix = OptOutputPostfix.getValue();
  CcOptions.Verbose = OptVerbose;
  CcOptions.DumpIntermediate = OptDumpIntermediate;
  CcOptions.ConstraintOutputJson = OptConstraintOutputJson.getValue();
  CcOptions.StatsOutputJson = OptStatsOutputJson.getValue();
  CcOptions.WildPtrInfoJson = OptWildPtrInfoJson.getValue();
  CcOptions.PerPtrInfoJson = OptPerPtrWILDInfoJson.getValue();
  CcOptions.AddCheckedRegions = OptAddCheckedRegions;
  CcOptions.EnableAllTypes = OptAllTypes;
  CcOptions.DisableCCTypeChecker = OptDiableCCTypeChecker;
  CcOptions.WarnRootCause = OptWarnRootCause;
  CcOptions.WarnAllRootCause = OptWarnAllRootCause;

#ifdef FIVE_C
  CcOptions.RemoveItypes = OptRemoveItypes;
  CcOptions.ForceItypes = OptForceItypes;
#endif

  //Add user specified function allocators
  std::string Malloc = OptMalloc.getValue();
  if (!Malloc.empty()) {
    std::string delimiter = ",";
    size_t pos = 0;
    std::string token;
    while ((pos = Malloc.find(delimiter)) != std::string::npos) {
      token = Malloc.substr(0, pos);
      CcOptions.AllocatorFunctions.push_back(token);
      Malloc.erase(0, pos + delimiter.length());
    }
    token = Malloc;
    CcOptions.AllocatorFunctions.push_back(token);
  }
  else
    CcOptions.AllocatorFunctions = {};

  // Create CConv Interface.
  _3CInterface _3CInterface(CcOptions,
                             OptionsParser.getSourcePathList(),
                             &(OptionsParser.getCompilations()));

  if (OptVerbose)
    errs() << "Calling Library to building Constraints.\n";
  // First build constraints.
  if (!_3CInterface.BuildInitialConstraints()) {
    errs() << "Failure occurred while trying to build constraints. Exiting.\n";
    return 1;
  }

  if (OptVerbose) {
    errs() << "Finished Building Constraints.\n";
    errs() << "Trying to solve Constraints.\n";
  }

  // Next solve the constraints.
  if (!_3CInterface.SolveConstraints(OptWarnRootCause)) {
    errs() << "Failure occurred while trying to solve constraints. Exiting.\n";
    return 1;
  }

  if (OptVerbose) {
    errs() << "Finished solving constraints.\n";
    errs() << "Trying to rewrite the converted files back.\n";
  }

  // Write all the converted files back.
  if (!_3CInterface.WriteAllConvertedFilesToDisk()) {
    errs() << "Failure occurred while trying to rewrite converted files back."
              "Exiting.\n";
    return 1;
  }

  return 0;
}
