//=--3CCommands.h-------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Helper methods used to handle 3C commands.
//===----------------------------------------------------------------------===//

#ifdef INTERACTIVE3C
#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_3CCOMMANDS_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_3CCOMMANDS_H

#include "Protocol.h"
#include "clang/3C/3C.h"

namespace clang {
namespace clangd {
// Convert the provided Diagnostic into Commands
void As3CCommands(const Diagnostic &D, std::vector<Command> &OutCommands);
// Check if the execute command request from the client is a 3C command.
bool Is3CCommand(const ExecuteCommandParams &Params);

// Interpret the provided execute command request as 3C command
// and execute them.
bool Execute3CCommand(const ExecuteCommandParams &Params,
                      std::string &ReplyMessage, _3CInterface &CcInterface);
} // namespace clangd
} // namespace clang
#endif // LLVM_CLANG_TOOLS_EXTRA_CLANGD_3CCOMMANDS_H
#endif
