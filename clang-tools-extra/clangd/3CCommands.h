//=--3CCommands.h-------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Helper methods used to handle CConv commands.
//===----------------------------------------------------------------------===//

#ifdef INTERACTIVECCCONV
#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_3CCOMMANDS_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_3CCOMMANDS_H

#include "Protocol.h"
#include "clang/3C/3C.h"

namespace clang {
namespace clangd {
  // Convert the provided Diagnostic into Commands
  void AsCCCommands(const Diagnostic &D, std::vector<Command> &OutCommands);
  // Check if the execute command request from the client is a CConv command.
  bool Is3CCommand(const ExecuteCommandParams &Params);

  // Interpret the provided execute command request as CConv command
  // and execute them.
  bool ExecuteCCCommand(const ExecuteCommandParams &Params,
                        std::string &ReplyMessage,
                        _3CInterface &CcInterface);
}
}
#endif //LLVM_CLANG_TOOLS_EXTRA_CLANGD_3CCOMMANDS_H
#endif
