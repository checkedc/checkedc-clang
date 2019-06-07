//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Type declarations for map data structures.
//===----------------------------------------------------------------------===//
#ifndef _UTILS_H
#define _UTILS_H
#include <set>
#include "llvm/Support/CommandLine.h"

#include "PersistentSourceLoc.h"

class ConstraintVariable;
class ProgramInfo;

// Maps a Decl to the set of constraint variables for that Decl.
typedef std::map<PersistentSourceLoc, 
  std::set<ConstraintVariable*>> VariableMap;

// Maps a Decl to the DeclStmt that defines the Decl.
typedef std::map<clang::Decl*, clang::DeclStmt*> VariableDecltoStmtMap;

extern llvm::cl::opt<bool> Verbose;
extern llvm::cl::opt<bool> DumpIntermediate;

const clang::Type *getNextTy(const clang::Type *Ty);

ConstraintVariable *getHighest(std::set<ConstraintVariable*> Vs, ProgramInfo &Info);

clang::FunctionDecl *getDeclaration(clang::FunctionDecl *FD);

clang::FunctionDecl *getDefinition(clang::FunctionDecl *FD);
#endif
