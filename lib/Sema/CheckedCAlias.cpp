//===----------- CheckedCAlias - Checked C alias rules checking  ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements alias restrictions required by the Checked C
//  language extension.
//
//===----------------------------------------------------------------------===//


#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Sema/Sema.h"

using namespace clang;
using namespace sema;

namespace {
class CollectBoundsMemberUses : public RecursiveASTVisitor<CollectBoundsMemberUses> {
private:
  FieldDecl *MemberWithBounds;
  ASTContext &Context;

public:
  CollectBoundsMemberUses(FieldDecl *MemberWithBounds, ASTContext &Context) :
    MemberWithBounds(MemberWithBounds), Context(Context) {
  }

  bool VisitDeclRefExpr(DeclRefExpr *DR) {
    if (FieldDecl *UsedMember = dyn_cast<FieldDecl>(DR->getDecl()))
      Context.addMemberBoundsUse(UsedMember, MemberWithBounds);
    return true;
  }
};
}

void Sema::TrackMemberBoundsDependences(FieldDecl *FD, BoundsExpr *BE) {
  if (BE)
    CollectBoundsMemberUses(FD, getASTContext()).VisitExpr(BE);
}

