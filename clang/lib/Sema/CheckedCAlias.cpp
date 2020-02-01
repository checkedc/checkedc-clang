//===----------- CheckedCAlias - Checked C alias rules checking  ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements analyses for semantic checking of the Checked C
//  language extension.
// - Alias restrictions required by the Checked C language extension.
// - Computing what bounds expressions use variables modified by an assignment
//   or increment/decrement expression.
//
//===----------------------------------------------------------------------===//

// #define DEBUG_DEPENDENCES 1

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
// One correctness concern is what happens when a computation of a path cuts
// off because of an indirection.   In that case, the lvalue produced by the
// indirection can't be (or have sub-objects) subject to member bounds
// invariants.  If it were, the program must have taken the address of an
// intermediate object.  That's also not allowed (casts that create such
// pointers will be dealt with elsewhere.)

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
    for (unsigned i = 0; i < Path.size(); i++) {
        if (i >> 0)
          llvm::outs() << ' ';
        llvm::outs() << '(' << Path[i]->getName() << ')';
    }
    llvm::outs() << "\n";
  }

  static Expr *SimplifyLValue(Expr *E) {
    if (ParenExpr *PE = dyn_cast<ParenExpr>(E))
      return SimplifyLValue(PE->getSubExpr());
    else if (ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(E)) {
      if (ICE->getCastKind() == CK_LValueBitCast &&
          ICE->isBoundsSafeInterface())
        return SimplifyLValue(ICE->getSubExpr());
      else
        return E;
    } else
      return E;
  }

  static VarDecl *ComputeVar(Expr *E) {
    Expr *Simplified = SimplifyLValue(E);
    if (DeclRefExpr *DR = dyn_cast<DeclRefExpr>(Simplified))
      return dyn_cast<VarDecl>(DR->getDecl());
    else
      return nullptr;
  }
};
}

