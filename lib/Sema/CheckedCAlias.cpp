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
#include "clang/Sema/SemaDiagnostic.h"

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
    if (FieldDecl *UsedMember = dyn_cast<FieldDecl>(DR->getDecl())) {
      Context.addMemberBoundsUse(UsedMember, MemberWithBounds);
    }
    return true;
  }
};
}

namespace {
class CheckAddressTaken : public RecursiveASTVisitor<CheckAddressTaken> {
protected:

private:
  Sema &SemaRef;

 public:
  // Fields are stored in reverse order.
  typedef SmallVector<FieldDecl *,4> FieldPath;

  CheckAddressTaken(Sema &SemaRef) : SemaRef(SemaRef) {}

  bool ComputePathHelper(Expr *E, FieldPath &Path) {
    if (MemberExpr *ME = dyn_cast<MemberExpr>(E)) {
      ValueDecl *VD = ME->getMemberDecl();
      if (FieldDecl *FD = dyn_cast<FieldDecl>(VD)) {
        Path.push_back(FD);
        if (ME->isArrow()) {
          return true;
        }
        return ComputePathHelper(ME->getBase(), Path);
      } else {
        llvm_unreachable("unexpected member declaration");
        return false;
      }
    } else if (ParenExpr *PE = dyn_cast<ParenExpr>(E))
      return ComputePathHelper(PE->getSubExpr(), Path);
    else
      return true;
  }

  bool ComputePath(Expr *E, FieldPath &Path) {
    Path.clear();
    return ComputePathHelper(E, Path);
  }

  void CheckOperand(Expr *E, bool IsCheckedScope) {
    if (DeclRefExpr *DR = dyn_cast<DeclRefExpr>(E->IgnoreParens()))
      if (VarDecl *D = dyn_cast<VarDecl>(DR->getDecl()))
        if (D->getType()->isCheckedPointerType() &&
            D->hasBoundsExpr()) {
          SemaRef.Diag(E->getLocStart(),
                       diag::err_address_of_var_with_bounds) <<
            D <<
            E->getSourceRange();
          SemaRef.Diag(D->getBoundsExpr()->getLocStart(),
                       diag::note_var_bounds) <<
            D->getBoundsExpr()->getSourceRange();
          return;
        }

    FieldPath Path;
    if (!ComputePath(E, Path))
      // invalid member expression, bail out.
      return;
    if (Path.empty())
      // This is not the address of a member access.
      return;

    // TODO: handle cases involving nested members.  For now,
    // just look at the outermost member, which is the first
    // element of the path (i.e. given e.f.g,  look at g).

    // Taking the address of a member with checked pointer
    // type and bounds is not allowed.  It is allowed for
    // other cases, such s the member being an array or
    // the bounds for a bounds-safe interface.
    FieldDecl *Field = Path[0];
    if ((Field->getType()->isCheckedPointerType() ||
         Field->getType()->isIntegralType(SemaRef.getASTContext())) &&
        Field->hasBoundsExpr()) {
        SemaRef.Diag(E->getLocStart(),
                     diag::err_address_of_member_with_bounds) <<
        E->getSourceRange();
      SemaRef.Diag(Field->getBoundsExpr()->getLocStart(),
                   diag::note_member_bounds) <<
        Field->getBoundsExpr()->getSourceRange();
      }

    // Taking the address of a member used in a bounds expression is not
    // allowed.
    ASTContext &Context = SemaRef.getASTContext();
    ASTContext::member_bounds_iterator start = Context.using_member_bounds_begin(Field);
    ASTContext::member_bounds_iterator end = Context.using_member_bounds_end(Field);
    bool EmittedErrorMessage = false;
    for ( ; start != end; ++start) {
      const FieldDecl *MemberWithBounds = *start;
      QualType QT = MemberWithBounds->getType();
      // Always diagnose members in checked scopes.  For unchecked
      // scopes, diagnose members used in bounds for checked members.  Don't
      // diagnose bounds-safe interfaces.
      if (IsCheckedScope ||
          QT->isCheckedArrayType() ||
          QT->isCheckedPointerType() ||
          QT->isIntegralType(SemaRef.getASTContext())) {
        if (!EmittedErrorMessage) {
          SemaRef.Diag(E->getLocStart(), diag::err_address_of_member_in_bounds) << E->getSourceRange();
          EmittedErrorMessage = true;
        }
        SemaRef.Diag(MemberWithBounds->getLocStart(), diag::note_member_bounds) <<
          MemberWithBounds->getSourceRange();
      }
    }
  }

public:
  bool VisitUnaryOperator(UnaryOperator *UO) {
    if (UO->getOpcode() == UO_AddrOf)
      CheckOperand(UO->getSubExpr(), UO->getType()->isCheckedPointerType());
    return true;
  }
};
}

void Sema::CheckAddressTakenMembers(UnaryOperator *AddrOf) {
  CheckAddressTaken(*this).TraverseStmt(AddrOf);;

}
void Sema::TrackMemberBoundsDependences(FieldDecl *FD, BoundsExpr *BE) {
  if (BE)
    CollectBoundsMemberUses(FD, getASTContext()).TraverseStmt(BE);
}
