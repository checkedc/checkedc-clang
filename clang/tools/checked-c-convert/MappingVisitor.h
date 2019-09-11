//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// The MappingVisitor is used to traverse an AST and re-define a mapping from
// PersistendSourceLocations to "live" AST objects. This is needed to support
// multi-compilation unit analyses, where after each compilation unit is 
// analyzed, the state of the analysis is "shelved" and all references to AST
// data structures are replaced with data structures that survive the clang
// constructed AST.
//===----------------------------------------------------------------------===//
#ifndef _MAPPING_VISITOR_H
#define _MAPPING_VISITOR_H
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include "Utils.h"
#include "PersistentSourceLoc.h"

class MappingVisitor
  : public clang::RecursiveASTVisitor<MappingVisitor> {
public:
  MappingVisitor(std::set<PersistentSourceLoc> S, clang::ASTContext &C) : 
    SourceLocs(S),Context(C) {}

  // TODO: It's possible the Type field in this tuple isn't needed.
  typedef std::tuple<clang::Stmt*, clang::Decl*, clang::Type*> 
    StmtDeclOrType;

  bool VisitDeclStmt(clang::DeclStmt *S);

  bool VisitDecl(clang::Decl *D);

  std::pair<std::map<PersistentSourceLoc, StmtDeclOrType>, 
    VariableDecltoStmtMap>
  getResults() 
  {
    return std::pair<std::map<PersistentSourceLoc, StmtDeclOrType>, 
      VariableDecltoStmtMap>(PSLtoSDT, DeclToDeclStmt);
  }

private:
  // A map from a PersistentSourceLoc to a tuple describing a statement, decl
  // or type.
  std::map<PersistentSourceLoc, StmtDeclOrType> PSLtoSDT;
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
