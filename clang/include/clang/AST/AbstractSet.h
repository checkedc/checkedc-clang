#ifndef LLVM_CLANG_ABSTRACT_SET_H
#define LLVM_CLANG_ABSTRACT_SET_H

#include <set>
#include "clang/AST/ASTContext.h"
#include "clang/AST/CanonBounds.h"
#include "clang/AST/Expr.h"
#include "clang/AST/PreorderAST.h"

namespace clang {
  using Result = Lexicographic::Result;

  class AbstractSet {
  private:
    // Canonical form of all lvalue expressions that belong to this AbstractSet.
    PreorderAST CanonicalForm;

    // LValue expression that is a representative of all lvalue expressions that
    // belong to this AbstractSet. This can be used in bounds validation to:
    // 1. Get the Decl for the representative. This is used to determine the
    //    location of the note that specifies the declared (target) bounds.
    // 2. Get the target bounds for the representative. This is used to prove
    //    or disprove that the inferred bounds for the expressions in this
    //    AbstractSet imply the target bounds. All lvalue expressions in this
    //    AbstractSet have the same target bounds as the representative.
    Expr *Representative;

  public:
    AbstractSet(PreorderAST P) : CanonicalForm(P) {}

    void SetRepresentative(Expr *E) {
      Representative = E;
    }

    Expr *GetRepresentative() {
      return Representative;
    }

    // The comparison between two AbstractSets is the same as the
    // lexicographic comparison between their CanonicalForms.
    Result Compare(AbstractSet &Other) {
      return CanonicalForm.Compare(Other.CanonicalForm);
    }

    bool operator<(AbstractSet &Other) {
      return Compare(Other) == Result::LessThan;
    }
    bool operator==(AbstractSet &Other) {
      return Compare(Other) == Result::Equal;
    }
  };

  // Custom comparison for SortedAbstractSets so that the AbstractSets are
  // sorted lexicographically by their CanonicalForms.
  struct AbstractSetComparer {
    bool operator()(AbstractSet *A, AbstractSet *B) const {
      return *A < *B;
    }
  };

  class AbstractSetManager {
  private:
    // Maintain a sorted set of AbstractSets that have been created while
    // traversing a function. A binary search in this set is used to determine
    // whether an lvalue expression belongs to an existing AbstractSet.
    static std::set<AbstractSet *, AbstractSetComparer> SortedAbstractSets;

  public:
    // Returns the AbstractSet that contains the lvalue expression E. If
    // there is an AbstractSet A in SortedAbstractSets that contains E,
    // GetOrCreateAbstractSet returns A. Otherwise, it creates a new
    // AbstractSet for E.
    static AbstractSet *GetOrCreateAbstractSet(Expr *E, ASTContext &Ctx);

    // Clears the contents of the AbstractSetManager, since storage of the
    // AbstractSets should not persist across functions.
    static void Clear(void);
  };
} // end namespace clang

#endif