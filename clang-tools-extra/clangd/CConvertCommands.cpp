//=--CConvertCommands.cpp-----------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of CConv command helper methods.
//===----------------------------------------------------------------------===//

#ifdef INTERACTIVECCCONV
#include "CConvertCommands.h"

namespace clang {
namespace clangd {

#define CCONVSOURCE "CConv_RealWild"

static bool GetPtrIDFromDiagMessage(const Diagnostic &diagMsg,
                                    unsigned long &ptrID) {
  if (diagMsg.source.rfind(CCONVSOURCE, 0) == 0) {
    ptrID = atoi(diagMsg.code.c_str());
    return true;
  }
  return false;
}

void AsCCCommands(const Diagnostic &D, std::vector<Command> &OutCommands) {
  unsigned long ptrID;
  if (GetPtrIDFromDiagMessage(D, ptrID)) {
    Command allPtrsCmd;
    CConvertManualFix ptrFix;
    ptrFix.ptrID = ptrID;
    allPtrsCmd.ccConvertManualFix = ptrFix;
    Command singlePtrCmd = allPtrsCmd;

    allPtrsCmd.command = Command::CCONV_APPLY_FOR_ALL;
    allPtrsCmd.title = "Make this pointer non-WILD and apply the "
                       "same observation to all the pointers.";

    OutCommands.push_back(allPtrsCmd);

    singlePtrCmd.command = Command::CCONV_APPLY_ONLY_FOR_THIS;
    singlePtrCmd.title = "Make ONLY this pointer non-WILD.";

    OutCommands.push_back(singlePtrCmd);
  }
}

bool IsCConvCommand(const ExecuteCommandParams &Params) {
    return (Params.command.rfind(Command::CCONV_APPLY_ONLY_FOR_THIS, 0) == 0) ||
           (Params.command.rfind(Command::CCONV_APPLY_FOR_ALL, 0) == 0);
}

bool ExecuteCCCommand(const ExecuteCommandParams &Params,
                    std::string &replyMessage,
                    CConvInterface &ccInterface) {
  replyMessage = "Checked C Pointer Modified.";
  if (Params.command.rfind(Command::CCONV_APPLY_ONLY_FOR_THIS, 0) == 0) {
    int ptrID = Params.ccConvertManualFix->ptrID;
    ccInterface.MakeSinglePtrNonWild(ptrID);
    return true;
  }
  if (Params.command.rfind(Command::CCONV_APPLY_FOR_ALL, 0) == 0) {
    int ptrID = Params.ccConvertManualFix->ptrID;
    ccInterface.InvalidateWildReasonGlobally(ptrID);
    return true;
  }
  return false;
}
}
}
#endif