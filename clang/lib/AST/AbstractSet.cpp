#include "clang/AST/AbstractSet.h"

using namespace clang;

std::set<AbstractSet *, AbstractSetComparer> AbstractSetManager::SortedAbstractSets;

AbstractSet *AbstractSetManager::GetOrCreateAbstractSet(Expr *E, ASTContext &Ctx) {
  // Create a canonical form for E.
  PreorderAST P(Ctx, E);
  P.Normalize();

  // Search for an existing AbstractSet in SortedAbstractSets that contains E,
  // i.e. an AbstractSet whose CanonicalForm is equal to E's canonical form.
  AbstractSet *Test = new AbstractSet(P);
  auto It = SortedAbstractSets.find(Test);
  if (It != SortedAbstractSets.end()) {
    delete Test;
    return *It;
  }

  // If there is no existing AbstractSet that contains E, create a new
  // AbstractSet that contains E.
  AbstractSet *A = new AbstractSet(P);
  A->SetRepresentative(E);
  SortedAbstractSets.emplace(A);
  return A;
}

void AbstractSetManager::Clear(void) {
  SortedAbstractSets.clear();
}
