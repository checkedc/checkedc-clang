//=--CheckedRegions.h---------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This class contains functions and classes that deal with
// adding the _Checked and _Unchecked annotations to code
// when enabled with the -addcr flag
//===----------------------------------------------------------------------===//

#ifndef _CHECKEDREGIONS_H
#define _CHECKEDREGIONS_H

#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/ASTContext.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "ProgramInfo.h"

using namespace clang;
using namespace llvm;

typedef enum { 
  IS_UNCHECKED,
  IS_CHECKED,
} AnnotationNeeded;

class CheckedRegionAdder : public RecursiveASTVisitor<CheckedRegionAdder>
{
  public:
    explicit CheckedRegionAdder(ASTContext *_C, Rewriter &_R,
        std::map<llvm::FoldingSetNodeID, AnnotationNeeded> &M)
      : Context(_C), Writer(_R), Map(M) {}

    bool VisitCompoundStmt(CompoundStmt *S);

  private:
    ASTContext* Context;
    Rewriter& Writer;
    std::map<llvm::FoldingSetNodeID, AnnotationNeeded> &Map;
};

class CheckedRegionFinder : public RecursiveASTVisitor<CheckedRegionFinder>
{
  public:
    explicit CheckedRegionFinder(ASTContext *_C, Rewriter &_R, ProgramInfo &_I,
                                std::set<FoldingSetNodeID> &S,
                                std::map<FoldingSetNodeID, AnnotationNeeded> &M)
      : Context(_C), Writer(_R), Info(_I), Seen(S), Map(M) {}
    int Nwild = 0;
    int Nchecked = 0;
    int Ndecls = 0;

    bool VisitForStmt(ForStmt *S);
    bool VisitSwitchStmt(SwitchStmt *S);
    bool VisitIfStmt(IfStmt *S);
    bool VisitWhileStmt(WhileStmt *S);
    bool VisitDoStmt(DoStmt *S);
    bool VisitCompoundStmt(CompoundStmt *S);
    bool VisitCStyleCastExpr(CStyleCastExpr *E);
    bool VisitUnaryOperator(UnaryOperator *U);
    bool VisitCallExpr(CallExpr *C);
    bool VisitVarDecl(VarDecl *VD);
    bool VisitParmVarDecl(ParmVarDecl *PVD);
    bool VisitMemberExpr(MemberExpr *E);




  private:
    void handleChildren(const Stmt::child_range &Stmts);
    void addUncheckedAnnotation(CompoundStmt *S, int LocalWild);
    bool isInStatementPosition(CallExpr *C);
    bool hasUncheckedParameters(CompoundStmt *S);
    bool isUncheckedPtr(QualType Qt);
    bool isUncheckedPtrAcc(QualType Qt, std::set<std::string> &Seen);
    bool isUncheckedStruct(QualType Qt, std::set<std::string> &Seen);
    bool isFunctionBody(CompoundStmt *S);

    ASTContext* Context;
    Rewriter& Writer;
    ProgramInfo& Info;
    std::set<FoldingSetNodeID>& Seen;
    std::map<FoldingSetNodeID, AnnotationNeeded>& Map;

};



#endif //_CHECKEDREGIONS_H
