//=--CtxSensAVarBounds.h------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file handles context sensitive infrastructure for array bounds
// inference.
//
//===----------------------------------------------------------------------===//

#ifndef _CTXSENSAVARBOUNDS_H
#define _CTXSENSAVARBOUNDS_H

#include "ABounds.h"
#include "AVarGraph.h"
#include "ProgramVar.h"
#include "clang/AST/Decl.h"
#include "clang/3C/PersistentSourceLoc.h"
#include "clang/3C/ConstraintVariables.h"

class ProgramInfo;
class ConstraintResolver;

class AVarBoundsInfo;
typedef std::map<std::string, std::map<BoundsKey, BoundsKey>> CtxStKeyMap;
typedef std::map<PersistentSourceLoc, std::map<BoundsKey, BoundsKey>> CtxCEKeyMap;

// This handles handles all the context-sensitive portions of array bounds
// inference.
class CtxSensitiveBoundsKeyHandler {
public:
  CtxSensitiveBoundsKeyHandler(AVarBoundsInfo *ABInfo) : ABI(ABInfo) {
    clearAll();
  }
  virtual ~CtxSensitiveBoundsKeyHandler() {
    clearAll();
  }
  void insertCtxSensBoundsKey(ProgramVar *OldPV, BoundsKey NK,
                              const ProgramVarScope *NPS);

  // Create context sensitive bounds key with scope NPS for OK bounds key
  // and store the newly created bounds key in CBMap.
  void createCtxSensBoundsKey(BoundsKey OK,
                              const ProgramVarScope *NPS,
                              std::map<BoundsKey, BoundsKey> &CBMap);
  // Create context sensitive BoundsKey for struct member access.
  void contextualizeCVar(MemberExpr *ME,
                         ASTContext *C,
                         ProgramInfo &I);

  void contextualizeCVar(VarDecl *VD,
                         ASTContext *C,
                         ProgramInfo &I);

  // Create context sensitive BoundsKey variables for the given set of
  // ConstraintVariables for the CE call expression.
  void contextualizeCVar(CallExpr *CE,
                         const std::set<ConstraintVariable *> &CV,
                         ASTContext *C);

  // Get context sensitive bounds key for field access identified by
  // ME and store the bounds key in CSKey.
  // This function return true if success.
  bool tryGetMECSKey(MemberExpr *ME, ASTContext *C,
                     ProgramInfo &I, BoundsKey &CSKey);
  // Get context sensitive bounds key for a given FieldDecl,
  bool tryGetFieldCSKey(FieldDecl *FD, CtxStKeyMap *CSK,
                        const std::string &AK, ASTContext *C,
                        ProgramInfo &I, BoundsKey &CSKey);

  // Get context sensitive bounds key for BK at PSL.
  BoundsKey getCtxSensCEBoundsKey(const PersistentSourceLoc &PSL,
                                  BoundsKey BK);

  // Handle context sensitive assignment (from R to L) to a variable.
  bool handleContextSensitiveAssignment(const PersistentSourceLoc &PSL,
                                        clang::Decl *L,
                                        ConstraintVariable *LCVar,
                                        clang::Expr *R, CVarSet &RCVars,
                                        const std::set<BoundsKey> &CSRKeys,
                                        ASTContext *C, ConstraintResolver *CR);

  CtxStKeyMap *getCtxStKeyMap(bool IsGlobal);
private:
  void clearAll() {
    CSBoundsKey.clear();
    LocalMEBoundsKey.clear();
    GlobalMEBoundsKey.clear();
  }

  // Get the map (i.e., local or global) where the context-sensitive
  // bounds key for ME should be stored.
  CtxStKeyMap *getCtxStKeyMap(MemberExpr *ME, ASTContext *C);

  void contextualizeStructRecord(ProgramInfo &I,
                                 ASTContext *C,
                                 const RecordDecl *RD, const std::string &AK,
                                 std::map<BoundsKey, BoundsKey> &BKMap, bool IsGlobal);

  // Get string that represents a context sensitive key for the struct
  // member access ME.
  std::string getCtxStructKey(MemberExpr *ME, ASTContext *C);

  // For the given expression E, get all the bounds key and store them in
  // AllKeys. This function returns true on success.
  bool deriveBoundsKeys(clang::Expr *E,
                        const CVarSet &CVars,
                        ASTContext *C,
                        ConstraintResolver *CR,
                        std::set<BoundsKey> &AllKeys);

  // Context sensitive bounds key.
  // For each call-site a map of original bounds key and the bounds key
  // specific to this call-site.
  CtxCEKeyMap CSBoundsKey;
  // Context-sensitive keys for member access of a function local
  // struct variable.
  CtxStKeyMap LocalMEBoundsKey;
  // Context-sensitive keys for member access of a global struct variable.
  CtxStKeyMap GlobalMEBoundsKey;
  AVarBoundsInfo *ABI;
};

// This class creates context sensitive bounds key information that is
// useful to resolve certain bounds information.
// We create context-sensitive bounds key for:
// function parameters/return values/structure member access.
// Consider the following example:
// _Arry_ptr<int> foo(unsigned int s) : count(s);
// ....
// int *a, *c;
// unsigned b, d;
// a = foo(b);
// c = foo(d);
// ...
// Here, when we do our analysis we do not know whether b or d is the bounds
// of a.
// The reason for this is because we maintain a single bounds variable for foo,
// consequently, when we do our flow analysis we see that b and d both propagate
// to s (which is the bounds of the return value of foo).
// However, if we maintain context sensitive bounds keys, then we know that
// at a = foo(b), it is b that is passed to s and there by helps us infer that
// the bounds of a should be b i.e., _Array_ptr<a> : count(b).
// This class helps in maintaining the context sensitive bounds information.
class ContextSensitiveBoundsKeyVisitor :
  public RecursiveASTVisitor<ContextSensitiveBoundsKeyVisitor> {
public:
  explicit ContextSensitiveBoundsKeyVisitor(ASTContext *C, ProgramInfo &I,
                                            ConstraintResolver *CResolver) :
    Context(C), Info(I),
    CR(CResolver) { }

  virtual ~ContextSensitiveBoundsKeyVisitor() { }

  bool VisitCallExpr(CallExpr *CE);

  bool VisitMemberExpr(MemberExpr *ME);

  bool VisitDeclStmt(DeclStmt *DS);

private:
  ASTContext *Context;
  ProgramInfo &Info;
  ConstraintResolver *CR;
};

#endif //_CTXSENSAVARBOUNDS_H