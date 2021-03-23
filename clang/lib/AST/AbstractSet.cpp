#include "clang/AST/AbstractSet.h"

using namespace clang;


AbstractSet *AbstractSetManager::GetOrCreateAbstractSet(Expr *E) {
  // Create a canonical form for E.
  PreorderAST *P = new PreorderAST(S.getASTContext(), E);
  P->Normalize();

  // Search for an existing PreorderAST that is equivalent to the canonical
  // form for E.
  auto I = SortedPreorderASTs.find(P);
  if (I != SortedPreorderASTs.end()) {
    PreorderAST *ExistingCanonicalForm = *I;
    // If an AbstractSet exists in PreorderASTAbstractSetMap whose CanonicalForm
    // is equivalent to ExistingCanonicalForm, then that AbstractSet is the
    // one that contains E.
    auto It = PreorderASTAbstractSetMap.find(ExistingCanonicalForm);
    if (It != PreorderASTAbstractSetMap.end()) {
      return It->second;
    }
  }

  // If there is no existing AbstractSet that contains E, create a new
  // AbstractSet that contains E.
  AbstractSet *A = new AbstractSet(*P, E);
  SortedPreorderASTs.emplace(P);
  PreorderASTAbstractSetMap[P] = A;
  return A;
}

}
