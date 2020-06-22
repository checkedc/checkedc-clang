//=--AVarBoundsInfo.h---------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the bounds information about various ARR atoms.
//
//===----------------------------------------------------------------------===//
#ifndef _AVARBOUNDSINFO_H
#define _AVARBOUNDSINFO_H

#include "ABounds.h"
#include "AVarGraph.h"
#include "ProgramVar.h"
#include "clang/AST/Decl.h"
#include "clang/CConv/PersistentSourceLoc.h"

class AVarBoundsInfo {
public:
  AVarBoundsInfo() {
    BCount = 1;
    PVarInfo.clear();
    BInfo.clear();
    DeclVarMap.clear();
    ProgVarGraph.clear();
  }

  // Checks if the given declaration is a valid bounds variable.
  bool isValidBoundVariable(clang::Decl *D);
  void insertBounds(clang::Decl *D, ABounds *B);

  // Try and get BoundsKey, into R, for the given declaration. If the declaration
  // does not have a BoundsKey then return false.
  bool getVariable(clang::Decl *D, BoundsKey &R);

  // Gets VarKey for various declarations.
  void insertVariable(clang::Decl *D);

  BoundsKey getVariable(clang::VarDecl *VD);

  BoundsKey getVariable(clang::ParmVarDecl *PVD);

  BoundsKey getVariable(clang::FieldDecl *FD);

  bool getVariable(clang::Expr *E, const ASTContext &C, BoundsKey &R);

  bool addAssignment(clang::Decl *L, clang::Decl *R);

  bool addAssignment(clang::DeclRefExpr *L, clang::DeclRefExpr *R);

  bool addAssignment(BoundsKey L, BoundsKey R);

  // Get the ProgramVar for the provided VarKey.
  ProgramVar *getProgramVar(BoundsKey VK);

private:
  // Variable that is used to generate new bound keys.
  BoundsKey BCount;
  // Map of VarKeys and corresponding program variables.
  std::map<BoundsKey, ProgramVar *> PVarInfo;
  // Map of APSInt (constants) and corresponding VarKeys.
  std::map<llvm::APSInt, BoundsKey> ConstVarKeys;
  // Map of Persistent source loc and  corresponding bounds information.
  // Note that although each PSL could have multiple ConstraintKeys Ex: **p.
  // Only the outer most pointer can have bounds.
  std::map<BoundsKey, ABounds *> BInfo;
  // Map of Persistent source loc and BoundsKey of regular variables.
  std::map<PersistentSourceLoc, BoundsKey> DeclVarMap;
  // Map of parameter keys and BoundsKey for function parameters.
  std::map<std::tuple<std::string, bool, unsigned>, BoundsKey> ParamDeclVarMap;
  // Graph of all program variables.
  AVarGraph ProgVarGraph;

  bool hasVarKey(PersistentSourceLoc &PSL);

  BoundsKey getVarKey(PersistentSourceLoc &PSL);

  BoundsKey getVarKey(llvm::APSInt &API);

  void insertVarKey(PersistentSourceLoc &PSL, BoundsKey NK);

  void insertProgramVar(BoundsKey NK, ProgramVar *PV);
};

#endif // _AVARBOUNDSINFO_H
