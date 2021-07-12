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
    llvm::raw_ostream &OS;

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

    // FieldWithBounds is a field that has a declared bounds expression.
    // It is used to track the field with which a declared bounds expression
    // is associated. For example, FieldWithBounds tracks the field "f" in
    // the following:
    // struct S {
    //   _Array_ptr<int> f : bounds(lower, upper);
    // };
    FieldDecl *FieldWithBounds = nullptr;

    // InBoundsExprLower indicates that we are currently processing the lower
    // bounds expression of a BoundsExpr that has been expanded to a
    // RangeBoundsExpr. This flag is used to determine whether a variable
    // occurs in the lower bounds expression of a VarWithBounds.
    bool InBoundsExprLower = false;

    // InBoundsExprUpper indicates that we are currently processing the upper
    // bounds expression of a BoundsExpr that has been expanded to a
    // RangeBoundsExpr. This flag is used to determine whether a variable
    // occurs in the upper bounds expression of a VarWithBounds.
    bool InBoundsExprUpper = false;

    // ProcessedRecords keeps tracks of the record declarations whose fields
    // have been traversed by the FillBoundsSiblingFields method. This avoids
    // unnecessary duplicate traversals of record fields.
    llvm::SmallPtrSet<const RecordDecl *, 2> ProcessedRecords;

    // GetRecordDecl returns the record declaration, if any, that is
    // associated with the given type. For example, if the given type is
    // struct S, struct S *, _Ptr<struct S>, struct S **, etc.,
    // GetRecordDecl will return the declaration of S.
    RecordDecl *GetRecordDecl(const QualType Ty) {
      const Type *T = Ty.getTypePtr();
      // If T is a pointer type T * or array type T[],
      // getPointeeOrArrayElementType will return T, which itself may be a
      // pointer or array type. Keep descending T until T is not a pointer
      // or array type.
      while (T->isAnyPointerType() || T->isArrayType())
        T = T->getPointeeOrArrayElementType();
      return T->getAsRecordDecl();
    }

    // AddPtrWithBounds adds PtrWithBounds to the set of variables or fields in
    // whose bounds VarOrField occurs.
    template<class T, class U>
    void AddPtrWithBounds(T &Map, const U *PtrWithBounds, const U *VarOrField) {
      auto It = Map.find(VarOrField);
      if (It != Map.end())
        It->second.insert(PtrWithBounds);
      else {
        typename T::mapped_type Ptrs;
        Ptrs.insert(PtrWithBounds);
        Map[VarOrField] = Ptrs;
      }
    }

    // If the type T is associated with a record declaration S,
    // FillBoundsSiblingFields traverses the fields in S in order to map each
    // field F in S to the fields in S in whose declared bounds F appears.
    void FillBoundsSiblingFields(const QualType T) {
      RecordDecl *S = GetRecordDecl(T);
      if (!S)
        return;

      // Do not traverse a record declaration more than once.
      // FillBoundsSiblingFields can be called more than once on a given
      // record declaration if a function declares multiple variables with
      // the same record type, or in case of recursive record definitions.
      if (ProcessedRecords.count(S))
        return;

      FieldWithBounds = nullptr;
      ProcessedRecords.insert(S);
      for (auto It = S->field_begin(), E = S->field_end(); It != E; ++It) {
        auto *Field = *It;

        // Recursively traverse the record declarations of struct or
        // union-typed fields.
        FillBoundsSiblingFields(Field->getType());

        // For fields with declared bounds expressions, get the sibling fields
        // that implicitly or explicltly appear within the declared bounds.
        if (BoundsExpr *FieldBounds = Field->getBoundsExpr()) {
          FieldWithBounds = Field;
          // Fields with count bounds implicitly occur in their own declared
          // bounds. For example, if a field p has declared bounds count(1),
          // then p occurs in the declared bounds of p.
          if (isa<CountBoundsExpr>(FieldBounds))
            AddPtrWithBounds(Info.BoundsSiblingFields, FieldWithBounds, Field);

          // Traverse the declared bounds to visit the DeclRefExprs that
          // explicitly occur in the declared bounds. These DeclRefExprs
          // are the siblings of Field that appear in its declared bounds.
          TraverseStmt(FieldBounds);
          FieldWithBounds = nullptr;
        }
      }
    }

  public:
    PrepassHelper(Sema &SemaRef, PrepassInfo &Info) :
      SemaRef(SemaRef), Info(Info), OS(llvm::outs()) {}

    bool VisitVarDecl(VarDecl *V) {
      if (!V || V->isInvalidDecl())
        return true;

      // If V declares a variable with a struct or union type (e.g. struct S,
      // struct S *, etc.), traverse the fields in the declaration of S in
      // order to map to each field F in S to the fields in S in whose
      // declared bounds F appears.
      FillBoundsSiblingFields(V->getType());

      // If V has a bounds expression, traverse it so we visit the
      // DeclRefExprs within the bounds.
      if (V->hasBoundsExpr()) {
        if (BoundsExpr *B = SemaRef.NormalizeBounds(V)) {
          VarWithBounds = V;
          TraverseStmt(B);
          VarWithBounds = nullptr;
        }

      // Else if V does not have a bounds expression but V is a checked array
      // or a checked null-terminated array then V occurs in its upper and
      // lower bounds. So we add V to the set of variables that occur in its
      // lower and upper bounds. For example:
      // void f(_Nt_array_ptr<char> p);
      // char p _Checked[] = "ab";
      } else if (V->getType()->isCheckedPointerArrayType() ||
                 V->getType()->isCheckedArrayType()) {
        AddPtrWithBounds(Info.BoundsVarsLower, V, V);
        AddPtrWithBounds(Info.BoundsVarsUpper, V, V);
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
      // If E is a use of a field declaration F, then we add FieldWithBounds
      // to the set of all fields in whose bounds expressions F occurs.
      if (FieldWithBounds) {
        const FieldDecl *F = dyn_cast_or_null<FieldDecl>(E->getDecl());
        if (F && !F->isInvalidDecl())
          AddPtrWithBounds(Info.BoundsSiblingFields, FieldWithBounds, F);
        return true;
      }

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

      if (VarWithBounds) {
        // We add VarWithBounds to the set of all variables in whose lower and
        // upper bounds expressions V occurs.
        if (InBoundsExprLower)
          AddPtrWithBounds(Info.BoundsVarsLower, VarWithBounds, V);
        else if (InBoundsExprUpper)
          AddPtrWithBounds(Info.BoundsVarsUpper, VarWithBounds, V);
      }

      return true;
    }

    // For a temporary binding with a struct type (or pointer to a struct
    // type), fill in the bounds sibling fields for the record declaration.
    bool VisitCHKCBindTemporaryExpr(CHKCBindTemporaryExpr *E) {
      FillBoundsSiblingFields(E->getType());
      return true;
    }

    // For a call expression with a struct type (or pointer to a struct
    // type), fill in the bounds sibling fields for the record declaration.
    bool VisitCallExpr(CallExpr *E) {
      FillBoundsSiblingFields(E->getType());
      return true;
    }

    bool VisitBoundsExpr(BoundsExpr *B) {
      if (!VarWithBounds)
        return true;

      if (RangeBoundsExpr *R = dyn_cast_or_null<RangeBoundsExpr>(B)) {
        InBoundsExprLower = true;
        TraverseStmt(R->getLowerExpr());
        InBoundsExprLower = false;

        InBoundsExprUpper = true;
        TraverseStmt(R->getUpperExpr());
        InBoundsExprUpper = false;
      }

      return true;
    }

    bool ProcessWhereClause(WhereClause *WC) {
      if (!WC)
        return true;

      for (WhereClauseFact *Fact : WC->getFacts()) {
        if (BoundsDeclFact *F = dyn_cast<BoundsDeclFact>(Fact)) {
          if (BoundsExpr *NormalizedBounds = SemaRef.NormalizeBounds(F)) {
            VarDecl *OrigVarWithBounds = VarWithBounds;
            VarWithBounds = F->getVarDecl();
            TraverseStmt(NormalizedBounds);
            VarWithBounds = OrigVarWithBounds;
          }
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
      PrintDeclMap<const VarDecl *>(FD, "BoundsVars Lower",
                                    Info.BoundsVarsLower);
      PrintDeclMap<const VarDecl *>(FD, "BoundsVars Upper",
                                    Info.BoundsVarsUpper);
    }

    void DumpBoundsSiblingFields(FunctionDecl *FD) {
      PrintDeclMap<const FieldDecl *>(FD, "BoundsSiblingFields",
                                      Info.BoundsSiblingFields);
    }

    // Print a map from a key of type T to a set of elements of type T,
    // where T should inherit from NamedDecl.
    // This method can be used to print the BoundsVarsLower, BoundsVarsUpper
    // and BoundsSiblingFields maps.
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

  if (getLangOpts().DumpBoundsSiblingFields)
    Prepass.DumpBoundsSiblingFields(FD);
}
