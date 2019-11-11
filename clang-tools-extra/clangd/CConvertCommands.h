//
// Created by machiry on 11/9/19.
//

#ifndef LLVM_TOOLS_CLANG_TOOLS_EXTRA_CLANGD_CCONVERTCOMMANDS_H
#define LLVM_TOOLS_CLANG_TOOLS_EXTRA_CLANGD_CCONVERTCOMMANDS_H

#include "Protocol.h"

namespace clang {
namespace clangd {
  llvm::Optional<Command> asCCCommand(const Diagnostic &D, bool onlyThisPtr = false);
  bool applyCCCommand(const ExecuteCommandParams &Params, std::string &replyMessage);
}
}
#endif //LLVM_TOOLS_CLANG_TOOLS_EXTRA_CLANGD_CCONVERTCOMMANDS_H
