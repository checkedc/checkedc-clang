//
// Created by machiry on 11/9/19.
//

#include "CConvertCommands.h"
#include "CConvertDiagnostics.h"

namespace clang {
namespace clangd {
llvm::Optional<Command> asCCCommand(const Diagnostic &D, bool onlyThisPtr) {
  Command Cmd;
  unsigned long ptrID;
  if (getPtrIDFromDiagMessage(D, ptrID)) {
    CConvertManualFix ptrFix;
    ptrFix.ptrID = ptrID;
    Cmd.command = Command::CCONV_APPLY_FOR_ALL;
    Cmd.title = "Make this pointer non-WILD and apply the same observation to all the pointers.";
    if (onlyThisPtr) {
      Cmd.command = Command::CCONV_APPLY_ONLY_FOR_THIS;
      Cmd.title = "Make this pointer non-WILD ";
    }
    Cmd.ccConvertManualFix = ptrFix;
    return Cmd;
  }
  return None;
}

bool isCConvCommand(const ExecuteCommandParams &Params) {
    return (Params.command.rfind(Command::CCONV_APPLY_ONLY_FOR_THIS, 0) == 0) ||
           (Params.command.rfind(Command::CCONV_APPLY_FOR_ALL, 0) == 0);
}

bool applyCCCommand(const ExecuteCommandParams &Params, std::string &replyMessage) {
  replyMessage = "Checked C Pointer Modified.";
  if (Params.command.rfind(Command::CCONV_APPLY_ONLY_FOR_THIS, 0) == 0) {
    int ptrID = Params.ccConvertManualFix->ptrID;
    makeSinglePtrNonWild(ptrID);
    return true;
  }
  if (Params.command.rfind(Command::CCONV_APPLY_FOR_ALL, 0) == 0) {
    int ptrID = Params.ccConvertManualFix->ptrID;
    invalidateWildReasonGlobally(ptrID);
    return true;
  }
  return false;
}
}
}