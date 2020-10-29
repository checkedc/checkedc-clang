//=--3CCommands.cpp-----------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of CConv command helper methods.
//===----------------------------------------------------------------------===//

#ifdef INTERACTIVECCCONV
#include "3CCommands.h"

namespace clang {
namespace clangd {

#define CCONVSOURCE "CConv_RealWild"

static bool GetPtrIDFromDiagMessage(const Diagnostic &DiagMsg,
                                    unsigned long &PtrId) {
  if (DiagMsg.source.rfind(CCONVSOURCE, 0) == 0) {
    PtrId = atoi(DiagMsg.code.c_str());
    return true;
  }
  return false;
}

void AsCCCommands(const Diagnostic &D, std::vector<Command> &OutCommands) {
  unsigned long PtrId;
  if (GetPtrIDFromDiagMessage(D, PtrId)) {
    Command AllPtrsCmd;
    CConvertManualFix PtrFix;
    PtrFix.ptrID = PtrId;
    AllPtrsCmd.ccConvertManualFix = PtrFix;
    Command SinglePtrCmd = AllPtrsCmd;

    AllPtrsCmd.command = Command::CCONV_APPLY_FOR_ALL;
    AllPtrsCmd.title = "Make this pointer non-WILD and apply the "
                       "same observation to all the pointers.";

    OutCommands.push_back(AllPtrsCmd);

    SinglePtrCmd.command = Command::CCONV_APPLY_ONLY_FOR_THIS;
    SinglePtrCmd.title = "Make ONLY this pointer non-WILD.";

    OutCommands.push_back(SinglePtrCmd);
  }
}

bool IsCConvCommand(const ExecuteCommandParams &Params) {
    return (Params.command.rfind(Command::CCONV_APPLY_ONLY_FOR_THIS, 0) == 0) ||
           (Params.command.rfind(Command::CCONV_APPLY_FOR_ALL, 0) == 0);
}

bool ExecuteCCCommand(const ExecuteCommandParams &Params,
                    std::string &ReplyMessage,
                    CConvInterface &CcInterface) {
  ReplyMessage = "Checked C Pointer Modified.";
  if (Params.command.rfind(Command::CCONV_APPLY_ONLY_FOR_THIS, 0) == 0) {
    int PtrId = Params.ccConvertManualFix->ptrID;
    CcInterface.MakeSinglePtrNonWild(PtrId);
    return true;
  }
  if (Params.command.rfind(Command::CCONV_APPLY_FOR_ALL, 0) == 0) {
    int PtrId = Params.ccConvertManualFix->ptrID;
    CcInterface.InvalidateWildReasonGlobally(PtrId);
    return true;
  }
  return false;
}
}
}
#endif