// When member bounds are declared for a member, collect the dependencies on
// member paths (step 1).
namespace {
class CollectBoundsMemberUses :
  public RecursiveASTVisitor<CollectBoundsMemberUses> {
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
          SemaRef.Diag(E->getBeginLoc(),
                       diag::err_address_of_var_with_bounds) <<
            D <<
            E->getSourceRange();
          SemaRef.Diag(D->getBoundsExpr()->getBeginLoc(),
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
    // other cases, such as the member being an array or
    // the bounds for a bounds-safe interface.
    ASTContext &Context = SemaRef.getASTContext();
    const FieldDecl *Field = Path[0];
    if (Field->getBoundsExpr() && !Field->getType()->isArrayType() &&
        (IsCheckedScope || !Field->hasBoundsSafeInterface(Context))) {
      SemaRef.Diag(E->getBeginLoc(), diag::err_address_of_member_with_bounds) <<
        E->getSourceRange();
      SemaRef.Diag(Field->getBoundsExpr()->getBeginLoc(),
                   diag::note_member_bounds) <<
        Field->getBoundsExpr()->getSourceRange();
    }

    // Given a member path, see if any of its suffix member paths are used in a
    // member bounds expression   Given a.b.c see if c, b.c, or a.b.c
    // are used in bounds expressions.
    for (unsigned i = 1; i <= Path.size(); i++) {
       // Paths are stored in reverse order, so we can just copy
       // the path and truncate it.
      ASTContext::MemberPath SuffixPath(Path);
      SuffixPath.set_size(i);
      // Taking the address of a member used in a bounds expression is not
      // allowed.

      auto start = Context.using_member_bounds_begin(SuffixPath);
      auto end = Context.using_member_bounds_end(SuffixPath);
      bool EmittedErrorMessage = false;
      for ( ; start != end; ++start) {
        const FieldDecl *MemberWithBounds = *start;
        // Always diagnose members in checked scopes.  For unchecked
        // scopes, diagnose members used in bounds for checked members.  Don't
        // diagnose bounds-safe interfaces.
        if (IsCheckedScope || MemberWithBounds->hasBoundsDeclaration(Context)) {
          if (!EmittedErrorMessage) {
            SemaRef.Diag(E->getBeginLoc(),
                         diag::err_address_of_member_in_bounds) <<
              E->getSourceRange();
            EmittedErrorMessage = true;
          }
          SemaRef.Diag(MemberWithBounds->getBeginLoc(),
                       diag::note_member_bounds) <<
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

namespace {
// Update map from variables to bounds expressions that use those variables.
class UpdateDependences : public RecursiveASTVisitor<UpdateDependences> {
private:
   bool Add;              // whether to add or remove dependence.
   VarDecl *const BoundsDecl;   // variable with bounds declaration.
   Sema::BoundsDependencyTracker::DependentMap &Map;

   void AddDependence(VarDecl *D) {
    if (BoundsDecl == D)   // Do not add self-dependences.
      return;
    auto I = Map.find(D);
    if (I == Map.end()) {
      Map[D].push_back(BoundsDecl);
      return;
     }

    auto VecIter = I->second.begin();
    auto VecEnd = I->second.end();

    for ( ; VecIter != VecEnd; VecIter++)
      if (*VecIter == BoundsDecl)
        return;

    I->second.push_back(BoundsDecl);
  }

   void RemoveDependence(VarDecl *D) {
     if (D == BoundsDecl)    // Self-dependences aren't allowed.
       return;

     auto I = Map.find(D);
     if (I == Map.end())
       return;

     auto VecIter = I->second.begin();
     auto VecEnd = I->second.end();

     for ( ; VecIter != VecEnd; VecIter++)
       if (*VecIter == BoundsDecl) {
         I->second.erase(VecIter);
         if (I->second.empty())
           Map.erase(I);
         return;
       }

    return;
   }

public:
  UpdateDependences(VarDecl *BoundsDecl,
                    Sema::BoundsDependencyTracker::DependentMap &Map,
                    bool Add) :
    Add(Add), BoundsDecl(BoundsDecl), Map(Map) {}

  bool VisitDeclRefExpr(DeclRefExpr *DR) {
    if (VarDecl *D = dyn_cast<VarDecl>(DR->getDecl())) {
       if (Add)
         AddDependence(D);
       else
         RemoveDependence(D);
    }

    return true;
  }
};
}

void Sema::BoundsDependencyTracker::Add(VarDecl *D) {
  BoundsExpr *BE = D->getBoundsExpr();
  if (!BE)
    return;
  BoundsInScope.push_back(D);
  UpdateDependences(D, Map, true).TraverseStmt(BE);
}

unsigned Sema::BoundsDependencyTracker::EnterScope() {
  return BoundsInScope.size();
}

void Sema::BoundsDependencyTracker::ExitScope(unsigned scopeBegin) {
  while (BoundsInScope.size() > scopeBegin) {
    VarDecl *D = BoundsInScope.back();
    BoundsExpr *BE = D->getBoundsExpr();
    UpdateDependences(D, Map, false).TraverseStmt(BE);
    BoundsInScope.pop_back();
  }
}

void Sema::BoundsDependencyTracker::Dump(raw_ostream &OS) {
  OS << "\nBounds declarations in scope:\n";
  for (unsigned i = 0; i < BoundsInScope.size(); i++)
    BoundsInScope[i]->dump(OS);
  OS << "\nMap from variables to variables w/ bounds declarations:\n";
  for (auto Iter = Map.begin(); Iter != Map.end(); ++Iter) {
    OS << Iter->first->getDeclName() << ": ";
    bool first = true;
    OS << "{";
    for (auto VarIter = Iter->second.begin(); VarIter != Iter->second.end();
         ++VarIter) {
      if (first)
        first = false;
      else
        OS << ",";
      OS << (*VarIter)->getDeclName();
    }
    OS << "}\n";
  }
}

/// \brief Track that E modifies an lvalue expression used in Bounds.
void Sema::ModifiedBoundsDependencies::Add(Expr *E,
                            llvm::PointerUnion<VarDecl *, MemberExpr *> LValue,
                                           BoundsExpr *Bounds) {
    LValueWithBounds Pair(LValue, Bounds);
    auto I = Tracker.find(E);
    if (I == Tracker.end()) {
      Tracker[E].push_back(Pair);
      return;
    }

    auto VecIter = I->second.begin();
    auto VecEnd = I->second.end();

    // Don't add the LValue/Bounds if they already on the list for E.
    for ( ; VecIter != VecEnd; VecIter++)
      if (VecIter->Bounds == Pair.Bounds &&
          VecIter->Target == Pair.Target)
        return;

    I->second.push_back(Pair);
}

void Sema::ModifiedBoundsDependencies::Dump(raw_ostream &OS) {
  OS << "Mapping from expressions to modified bounds:\n";
  for (auto Iter = Tracker.begin(); Iter != Tracker.end(); ++Iter) {
    OS << "Expression:\n";
    Iter->first->dump(OS);
    OS << "Modified:\n";
    for (auto VarIter = Iter->second.begin(); VarIter != Iter->second.end();
         ++VarIter) {
      OS << "LValue expression:\n";
      if (VarIter->Target.is<VarDecl *>())
        VarIter->Target.get<VarDecl *>()->dump(OS);
      else
        VarIter->Target.get<MemberExpr *>()->dump(OS);
      OS << "Bounds:\n";
      VarIter->Bounds->dump(OS);;
    }
  }
}

namespace {
// Update mapping from expressions that modify lvalues (assignments,
// increment/decrement expressions) to bounds expressions that use those
// lvalues.
 class ModifyingExprDependencies {
 private:
   Sema &SemaRef;
   Sema::ModifiedBoundsDependencies &Tracker;

 public:
 ModifyingExprDependencies(Sema &SemaRef,
                           Sema::ModifiedBoundsDependencies &Tracker) :
   SemaRef(SemaRef), Tracker(Tracker) {}

 // Statement to traverse.  This iterates recursively over a statement
 // and all of its children statements.
 void TraverseStmt(Stmt *S, bool Kind) {
   if (!S)
      return;

   bool NewScope = false;

   switch (S->getStmtClass()) {
     case Expr::UnaryOperatorClass:
       VisitUnaryOperator(cast<UnaryOperator>(S), Kind);
       break;
     case Expr::BinaryOperatorClass:
     case Expr::CompoundAssignOperatorClass:
       VisitBinaryOperator(cast<BinaryOperator>(S), Kind);
       break;
     case Stmt::CompoundStmtClass: {
       CompoundStmt *CS = cast<CompoundStmt>(S);
       Kind = CS->isCheckedScope();
       NewScope = true;
       break;
     }
     case Stmt::DeclStmtClass: {
       DeclStmt *DS = cast<DeclStmt>(S);
       auto BeginDecls = DS->decl_begin(), EndDecls = DS->decl_end();
       for (auto I = BeginDecls; I != EndDecls; ++I) {
         Decl *D = *I;
         // The initializer expression is visited during the loop
         // below iterating children.
         if (VarDecl *VD = dyn_cast<VarDecl>(D))
           VisitVarDecl(VD, Kind);
       }
       break;
     }
     default:
       break;
    }

    unsigned CurrentBoundsScope = 0;
    if (NewScope)
      CurrentBoundsScope = SemaRef.BoundsDependencies.EnterScope();

    auto Begin = S->child_begin(), End = S->child_end();
    for (auto I = Begin; I != End; ++I) {
      TraverseStmt(*I, Kind);
    }

    if (NewScope) {
#if DEBUG_DEPENDENCES
      SemaRef.BoundsDependencies.Dump(llvm::outs());
#endif
      SemaRef.BoundsDependencies.ExitScope(CurrentBoundsScope);
    }
 }

 //
 // Visit methods take some action for a specific node.
 //

  /// \brief Record the lvalues that the bounds for D depened upon.
  void VisitVarDecl(VarDecl *D, bool InCheckedScope) {
    SemaRef.BoundsDependencies.Add(D);
  }

  /// /\brief Record the bounds that use the LValue expression modified by E.
  void RecordLValueUpdate(Expr *E, Expr *LValue, bool InCheckedScope) {
    if (DeclRefExpr *DR = dyn_cast<DeclRefExpr>(LValue)) {
      if (VarDecl *D = dyn_cast<VarDecl>(DR->getDecl())) {
        Sema::BoundsDependencyTracker::VarBoundsIteratorRange Range =
          SemaRef.BoundsDependencies.DependentBoundsDecls(D);
        for (auto Current = Range.begin(), End = Range.end(); Current != End;
             ++Current) {
          VarDecl *VarWithBounds = *Current;
          assert(VarWithBounds->getBoundsExpr());
          if (InCheckedScope ||
              VarWithBounds->hasBoundsDeclaration(SemaRef.getASTContext()))
            Tracker.Add(E, VarWithBounds, VarWithBounds->getBoundsExpr());
        }
      }
    }
  }

  void VisitUnaryOperator(UnaryOperator *E, bool InCheckedScope) {
    if (!UnaryOperator::isIncrementDecrementOp(E->getOpcode()))
      return;

    Expr *LValue = Helper::SimplifyLValue(E->getSubExpr());
    RecordLValueUpdate(E, LValue, InCheckedScope);
  }

  void VisitBinaryOperator(BinaryOperator *E, bool InCheckedScope) {
    if (!E->isAssignmentOp())
      return;

    Expr *LValue = Helper::SimplifyLValue(E->getLHS());
    RecordLValueUpdate(E, LValue, InCheckedScope);
  }
};
}

void Sema::ComputeBoundsDependencies(ModifiedBoundsDependencies &Tracker,
                                     FunctionDecl *FD, Stmt *Body) {
  if (!Body)
    return;

#if DEBUG_DEPENDENCES
  llvm::outs() << "Computing bounds dependencies for "
               << FD->getName()
               << ".\n";
#endif

  // Track parameter bounds declarations in function parameter scope.
  unsigned CurrentBoundsScope = BoundsDependencies.EnterScope();
  for (auto ParamIter = FD->param_begin(), ParamEnd = FD->param_end();
       ParamIter != ParamEnd; ++ParamIter)
    BoundsDependencies.Add(*ParamIter);

#if DEBUG_DEPENDENCES
  BoundsDependencies.Dump(llvm::outs());
 #endif

  ModifyingExprDependencies(*this, Tracker).TraverseStmt(Body, false);

  // Stop tracking parameter bounds declaration dependencies.
  BoundsDependencies.ExitScope(CurrentBoundsScope);

#if DEBUG_DEPENDENCES
  llvm::outs() << "Done " << FD->getName() << ".\n";
  llvm::outs() << "Results:\n";

  BoundsDependencies.Dump(llvm::outs());
  Tracker.Dump(llvm::outs());
#endif
}

// Check whether an expression contains an occurrence of the _Return_value
// expression.
namespace {
  class CheckForReturnValue : public RecursiveASTVisitor<CheckForReturnValue> {
  private:
    Sema &SemaRef;
    bool FoundReturnValue;

  public:
    CheckForReturnValue(Sema &SemaRef) :
      SemaRef(SemaRef), FoundReturnValue(false) {}

    bool ContainsReturnValueExpr() {
      return FoundReturnValue;
    }

    bool VisitBoundsValueExpr(BoundsValueExpr *BVE) {
      if (BVE->getKind() == BoundsValueExpr::Kind::Return) {
        FoundReturnValue = true;
        return false; // Stop the AST traversal early.
      }
      return true;
    }
  };
}

bool Sema::ContainsReturnValueExpr(Expr *E) {
  if (!E)
    return false;

  CheckForReturnValue Checker(*this);
  Checker.TraverseStmt(E);
  return Checker.ContainsReturnValueExpr();
}
