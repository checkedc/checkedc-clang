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
    Cmd.title = "This is non-WILD and this is applicable to the entire project";
    if (onlyThisPtr) {
      Cmd.command = Command::CCONV_APPLY_ONLY_FOR_THIS;
      Cmd.title = "Make this pointer non-WILD ";
    }
    Cmd.ccConvertManualFix = ptrFix;
    return Cmd;
  }
  return None;
}

bool applyCCCommand(const ExecuteCommandParams &Params, std::string &replyMessage) {
  replyMessage = "Checked C Pointer Modified.";
  if (Params.command.rfind(Command::CCONV_APPLY_ONLY_FOR_THIS, 0) == 0) {
    return true;
  }
  if (Params.command.rfind(Command::CCONV_APPLY_FOR_ALL, 0) == 0) {
    return true;
  }
  return false;
}
}
}