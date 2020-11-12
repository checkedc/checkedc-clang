//=--MappingVisitor.h---------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// The MappingVisitor is used to traverse an AST and re-define a mapping from
// PersistentSourceLocations to "live" AST objects. This is needed to support
// multi-compilation unit analyses, where after each compilation unit is
// analyzed, the state of the analysis is "shelved" and all references to AST
// data structures are replaced with data structures that survive the clang
// constructed AST.
//===----------------------------------------------------------------------===//

#ifndef _MAPPING_VISITOR_H
#define _MAPPING_VISITOR_H
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include "PersistentSourceLoc.h"
#include "Utils.h"

typedef std::tuple<clang::Stmt *, clang::Decl *> StmtDecl;
typedef std::map<PersistentSourceLoc, StmtDecl> SourceToDeclMapType;
typedef std::pair<SourceToDeclMapType, VariableDecltoStmtMap>
    MappingResultsType;

class MappingVisitor : public clang::RecursiveASTVisitor<MappingVisitor> {
public:
  MappingVisitor(std::set<PersistentSourceLoc> S, clang::ASTContext &C)
      : SourceLocs(S), Context(C) {}

  bool VisitDeclStmt(clang::DeclStmt *S);

  bool VisitDecl(clang::Decl *D);

  MappingResultsType getResults() {
    return std::pair<std::map<PersistentSourceLoc, StmtDecl>,
                     VariableDecltoStmtMap>(PSLtoSDT, DeclToDeclStmt);
  }

private:
  // A map from a PersistentSourceLoc to a tuple describing a statement, decl
  // or type.
  SourceToDeclMapType PSLtoSDT;
  // The set of PersistentSourceLoc's this instance of MappingVisitor is tasked
  // with re-instantiating as either a Stmt, Decl or Type.
  std::set<PersistentSourceLoc> SourceLocs;
  // The ASTContext for the particular AST that the MappingVisitor is
  // traversing.
  clang::ASTContext &Context;
  // A mapping of individual Decls to the DeclStmt that contains them.
  VariableDecltoStmtMap DeclToDeclStmt;
};

#endif
