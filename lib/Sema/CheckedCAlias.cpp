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

//
// Check that code does not take the address of a member used in a member
// bounds expression.
//
// Note: we assume that casts that create pointers to members used in
// member bounds expression will be dealt with elsewhere.
//
// This is straightforward to implement when member bounds only directly
// use members defined by a struct.  We just look for address-of
// operators applied to a member access to a member used in a member bounds:
//
// struct S {
//   _Array_ptr<int> : count(len);
//   int len;
// };
//
// struct S sVar;
// &sVar.len;   // flag as error.
//
// However, in C, members of members may be accessed (this has the
// effect of creating a "narrower" lvalue that accesses only a slice
// of the original object).   We allow this in bounds expressions too.
//
// struct A {
//   int len;
// };
// struct B {
//   struct A myA;
//   _Array_ptr<int> r : count(myA.len);
// };
//
// struct A aVar;
// struct B bVar;
//
// This makes this more complex to implement: we want
// to disallow ONLY taking the addresses of members that are part of
// a structure object that uses those members in member bounds expressions.
// For example &bVar.myA.len is not allowed, but &aVar.len is allowed.
// The original simple algorithm won't work.  It would disallow the latter
// case.
//
// We use an algorithm that works in terms of paths instead. A path is
// a sequence of a member accesses that start at some lvalue expression with
// structure type.  For example, given e1.a.b.c, the sequence [a,b,c] is a
// path, as is [a,b] and [a].  Member bounds expressions may use paths
// that start at members (the base expression is implicit).
//
// For struct S, p uses the path [len].  For struct B, r uses the
// path [myA, len].  For struct B, we must ensure that program also does not
// take the address of a non-leaf member of a path (&myA).
// 
// Paths provide structurally-based aliasing information.  We can use paths
// to when an lvalue created by a member access might provide access (an alias)
// to a member of an object subject to a member bounds invariant.
//
// The algorithm has 3 steps:
// 1. For each member bounds expression, track what paths it depends upon.
// Only look at paths starting at members.  In ASTContext, there is a mapping
// from paths to members with bounds expression (we don't directly map to the
// bounds expressions).
//
// Given a path, also track dependences on subpaths (prefixes of the path). 
// Given a bounds expression v that uses the path a.b.c, track that v depends on
// a, a.b, and a.b.c.  This lets us detect when a program tries to take the
// address of an intermediate struct.
//
// When computing paths, disregard parentheses and bounds-safe interface
// casts.
//
// 2. At any address-of operation, check to see if it is an address of a
// member expression m.  Do this by computing a path expression p for
// its subexpression.
//
// 3. If there is a path expression, check that the expression is:
//   a. Not taking the address of a checked pointer with bounds.  We only
//      examine the leaf member of a, as the other members can't possibly
//      have a checked pointer type.
//   b. Not taking the address of a member path used in a member bounds.
//   The path p may be longer than the paths used in bounds.  This can
//   happen when a struct with bounds is embedded in another struct:
//
//   struct C {
//     struct B myB;
//   };
//   struct C cVar;
//   &cVar.myB.myA.len;
//
//   To handle this, we check p and all suffix subpaths of p to see if one
//   of those paths is used by a member bounds expression. If so, this
//   is not allowed.  In the example, we check len, myA.len, myA.myB.len,
//   and myC.myB.myA.len.   In this case, we discover that myA.len is used by
//   a bounds expression.
//
//   Note that paths that are shorter than the desired path are not considered
//   an error (the lvalue for that path is not involved in a member bounds 
//   invariant). For example:
//    struct A tmpA = Var.myBa.myA;
//    &tmpa.len
//
// One correctness concern is what happens when a computation of a path cuts off
// because of an indirection.   In that case, the lvalue produced by the indirection
// can't be (or have sub-objects) subject to member bounds invariants.  If it were,
// the program must have taken the address of an intermediate object.  That's also
// not allowed (casts that create such pointers will be dealt with elsewhere.)

