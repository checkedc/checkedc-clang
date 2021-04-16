#include "clang/AST/CheckedCPrepass.h"
#include "clang/Sema/Sema.h"

using namespace clang;

class PrepassHelper : public RecursiveASTVisitor<PrepassHelper> {
  private:
    Sema &SemaRef;
    PrepassInfo &Info;
    // InBoundsExpr tracks whether the expressions being visited are
    // within a declared bounds expression.
    bool InBoundsExpr = false;

  public:
    PrepassHelper(Sema &SemaRef, PrepassInfo &Info) :
      SemaRef(SemaRef), Info(Info) {}

    bool VisitVarDecl(VarDecl *V) {
      // If V has a bounds expression, traverse it so we visit the
      // DeclRefExprs within the bounds.
      if (V->hasBoundsExpr()) {
        if (BoundsExpr *B = SemaRef.NormalizeBounds(V)) {
          InBoundsExpr = true;
          TraverseStmt(B);
          InBoundsExpr = false;
        }
      }
      return true;
    }

    // We may modify the VarUses map when a DeclRefExpr is visited.
    bool VisitDeclRefExpr(DeclRefExpr *E) {
      VarDecl *V = dyn_cast_or_null<VarDecl>(E->getDecl());
      if (!V)
        return true;
      // We only add the V => E pair to the VarUses map if:
      // 1. E is within a declared bounds expression, or:
      // 2. V has a declared bounds expression.
      if (!InBoundsExpr && !V->hasBoundsExpr())
        return true;
      if (Info.VarUses.count(V))
        return true;
      Info.VarUses[V] = E;
      return true;
    }
};

// Traverse a function in order to gather information that is used by different
// Checked C analyses such as bounds declaration checking, bounds widening, etc.
void Sema::CheckedCPrepass(PrepassInfo &Info, FunctionDecl *FD, Stmt *Body) {
  PrepassHelper Prepass(*this, Info);
  for (auto I = FD->param_begin(); I != FD->param_end(); ++I) {
    ParmVarDecl *Param = *I;
    Prepass.VisitVarDecl(Param);
  }
  Prepass.TraverseStmt(Body);
}

