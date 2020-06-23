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

#include <boost/config.hpp>
#include <boost/bimap.hpp>

// Class that maintains stats about how the bounds of various variables is
// computed.
class AVarBoundsStats {
public:
  // Found by using variables that start with same prefix as the corresponding
  // array variable.
  std::set<BoundsKey> NamePrefixMatch;
  // Found by using allocation sites.
  std::set<BoundsKey> AllocatorMatch;
  // Found by using variable names that match size related words.
  std::set<BoundsKey> VariableNameMatch;
  // Neighbour scalar parameter match.
  std::set<BoundsKey> NeighbourParamMatch;
  // These are dataflow matches i.e., matches found by dataflow analysis
  std::set<BoundsKey> DataflowMatch;
  AVarBoundsStats() {
    clear();
  }
  ~AVarBoundsStats() {
    clear();
  }
private:
  void clear() {
    NamePrefixMatch.clear();
    AllocatorMatch.clear();
    VariableNameMatch.clear();
    NeighbourParamMatch.clear();
    DataflowMatch.clear();
  }

};

typedef boost::bimap<PersistentSourceLoc, BoundsKey> DeclKeyBiMapType;
typedef DeclKeyBiMapType::value_type DeclMapItemType;
typedef boost::bimap<std::tuple<std::string, bool, unsigned>,
                     BoundsKey> ParmKeyBiMapType;
typedef ParmKeyBiMapType::value_type ParmMapItemType;

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

  void insertDeclaredBounds(clang::Decl *D, ABounds *B);
  bool mergeBounds(BoundsKey L, ABounds *B);
  bool removeBounds(BoundsKey L);
  bool replaceBounds(BoundsKey L, ABounds *B);
  ABounds *getBounds(BoundsKey L);

  // Try and get BoundsKey, into R, for the given declaration. If the declaration
  // does not have a BoundsKey then return false.
  bool getVariable(clang::Decl *D, BoundsKey &R);

  // Gets VarKey for various declarations.
  void insertVariable(clang::Decl *D);

  BoundsKey getVariable(clang::VarDecl *VD);

  BoundsKey getVariable(clang::ParmVarDecl *PVD);

  BoundsKey getVariable(clang::FieldDecl *FD);

  BoundsKey getConstKey(uint64_t value);

  bool getVariable(clang::Expr *E, const ASTContext &C, BoundsKey &R);

  bool addAssignment(clang::Decl *L, clang::Decl *R);

  bool addAssignment(clang::DeclRefExpr *L, clang::DeclRefExpr *R);

  bool addAssignment(BoundsKey L, BoundsKey R);

  // Get the ProgramVar for the provided VarKey.
  ProgramVar *getProgramVar(BoundsKey VK);

  AVarBoundsStats &getBStats() { return BoundsInferStats; }

private:
  // Variable that is used to generate new bound keys.
  BoundsKey BCount;
  // Map of VarKeys and corresponding program variables.
  std::map<BoundsKey, ProgramVar *> PVarInfo;
  // Map of APSInt (constants) and corresponding VarKeys.
  std::map<uint64_t, BoundsKey> ConstVarKeys;
  // Map of Persistent source loc and  corresponding bounds information.
  // Note that although each PSL could have multiple ConstraintKeys Ex: **p.
  // Only the outer most pointer can have bounds.
  std::map<BoundsKey, ABounds *> BInfo;
  // Set that contains BoundsKeys of variables which have invalid bounds.
  std::set<BoundsKey> InvalidBounds;
  // Set of BoundsKeys that correspond to pointers.
  std::set<BoundsKey> PointerBoundsKey;
  // Map of Persistent source loc and BoundsKey of regular variables.
  DeclKeyBiMapType DeclVarMap;
  // Map of parameter keys and BoundsKey for function parameters.
  ParmKeyBiMapType ParamDeclVarMap;
  // Graph of all program variables.
  AVarGraph ProgVarGraph;
  // Stats on techniques used to find length for various variables.
  AVarBoundsStats BoundsInferStats;

  bool hasVarKey(PersistentSourceLoc &PSL);

  BoundsKey getVarKey(PersistentSourceLoc &PSL);

  BoundsKey getVarKey(llvm::APSInt &API);

  void insertVarKey(PersistentSourceLoc &PSL, BoundsKey NK);

  void insertProgramVar(BoundsKey NK, ProgramVar *PV);
};

#endif // _AVARBOUNDSINFO_H