namespace {
class Helper {
public:
  // Given a member access expression, compute the path of member accesses
  // for the expression.  The member are stored in reverse order. The
  // expression and base expressions may possibly be parenthesized.
  // For example, given a.b.c, this returns the path b.c.a.
  static bool ComputePathHelper(Expr *E, ASTContext::MemberPath &Path) {
    if (DeclRefExpr *DR = dyn_cast<DeclRefExpr>(E)) {
      // Checked C bounds expressions can directly reference a member.
      if (FieldDecl *UsedMember = dyn_cast<FieldDecl>(DR->getDecl()))
        Path.push_back(UsedMember->getCanonicalDecl());
      return true;
    } else if (MemberExpr *ME = dyn_cast<MemberExpr>(E)) {
      ValueDecl *VD = ME->getMemberDecl();
      if (FieldDecl *FD = dyn_cast<FieldDecl>(VD)) {
        Path.push_back(FD->getCanonicalDecl());
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
    else if (ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(E)) {
      if (ICE->getCastKind() == CK_LValueBitCast &&
          ICE->isBoundsSafeInterface())
        return ComputePathHelper(ICE->getSubExpr(), Path);
      else
        return true;
    } else
      return true;
  }

  static bool ComputePath(Expr *E, ASTContext::MemberPath &Path) {
    Path.clear();
    return ComputePathHelper(E, Path);
  }

  static void DumpPath(ASTContext::MemberPath &Path) {
    for (int i = 0; i < Path.size(); i++) {
        if (i >> 0)
          llvm::outs() << ' ';
        llvm::outs() << '(' << Path[i]->getName() << ')';
    }
    llvm::outs() << "\n";
  }

  static VarDecl *ComputeVar(Expr *E) {
    if (DeclRefExpr *DR = dyn_cast<DeclRefExpr>(E))
      return dyn_cast<VarDecl>(DR->getDecl());
    else if (ParenExpr *PE = dyn_cast<ParenExpr>(E))
      return ComputeVar(PE->getSubExpr());
    else if (ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(E)) {
      if (ICE->getCastKind() == CK_LValueBitCast &&
          ICE->isBoundsSafeInterface())
        return ComputeVar(ICE->getSubExpr());
      else
        return nullptr;
    } else
      return nullptr;
  }
};
}

// When member bounds are declared for a member, collect the dependencies on member
// paths (step 1).
namespace {
class CollectBoundsMemberUses : public RecursiveASTVisitor<CollectBoundsMemberUses> {
private:
  FieldDecl *MemberWithBounds;
  ASTContext &Context;

public:
  CollectBoundsMemberUses(FieldDecl *MemberWithBounds, ASTContext &Context) :
    MemberWithBounds(MemberWithBounds), Context(Context) {
  }

  void Analyze(Expr *E) {
    ASTContext::MemberPath Path;
    if (Helper::ComputePath(E, Path))
      if (Path.size() > 0)
        Context.addMemberBoundsUse(Path, MemberWithBounds);
  }

  bool VisitMemberExpr(MemberExpr *ME) {
    Analyze(ME);
    return true;

  }
  bool VisitDeclRefExpr(DeclRefExpr *DR) {
    Analyze(DR);
    return true;
  }
};
}

// Check that members used in member bounds or with member bounds are not
// address-taken (steps 2 and 3).
namespace {
class CheckAddressTaken : public RecursiveASTVisitor<CheckAddressTaken> {
protected:

private:
  Sema &SemaRef;

 public:
  CheckAddressTaken(Sema &SemaRef) : SemaRef(SemaRef) {}

  void CheckOperand(Expr *E, bool IsCheckedScope) {
    // Simple check for variables with bounds.
    if (E->getType()->isCheckedPointerType())
      if (VarDecl *D = Helper::ComputeVar(E)) {
        if (D->hasBoundsExpr()) {
          SemaRef.Diag(E->getLocStart(),
                       diag::err_address_of_var_with_bounds) <<
            D <<
            E->getSourceRange();
          SemaRef.Diag(D->getBoundsExpr()->getLocStart(),
                       diag::note_var_bounds) <<
            D->getBoundsExpr()->getSourceRange();
          return;
        }
      }

    ASTContext::MemberPath Path;
    if (!Helper::ComputePath(E, Path))
      // invalid member expression, bail out.
      return;
    if (Path.empty())
      // This is not the address of a member access.
      return;

    // Taking the address of a member with checked pointer
    // type and bounds is not allowed.  It is allowed for
    // other cases, such s the member being an array or
    // the bounds for a bounds-safe interface.
    const FieldDecl *Field = Path[0];
    QualType FieldTy = Field->getType();
    if (Field->hasBoundsExpr()) {
      if (FieldTy->isCheckedPointerType() ||
          FieldTy->isIntegralType(SemaRef.getASTContext()) ||
          (IsCheckedScope && FieldTy->isUncheckedPointerType())) {
        SemaRef.Diag(E->getLocStart(),
                     diag::err_address_of_member_with_bounds) <<
        E->getSourceRange();
        SemaRef.Diag(Field->getBoundsExpr()->getLocStart(),
                     diag::note_member_bounds) <<
          Field->getBoundsExpr()->getSourceRange();
      }
    }

    // Given a member path, see if any of its suffix member paths are used in a
    // member bounds expression   Given a.b.c see if c, b.c, or a.b.c
    // are used in bounds expressions.
    for (int i = 1; i <= Path.size(); i++) {
       // Paths are stored in reverse order, so we can just copy
       // the path and truncate it.
      ASTContext::MemberPath SuffixPath(Path);
      SuffixPath.set_size(i);
      // Taking the address of a member used in a bounds expression is not
      // allowed.
      ASTContext &Context = SemaRef.getASTContext();
      ASTContext::member_bounds_iterator start = Context.using_member_bounds_begin(SuffixPath);
      ASTContext::member_bounds_iterator end = Context.using_member_bounds_end(SuffixPath);
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
            MemberWithBounds->getBoundsExpr()->getSourceRange();
        }
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
