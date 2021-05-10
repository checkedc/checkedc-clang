//===--- CheckedCAnalysesPrepass.cpp: Data used by Checked C analyses ---===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
//
//  This file implements methods to traverse a function and gather different
//  kinds of information. This information is used by different Checked C
//  analyses such as bounds declaration checking, bounds widening, etc.
//
//===---------------------------------------------------------------------===//

#include "clang/Sema/CheckedCAnalysesPrepass.h"
#include "clang/Sema/Sema.h"

using namespace clang;

class PrepassHelper : public RecursiveASTVisitor<PrepassHelper> {
  private:
    Sema &SemaRef;
    PrepassInfo &Info;

    // VarWithBounds is a variable that has a bounds expression. It is used to
    // track:
    // 1. Whether a visited expression is within a declared or a where clause
    // bounds expression. For example, VarWithBounds tracks the expressions
    // "lower" and "upper" in the following:
    // _Nt_array_ptr<char> p : bounds(lower, upper);
    // int x = 1 _Where p : bounds(lower, upper);

    // 2. The variable with which a declared or a where clause bounds
    // expression is associated. For example, VarWithBounds tracks the variable
    // "p" in the following:
    // _Nt_array_ptr<char> p : bounds(lower, upper);
    // int x = 1 _Where p : bounds(lower, upper);

    VarDecl *VarWithBounds = nullptr;
    llvm::raw_ostream &OS;

  public:
    PrepassHelper(Sema &SemaRef, PrepassInfo &Info) :
      SemaRef(SemaRef), Info(Info), OS(llvm::outs()) {}

    bool VisitVarDecl(VarDecl *V) {
      if (!V || V->isInvalidDecl())
        return true;
      // If V has a bounds expression, traverse it so we visit the
      // DeclRefExprs within the bounds.
      if (V->hasBoundsExpr()) {
        if (BoundsExpr *B = SemaRef.NormalizeBounds(V)) {
          VarWithBounds = V;
          TraverseStmt(B);
          VarWithBounds = nullptr;
        }
      }
      // Process any where clause attached to this VarDecl.
      // Note: This also handles function parameters.
      // For example,
      // int x = 1 _Where p : bounds(lower, upper);
      // void f(_Nt_array_ptr<char> p : bounds(lower, upper)) {}
      return ProcessWhereClause(V->getWhereClause());
    }

    // We may modify the VarUses map when a DeclRefExpr is visited.
    bool VisitDeclRefExpr(DeclRefExpr *E) {
      const VarDecl *V = dyn_cast_or_null<VarDecl>(E->getDecl());
      if (!V || V->isInvalidDecl())
        return true;
      // We only add the V => E pair to the VarUses map if:
      // 1. E is within a declared bounds expression, or:
      // 2. V has a declared bounds expression.
      if (VarWithBounds || V->hasBoundsExpr()) {
        if (!Info.VarUses.count(V))
          Info.VarUses[V] = E;
      }

      // We add VarWithBounds to the set of all variables in whose bounds
      // expressions V occurs.
      if (VarWithBounds) {
        auto It = Info.BoundsVars.find(V);
        if (It != Info.BoundsVars.end())
          It->second.insert(VarWithBounds);
        else {
          VarSetTy Vars;
          Vars.insert(VarWithBounds);
          Info.BoundsVars[V] = Vars;
        }
      }

      return true;
    }

    bool ProcessWhereClause(WhereClause *WC) {
      if (!WC)
        return true;

      for (WhereClauseFact *Fact : WC->getFacts()) {
        if (BoundsDeclFact *BDF = dyn_cast<BoundsDeclFact>(Fact)) {
          VarDecl *V = BDF->Var;
          BoundsExpr *B = BDF->Bounds;

          VarDecl *OrigVarWithBounds = VarWithBounds;
          VarWithBounds = V;
          TraverseStmt(B);
          VarWithBounds = OrigVarWithBounds;
        }
      }

      return true;
    }

    bool VisitNullStmt(NullStmt *S) {
      // Process any where clause attached to a NullStmt. For example,
      // _Where p : bounds(lower, upper);
      return ProcessWhereClause(S->getWhereClause());
    }

    bool VisitValueStmt(ValueStmt *S) {
      // Process any where clause attached to a ValueStmt. For example,
      // x = 1 _Where p : bounds(lower, upper);
      return ProcessWhereClause(S->getWhereClause());
    }

    void DumpBoundsVars(FunctionDecl *FD) {
      PrintDeclMap<const VarDecl *>(FD, "BoundsVars", Info.BoundsVars);
    }

    // Print a map from a key of type T to a set of elements of type T,
    // where T should inherit from NamedDecl.
    // This method can be used to print the BoundsVars and
    // BoundsSiblingFields maps.
    template<class T>
    void PrintDeclMap(FunctionDecl *FD, const char *Message,
                      llvm::DenseMap<T, llvm::SmallPtrSet<T, 2>> Map) {
      OS << "--------------------------------------\n"
         << "In function: " << FD->getName() << "\n"
         << Message << ":\n";

      // Decls is a map of NamedDecls (keys) to a set of NamedDecls (values).
      // So there is no defined iteration order for its keys or values.
      // So we copy the keys to a vector, sort the vector and then iterate it.
      // While iterating each key we also copy its value (which is a set of
      // NamedDecls) to a vector, sort the vector and iterate it.
      llvm::SmallVector<T, 2> Decls;
      for (const auto item : Map)
        Decls.push_back(item.first);

      SortDecls(Decls);

      for (const auto D : Decls) {
        OS << D->getQualifiedNameAsString() << ": { ";

        llvm::SmallVector<T, 2> InnerDecls;
        for (const auto item : Map[D])
          InnerDecls.push_back(item);

        SortDecls(InnerDecls);

        for (const auto InnerD : InnerDecls)
          OS << InnerD->getQualifiedNameAsString() << " ";
        OS << "}\n";
      }
      OS << "--------------------------------------\n";
    }

    // Sort a list of elements of type T, where T should inherit
    // from NamedDecl.
    template<class T>
    void SortDecls(llvm::SmallVector<T, 2> &Decls) {
      // Sort decls by their name. If two decls in a program have the same
      // name (for example, a variable in a nested scope that shadows a
      // variable from an outer scope, or fields from different structs that
      // have the same name), then we sort them by their source locations.
      llvm::sort(Decls.begin(), Decls.end(),
                 [](T A, T B) {
                   int StrCompare = A->getQualifiedNameAsString().compare(
                                    B->getQualifiedNameAsString());
                   return StrCompare != 0 ?
                          StrCompare < 0 :
                          A->getLocation() < B->getLocation();
                 });
    }
};

// Traverse a function in order to gather information that is used by different
// Checked C analyses such as bounds declaration checking, bounds widening, etc.
void Sema::CheckedCAnalysesPrepass(PrepassInfo &Info, FunctionDecl *FD,
                                   Stmt *Body) {
  PrepassHelper Prepass(*this, Info);
  for (auto I = FD->param_begin(); I != FD->param_end(); ++I) {
    ParmVarDecl *Param = *I;
    Prepass.VisitVarDecl(Param);
  }
  Prepass.TraverseStmt(Body);

  if (getLangOpts().DumpBoundsVars)
    Prepass.DumpBoundsVars(FD);
}
