//===---------- SemaBounds.cpp - Operations On Bounds Expressions --------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements operations on bounds expressions for semantic analysis.
//  The operations include:
//  * Abstracting bounds expressions so that they can be used in function types.
//    This also checks that requirements on variable references are met and
//    emit diagnostics if they are not.
//
//    The abstraction also removes extraneous details:
//    - References to ParamVarDecl's are abstracted to positional index numbers
//      in argument lists.
//    - References to other VarDecls's are changed to use canonical
//      declarations.
//
//    Line number information is left in place for expressions, though.  It
//    would be a lot of work to write functions to change the line numbers to
//    the invalid line number. The canonicalization of types ignores line number
//    information in determining if two expressions are the same.  Users of bounds
//    expressions that have been abstracted need to be aware that line number
//    information may be inaccurate.
//  * Concretizing bounds expressions from function types.  This undoes the
//    abstraction by substituting parameter varaibles for the positional index
//    numbers.
//
//  Debugging pre-processor flags:
//    - TRACE_CFG:
//      Dumps AST and CFG of the visited nodes when traversing the CFG.
//    - TRACE_RANGE:
//      Dumps the valid bounds ranges, memory access ranges and memory
//      access expressions.
//===----------------------------------------------------------------------===//

#include "clang/Analysis/CFG.h"
#include "clang/Analysis/Analyses/PostOrderCFGView.h"
#include "clang/AST/AbstractSet.h"
#include "clang/AST/CanonBounds.h"
#include "clang/AST/ExprUtils.h"
#include "clang/AST/NormalizeUtils.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Sema/AvailableFactsAnalysis.h"
#include "clang/Sema/BoundsUtils.h"
#include "clang/Sema/BoundsWideningAnalysis.h"
#include "clang/Sema/CheckedCAnalysesPrepass.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallString.h"
#include "TreeTransform.h"
#include <queue>

// #define TRACE_CFG 1
// #define TRACE_RANGE 1

using namespace clang;
using namespace sema;

namespace {
  class AbstractBoundsExpr : public TreeTransform<AbstractBoundsExpr> {
    typedef TreeTransform<AbstractBoundsExpr> BaseTransform;
    typedef ArrayRef<DeclaratorChunk::ParamInfo> ParamsInfo;

  private:
    const ParamsInfo Params;

    // TODO: change this constant when we want to error on global variables
    // in parameter bounds declarations.
    const bool errorOnGlobals = false;

  public:
    AbstractBoundsExpr(Sema &SemaRef, ParamsInfo Params) :
      BaseTransform(SemaRef), Params(Params) {}

    Decl *TransformDecl(SourceLocation Loc, Decl *D) {
      return D->getCanonicalDecl();
    }

    ExprResult TransformDeclRefExpr(DeclRefExpr *E) {
      ValueDecl *D = E->getDecl();
      if (VarDecl *V = dyn_cast<VarDecl>(D)) {
        if (V->isLocalVarDecl())
          // Parameter bounds may not be in terms of local variables
          SemaRef.Diag(E->getLocation(),
                       diag::err_out_of_scope_function_type_local);
        else if (V->isFileVarDecl() || V->isExternC()) {
          // Parameter bounds may not be in terms of "global" variables
          // TODO: This is guarded by a flag right now, as we don't yet
          // want to error everywhere.
          if (errorOnGlobals) {
            SemaRef.Diag(E->getLocation(),
                          diag::err_out_of_scope_function_type_global);
          }
        }
        else if (ParmVarDecl *PD = dyn_cast<ParmVarDecl>(D)) {
          // Parameter bounds may be in terms of other parameters,
          // in which case we'll convert to a position-based representation.
          for (auto &ParamInfo : Params)
            if (PD == ParamInfo.Param) {
              return SemaRef.CreatePositionalParameterExpr(
                PD->getFunctionScopeIndex(),
                PD->getType());
            }
          SemaRef.Diag(E->getLocation(),
                       diag::err_out_of_scope_function_type_parameter);
        }
      }

      ValueDecl *ND =
        dyn_cast_or_null<ValueDecl>(BaseTransform::TransformDecl(
          SourceLocation(), D));
      if (D == ND || ND == nullptr)
        return E;
      else {
        clang::NestedNameSpecifierLoc QualifierLoc  = E->getQualifierLoc();
        clang::DeclarationNameInfo NameInfo = E->getNameInfo();
        return getDerived().RebuildDeclRefExpr(QualifierLoc, ND, NameInfo,
                                                nullptr, nullptr);
      }
    }
  };
}

bool Sema::AbstractForFunctionType(
  BoundsAnnotations &Annots,
  ArrayRef<DeclaratorChunk::ParamInfo> Params) {  

  BoundsExpr *Expr = Annots.getBoundsExpr();
  // If there is no bounds expression, the itype does not change
  // as  aresult of abstraction.  Just return the original annotation.
  if (!Expr)
    return false;

  BoundsExpr *Result = nullptr;
  ExprResult AbstractedBounds =
    AbstractBoundsExpr(*this, Params).TransformExpr(Expr);
  if (AbstractedBounds.isInvalid()) {
    llvm_unreachable("unexpected failure to abstract bounds");
    Result = nullptr;
  } else {
    Result = dyn_cast<BoundsExpr>(AbstractedBounds.get());
    assert(Result && "unexpected dyn_cast failure");
  }

  if (Result == Expr)
    return false;

  Annots.setBoundsExpr(Result);
  return true;
}

namespace {
  class ConcretizeBoundsExpr : public TreeTransform<ConcretizeBoundsExpr> {
    typedef TreeTransform<ConcretizeBoundsExpr> BaseTransform;

  private:
    ArrayRef<ParmVarDecl *> Parameters;

  public:
    ConcretizeBoundsExpr(Sema &SemaRef, ArrayRef<ParmVarDecl *> Params) :
      BaseTransform(SemaRef),
      Parameters(Params) { }

    ExprResult TransformPositionalParameterExpr(PositionalParameterExpr *E) {
      unsigned index = E->getIndex();
      if (index < Parameters.size()) {
        ParmVarDecl *PD = Parameters[index];
        return SemaRef.BuildDeclRefExpr(PD, E->getType(),
          clang::ExprValueKind::VK_LValue, SourceLocation());
      } else {
        llvm_unreachable("out of range index for positional parameter");
        return ExprError();
      }
    }
  };
}

BoundsExpr *Sema::ConcretizeFromFunctionType(BoundsExpr *Expr,
                                             ArrayRef<ParmVarDecl *> Params) {
  if (!Expr)
    return Expr;

  BoundsExpr *Result;
  ExprSubstitutionScope Scope(*this); // suppress diagnostics

  ExprResult ConcreteBounds = ConcretizeBoundsExpr(*this, Params).TransformExpr(Expr);
  if (ConcreteBounds.isInvalid()) {
    llvm_unreachable("unexpected failure in making bounds concrete");
    return nullptr;
  }
  else {
    Result = dyn_cast<BoundsExpr>(ConcreteBounds.get());
    assert(Result && "unexpected dyn_cast failure");
    return Result;
  }
}

namespace {
  class CheckForModifyingArgs : public RecursiveASTVisitor<CheckForModifyingArgs> {
  private:
    Sema &SemaRef;
    const ArrayRef<Expr *> Arguments;
    llvm::SmallBitVector VisitedArgs;
    Sema::NonModifyingContext ErrorKind;
    Sema::NonModifyingMessage Message;
    bool ModifyingArg;
  public:
    CheckForModifyingArgs(Sema &SemaRef, ArrayRef<Expr *> Args,
                          Sema::NonModifyingContext ErrorKind,
                          Sema::NonModifyingMessage Message) :
      SemaRef(SemaRef),
      Arguments(Args),
      VisitedArgs(Args.size()),
      ErrorKind(ErrorKind),
      Message(Message),
      ModifyingArg(false) {}

    bool FoundModifyingArg() {
      return ModifyingArg;
    }

    bool VisitPositionalParameterExpr(PositionalParameterExpr *E) {
      unsigned index = E->getIndex();
      if (index < Arguments.size() && !VisitedArgs[index]) {
        VisitedArgs.set(index);
        if (!SemaRef.CheckIsNonModifying(Arguments[index], ErrorKind, Message)) {
          ModifyingArg = true;
        }
      }
      return true;
    }
  };
}

namespace {
  class ConcretizeBoundsExprWithArgs : public TreeTransform<ConcretizeBoundsExprWithArgs> {
    typedef TreeTransform<ConcretizeBoundsExprWithArgs> BaseTransform;

  private:
    ArrayRef<Expr *> Args;

  public:
    ConcretizeBoundsExprWithArgs(Sema &SemaRef, ArrayRef<Expr *> Args) :
      BaseTransform(SemaRef),
      Args(Args) { }

    ExprResult TransformPositionalParameterExpr(PositionalParameterExpr *E) {
      unsigned index = E->getIndex();
      if (index < Args.size()) {
        return SemaRef.MakeAssignmentImplicitCastExplicit(Args[index]);
      } else {
        llvm_unreachable("out of range index for positional parameter");
        return ExprError();
      }
    }
  };
}

BoundsExpr *Sema::ConcretizeFromFunctionTypeWithArgs(
  BoundsExpr *Bounds, ArrayRef<Expr *> Args,
  NonModifyingContext ErrorKind, NonModifyingMessage Message) {
  if (!Bounds || Bounds->isInvalid())
    return Bounds;

  auto CheckArgs = CheckForModifyingArgs(*this, Args, ErrorKind, Message);
  CheckArgs.TraverseStmt(Bounds);
  if (CheckArgs.FoundModifyingArg())
    return nullptr;

  ExprSubstitutionScope Scope(*this); // suppress diagnostics
  auto Concretizer = ConcretizeBoundsExprWithArgs(*this, Args);
  ExprResult ConcreteBounds = Concretizer.TransformExpr(Bounds);
  if (ConcreteBounds.isInvalid()) {
#ifndef NDEBUG
    llvm::outs() << "Failed concretizing\n";
    llvm::outs() << "Bounds:\n";
    Bounds->dump(llvm::outs(), Context);
    int count = Args.size();
    for (int i = 0; i < count; i++) {
      llvm::outs() << "Dumping arg " << i << "\n";
      Args[i]->dump(llvm::outs(), Context);
    }
    llvm::outs().flush();
#endif
    llvm_unreachable("unexpected failure in making function bounds concrete with arguments");
    return nullptr;
  }
  else {
    BoundsExpr *Result = dyn_cast<BoundsExpr>(ConcreteBounds.get());
    assert(Result && "unexpected dyn_cast failure");
    return Result;
  }
}

namespace {
  class ConcretizeMemberBounds : public TreeTransform<ConcretizeMemberBounds> {
    typedef TreeTransform<ConcretizeMemberBounds> BaseTransform;

  private:
    Expr *Base;
    bool IsArrow;

  public:
    ConcretizeMemberBounds(Sema &SemaRef, Expr *MemberBaseExpr, bool IsArrow) :
      BaseTransform(SemaRef), Base(MemberBaseExpr), IsArrow(IsArrow) { }

    // TODO: handle the situation where the base expression is an rvalue.
    // By C semantics, the result is an rvalue.  We are setting fields used in
    // bounds expressions to be lvalues, so we end up with a problems when
    // we expand the occurrences of the fields to be expressions that are
    //  rvalues.
    //
    // There are two problematic cases:
    // - We assume field expressions are lvalues, so we will have lvalue-to-rvalue
    //   conversions applied to rvalues.  We need to remove these conversions.
    // - The address of a field is taken.  It is illegal to take the address of
    //   an rvalue.
    //
    // rVvalue structs can arise from function returns of struct values.
    ExprResult TransformDeclRefExpr(DeclRefExpr *E) {
      if (FieldDecl *FD = dyn_cast<FieldDecl>(E->getDecl())) {
        if (Base->isRValue() && !IsArrow)
          // For now, return an error if we see an rvalue base.
          return ExprError();
        ASTContext &Context = SemaRef.getASTContext();
        ExprValueKind ResultKind;
        if (IsArrow)
          ResultKind = VK_LValue;
        else
          ResultKind = Base->isLValue() ? VK_LValue : VK_RValue;
        return
          MemberExpr::CreateImplicit(Context, Base, IsArrow, FD,
                                     E->getType(), ResultKind, OK_Ordinary);
      }
      return E;
    }
  };
}

BoundsExpr *Sema::MakeMemberBoundsConcrete(
  Expr *Base,
  bool IsArrow,
  BoundsExpr *Bounds) {
  ExprSubstitutionScope Scope(*this); // suppress diagnostics
  ExprResult ConcreteBounds =
    ConcretizeMemberBounds(*this, Base, IsArrow).TransformExpr(Bounds);
  if (ConcreteBounds.isInvalid())
    return nullptr;
  else {
    BoundsExpr *Result = dyn_cast<BoundsExpr>(ConcreteBounds.get());
    return Result;
  }
}

#if 0
namespace {
  // Convert occurrences of _Return_value in a return bounds expression
  // to _Current_expr_value.  Don't recurse into any return bounds
  // expressions nested in function types.  Occurrences of _Return_value
  // in those shouldn't be changed to _Current_value.
  class ReplaceReturnValue : public TreeTransform<ReplaceReturnValue> {
    typedef TreeTransform<ReplaceReturnValue> BaseTransform;

  public:
    ReplaceReturnValue(Sema &SemaRef) :BaseTransform(SemaRef) { }

    // Avoid transforming nested return bounds expressions.
    bool TransformReturnBoundsAnnotations(BoundsAnnotations &Annot,
                                          bool &Changed) {
      return false;
    }

    ExprResult TransformBoundsValueExpr(BoundsValueExpr *E) {
      BoundsValueExpr::Kind K = E->getKind();
      bool KindChanged = false;
      if (K == BoundsValueExpr::Kind::Return) {
        K = BoundsValueExpr::Kind::Current;
        KindChanged = true;
      }
      QualType QT = getDerived().TransformType(E->getType());
      if (!getDerived().AlwaysRebuild() && QT == E->getType() &&
          !KindChanged)
        return E;

      return getDerived().
        RebuildBoundsValueExpr(E->getLocation(), QT,K);
    }
   };
}
#endif

// Convert all temporary bindings in an expression to uses of the values	
// produced by a binding.   This should be done for bounds expressions that	
// are used in runtime checks.  That way we don't try to recompute a	
// temporary multiple times in an expression.	
namespace {	
  class PruneTemporaryHelper : public TreeTransform<PruneTemporaryHelper> {	
    typedef TreeTransform<PruneTemporaryHelper> BaseTransform;	


  public:	
    PruneTemporaryHelper(Sema &SemaRef) :	
      BaseTransform(SemaRef) { }	

    ExprResult TransformCHKCBindTemporaryExpr(CHKCBindTemporaryExpr *E) {	
      return new (SemaRef.Context) BoundsValueExpr(SourceLocation(), E);	
    }	
  };	

  Expr *PruneTemporaryBindings(Sema &SemaRef, Expr *E, CheckedScopeSpecifier CSS) {	
    // Account for checked scope information when transforming the expression.
    Sema::CheckedScopeRAII CheckedScope(SemaRef, CSS);

    Sema::ExprSubstitutionScope Scope(SemaRef); // suppress diagnostics	
    ExprResult R = PruneTemporaryHelper(SemaRef).TransformExpr(E);	
    if (R.isInvalid())
      return SemaRef.Context.getPrebuiltBoundsUnknown();
    else
      return R.get();
  }	
}

namespace {
  using EqualExprTy = SmallVector<Expr *, 4>;
  using DeclSetTy = llvm::DenseSet<const VarDecl *>;

  // EqualExprsContainsExpr returns true if the set Exprs contains an
  // expression that is equivalent to E.
  bool EqualExprsContainsExpr(Sema &S, const EqualExprTy Exprs, Expr *E,
                              EquivExprSets *EquivExprs) {
    for (auto I = Exprs.begin(); I != Exprs.end(); ++I) {
      if (Lexicographic(S.Context, EquivExprs).CompareExpr(*I, E) ==
        Lexicographic::Result::Equal)
        return true;
    }
    return false;
  }

  // Helper class for collecting a vector of unique variables as rvalues from an
  // expression. We collect rvalues because CheckingState.EquivExprSet uses
  // rvalues to check equality.
  class CollectVariableSetHelper
    : public RecursiveASTVisitor<CollectVariableSetHelper> {
  private:
    Sema &SemaRef;
    EqualExprTy VariableList;

  public:
    CollectVariableSetHelper(Sema &SemaRef)
      : SemaRef(SemaRef), VariableList() {}

    const EqualExprTy &GetVariableList() const { return VariableList; }

    bool VisitDeclRefExpr(DeclRefExpr *E) {
      // TODO: GitHub checkedc-clang issue #966. This method is quadratic
      // in the number of variables in an expression. It should use a
      // hashtable to determine whether E should be added to VariableList.
      if (!EqualExprsContainsExpr(SemaRef, VariableList, E, nullptr)) {
        VariableList.push_back(E);
      }

      return true;
    }
  };

  // Collect variables in E without duplication. If E is nullptr, return an
  // empty vector.
  EqualExprTy CollectVariableSet(Sema &SemaRef, Expr *E) {
      CollectVariableSetHelper Helper(SemaRef);
      Helper.TraverseStmt(E);
      return Helper.GetVariableList();
  }
}

namespace {
  // BoundsContextTy denotes a map of an AbstractSet to the bounds that
  // are currently known to be valid for the lvalue expressions in the set.
  using BoundsContextTy = llvm::DenseMap<const AbstractSet *, BoundsExpr *>;

  // ExprSetTy denotes a set of expressions.
  using ExprSetTy = SmallVector<Expr *, 4>;

  // ExprEqualMapTy denotes a map of an expression e to the set of
  // expressions that produce the same value as e.
  using ExprEqualMapTy = llvm::DenseMap<Expr *, ExprSetTy>;

  // Describes the position of a free variable (FR).
  enum class FreeVariablePosition {
    Lower = 0x1,    // The FR appears in (any) lower bounds.
    Upper = 0x2,    // The FR appears in (any) upper bounds.
    Observed = 0x4, // The FR appears in the observed bounds.
    Declared = 0x8, // The FR appears in the declared bounds.
  };

  // FreeVariableListTy denotes a vector of <free variable, position> pairs, and
  // represents a list of free variables and their positions w.r.t. the observed
  // and declared bounds.
  using FreeVariableListTy =
      SmallVector<std::pair<Expr *, FreeVariablePosition>, 4>;

  // AbstractSetSetTy denotes a set of AbstractSets.
  using AbstractSetSetTy = llvm::SmallPtrSet<const AbstractSet *, 4>;

  // CheckingState stores the outputs of bounds checking methods.
  // These members represent the state during bounds checking
  // and are updated while checking individual expressions.
  class CheckingState {
    public:
      // ObservedBounds maps AbstractSets to their current known bounds as
      // inferred by bounds checking.  These bounds are updated after
      // assignments to variables.
      //
      // ObservedBounds is named UC in the Checked C spec.
      //
      // The bounds in the ObservedBounds context should always be normalized
      // to range bounds if possible.  This allows updates to variables that
      // are implicitly used in bounds declarations to update the observed
      // bounds.  For example, an assignment to the variable p where p has
      // declared bounds count(i) should update the bounds of p, which
      // normalize to bounds(p, p + i).
      BoundsContextTy ObservedBounds;

      // EquivExprs stores sets of expressions that are equivalent to each
      // other after checking an expression e.  If two expressions e1 and
      // e2 are in the same set in EquivExprs, e1 and e2 produce the same
      // value.
      //
      // EquivExprs is named UEQ in the Checked C spec.
      EquivExprSets EquivExprs;

      // SameValue is a set of expressions that produce the same value as an
      // expression e once checking of e is complete.
      //
      // SameValue is named G in the Checked C spec.
      ExprSetTy SameValue;

      // LostLValues maps an AbstractSet A whose observed bounds are unknown
      // to a pair <B, E>, where the initial observed bounds B of A have been
      // set to unknown due to an assignment to the lvalue expression E, where
      // E had no original value.
      //
      // LostLValues is used to emit notes to provide more context to the user
      // when diagnosing unknown bounds errors.
      llvm::DenseMap<const AbstractSet *, std::pair<BoundsExpr *, Expr *>> LostLValues;

      // UnknownSrcBounds maps an AbstractSet A whose observed bounds are
      // unknown to the first expression with unknown bounds (if any) that
      // has been assigned to an lvalue expression in A.
      //
      // UnknownSrcBounds is used to emit notes to provide more context to the
      // user when diagnosing unknown bounds errors.
      llvm::DenseMap<const AbstractSet *, Expr *> UnknownSrcBounds;

      // BlameAssignments maps an AbstractSet A to an expression in a top-level
      // CFG statement that last updates any variable used in the declared
      // bounds of A.
      //
      // BlameAssignments is used to provide more context for two types of
      // diagnostic messages:
      //   1. The compiler cannot prove or can disprove the declared bounds for
      //   A are valid after an assignment to a variable in the bounds of A; and
      //   2. The inferred bounds of A become unknown after an assignment to a
      //   variable in the bounds of A.
      //
      // BlameAssignments is updated in UpdateAfterAssignment and reset after
      // checking each top-level CFG statement.
      llvm::DenseMap<const AbstractSet *, Expr *> BlameAssignments;

      // TargetSrcEquality maps a target expression V to the most recent
      // expression Src that has been assigned to V within the current
      // top-level CFG statement.  When validating the bounds context,
      // each pair <V, Src> should be included in a set EQ that contains
      // all equality facts in the EquivExprs state set.  The set EQ will
      // then be used to validate the bounds context.
      llvm::DenseMap<Expr *, Expr *> TargetSrcEquality;

      // LValuesAssignedChecked is a set of AbstractSets containing lvalue
      // expressions with unchecked pointer type that have been assigned an
      // expression with checked pointer type at some point during the current
      // top-level statement (if the statement occurs in an unchecked scope).
      // These AbstractSets should have their bounds validated during
      // ValidateBoundsContext. In an unchecked scope, AbstractSets containing
      // lvalue expressions with unchecked pointer type that have not been
      // assigned an expression with checked pointer type during the current
      // statement should not have their bounds validated.
      AbstractSetSetTy LValuesAssignedChecked;

      // Resets the checking state after checking a top-level CFG statement.
      void Reset() {
        SameValue.clear();
        LostLValues.clear();
        UnknownSrcBounds.clear();
        BlameAssignments.clear();
        TargetSrcEquality.clear();
        LValuesAssignedChecked.clear();
      }
  };
}

namespace {
  class DeclaredBoundsHelper : public RecursiveASTVisitor<DeclaredBoundsHelper> {
    private:
      Sema &SemaRef;
      BoundsContextTy &BoundsContextRef;
      AbstractSetManager &AbstractSetMgr;

    public:
      DeclaredBoundsHelper(Sema &SemaRef, BoundsContextTy &Context,
                           AbstractSetManager &AbstractSetMgr) :
        SemaRef(SemaRef),
        BoundsContextRef(Context),
        AbstractSetMgr(AbstractSetMgr) {}

      // If a variable declaration has declared bounds, modify BoundsContextRef
      // to map the variable declaration to the normalized declared bounds.
      // 
      // Returns true if visiting the variable declaration did not terminate
      // early.  Visiting variable declarations in DeclaredBoundsHelper should
      // never terminate early.
      bool VisitVarDecl(const VarDecl *D) {
        if (!D)
          return true;
        if (D->isInvalidDecl())
          return true;
        if (!D->hasBoundsExpr())
          return true;
        // Parameters declared within a statement (e.g. in a function pointer
        // declaration) should not be added to the bounds context. Parameters
        // to the current function will be added to the bounds context in
        // TraverseCFG.
        if (isa<ParmVarDecl>(D))
          return true;
        // The bounds expressions in the bounds context should be normalized
        // to range bounds.
        if (BoundsExpr *Bounds = SemaRef.NormalizeBounds(D)) {
          const AbstractSet *A = AbstractSetMgr.GetOrCreateAbstractSet(D);
          BoundsContextRef[A] = Bounds;
        }
        return true;
      }
  };

  // GetDeclaredBounds modifies the bounds context to map any variables
  // declared in S to their declared bounds (if any).
  void GetDeclaredBounds(Sema &SemaRef, BoundsContextTy &Context, Stmt *S,
                         AbstractSetManager &AbstractSetMgr) {
    DeclaredBoundsHelper Declared(SemaRef, Context, AbstractSetMgr);
    Declared.TraverseStmt(S);
  }
}

namespace {
  class CheckBoundsDeclarations {
  private:
    Sema &S;
    bool DumpBounds;
    bool DumpState;
    bool DumpSynthesizedMembers;
    uint64_t PointerWidth;
    Stmt *Body;
    CFG *Cfg;

    // Declaration for enclosing function. Having this here allows us to emit
    // the name of the function in any diagnostic message when checking return
    // bounds.
    FunctionDecl *FunctionDeclaration;
    // Return value expression for enclosing function, if any. Having this
    // here allows us to avoid reconstructing a return value for each
    // return statement.
    BoundsValueExpr *ReturnVal;
    // Expanded declared return bounds expression for enclosing function, if
    // any. Having this here allows us to avoid re-expanding the return bounds
    // for each return statement.
    BoundsExpr *ReturnBounds;

    ASTContext &Context;
    std::pair<ComparisonSet, ComparisonSet> &Facts;

    // Having a BoundsWideningAnalysis object here allows us to easily invoke
    // methods for bounds widening and get back the widened bounds info needed
    // for bounds inference/checking.
    BoundsWideningAnalysis BoundsWideningAnalyzer;

    // Having an AbstractSetManager object here allows us to create
    // AbstractSets for lvalue expressions while checking statements.
    AbstractSetManager AbstractSetMgr;

    // Map a field F in a record declaration to the sibling fields of F
    // in whose declared bounds F appears.
    BoundsSiblingFieldsTy BoundsSiblingFields;

    // When this flag is set to true, include the null terminator in the
    // bounds of a null-terminated array.  This is used when calculating
    // physical sizes during casts to pointers to null-terminated arrays.
    bool IncludeNullTerminator;

    void DumpAssignmentBounds(raw_ostream &OS, BinaryOperator *E,
                              BoundsExpr *LValueTargetBounds,
                              BoundsExpr *RHSBounds) {
      OS << "\n";
      E->dump(OS, Context);
      if (LValueTargetBounds) {
        OS << "Target Bounds:\n";
        LValueTargetBounds->dump(OS, Context);
      }
      if (RHSBounds) {
        OS << "RHS Bounds:\n ";
        RHSBounds->dump(OS, Context);
      }
    }

    void DumpBoundsCastBounds(raw_ostream &OS, CastExpr *E,
                              BoundsExpr *Declared, BoundsExpr *NormalizedDeclared,
                              BoundsExpr *SubExprBounds) {
      OS << "\n";
      E->dump(OS, Context);
      if (Declared) {
        OS << "Declared Bounds:\n";
        Declared->dump(OS, Context);
      }
      if (NormalizedDeclared) {
        OS << "Normalized Declared Bounds:\n ";
        NormalizedDeclared->dump(OS, Context);
      }
      if (SubExprBounds) {
        OS << "Inferred Subexpression Bounds:\n ";
        SubExprBounds->dump(OS, Context);
      }
    }

    void DumpInitializerBounds(raw_ostream &OS, VarDecl *D,
                               BoundsExpr *Target, BoundsExpr *B) {
      OS << "\n";
      D->dump(OS);
      OS << "Declared Bounds:\n";
      Target->dump(OS, Context);
      OS << "Initializer Bounds:\n ";
      B->dump(OS, Context);
    }

    void DumpExpression(raw_ostream &OS, Expr *E) {
      OS << "\n";
      E->dump(OS, Context);
    }

    void DumpCallArgumentBounds(raw_ostream &OS, BoundsExpr *Param,
                                Expr *Arg,
                                BoundsExpr *ParamBounds,
                                BoundsExpr *ArgBounds) {
      OS << "\n";
      if (Param) {
        OS << "Original parameter bounds\n";
        Param->dump(OS, Context);
      }
      if (Arg) {
        OS << "Argument:\n";
        Arg->dump(OS, Context);
      }
      if (ParamBounds) {
        OS << "Parameter Bounds:\n";
        ParamBounds->dump(OS, Context);
      }
      if (ArgBounds) {
        OS << "Argument Bounds:\n ";
        ArgBounds->dump(OS, Context);
      }
    }

    void DumpCheckingState(raw_ostream &OS, Stmt *S, CheckingState &State) {
      OS << "\nStatement S:\n";
      S->dump(OS, Context);

      OS << "Observed bounds context after checking S:\n";
      DumpBoundsContext(OS, State.ObservedBounds);

      OS << "Sets of equivalent expressions after checking S:\n";
      if (State.EquivExprs.size() == 0)
        OS << "{ }\n";
      else {
        OS << "{\n";
        for (auto OuterList = State.EquivExprs.begin(); OuterList != State.EquivExprs.end(); ++OuterList) {
          auto ExprList = *OuterList;
          DumpExprsSet(OS, ExprList);
        }
        OS << "}\n";
      }

      OS << "Expressions that produce the same value as S:\n";
      DumpExprsSet(OS, State.SameValue);
    }

    void DumpBoundsContext(raw_ostream &OS, BoundsContextTy &BoundsContext) {
      if (BoundsContext.empty())
        OS << "{ }\n";
      else {
        // The keys in an llvm::DenseMap are unordered.  Create a set of
        // abstract sets in the context sorted lexicographically in order
        // to guarantee a deterministic output so that printing the bounds
        // context can be tested.
        std::vector<const AbstractSet *> OrderedSets;
        for (auto const &Pair : BoundsContext)
          OrderedSets.push_back(Pair.first);
        llvm::sort(OrderedSets.begin(), OrderedSets.end(),
             [] (const AbstractSet *A, const AbstractSet *B) {
               return *(const_cast<AbstractSet *>(A)) < *(const_cast<AbstractSet *>(B));
             });

        OS << "{\n";
        for (auto I = OrderedSets.begin(); I != OrderedSets.end(); ++I) {
          const AbstractSet *A = *I;
          auto It = BoundsContext.find(A);
          if (It == BoundsContext.end())
            continue;
          OS << "LValue Expression:\n";
          A->GetRepresentative()->dump(OS, Context);
          OS << "Bounds:\n";
          It->second->dump(OS, Context);
        }
        OS << "}\n";
      }
    }

    void DumpExprsSet(raw_ostream &OS, ExprSetTy Exprs) {
      if (Exprs.size() == 0)
        OS << "{ }\n";
      else {
        OS << "{\n";
        for (auto I = Exprs.begin(); I != Exprs.end(); ++I) {
          Expr *E = *I;
          E->dump(OS, Context);
        }
        OS << "}\n";
      }
    }

    void DumpSynthesizedMemberAbstractSets(raw_ostream &OS,
                                           AbstractSetSetTy AbstractSets) {
      OS << "\nAbstractSets for member expressions:\n";
      if (AbstractSets.size() == 0)
        OS << "{ }\n";
      else {
        // The keys in an llvm::SmallPtrSet are unordered.  Create a set of
        // abstract sets sorted lexicographically in order to guarantee a
        // deterministic output so that printing the synthesized abstract
        // sets can be tested.
        std::vector<const AbstractSet *> OrderedSets;
        for (auto It : AbstractSets)
          OrderedSets.push_back(It);
        llvm::sort(OrderedSets.begin(), OrderedSets.end(),
             [] (const AbstractSet *A, const AbstractSet *B) {
               return *(const_cast<AbstractSet *>(A)) < *(const_cast<AbstractSet *>(B));
             });
        OS << "{\n";
        for (auto It : OrderedSets) {
          It->PrettyPrint(OS, Context);
          OS << "\n";
        }
        OS << "}\n";
      }
    }

    // Add bounds check to an lvalue expression, if it is an Array_ptr
    // dereference.  The caller has determined that the lvalue is being
    // used in a way that requies a bounds check if the lvalue is an
    // _Array_ptr or _Nt_array_ptr dereference.  The lvalue uses are to read
    // or write memory or as the base expression of a member reference.
    //
    // If the Array_ptr has unknown bounds, this is a compile-time error.
    // Generate an error message and set the bounds to an invalid bounds
    // expression.
    enum class OperationKind {
      Read,   // just reads memory
      Assign, // simple assignment to memory
      Other   // reads and writes memory, struct base check
    };

    bool AddBoundsCheck(Expr *E, OperationKind OpKind, CheckedScopeSpecifier CSS,
                        EquivExprSets *EquivExprs, BoundsExpr *LValueBounds) {
      assert(E->isLValue());
      bool NeedsBoundsCheck = false;
      QualType PtrType;
      if (Expr *Deref = S.GetArrayPtrDereference(E, PtrType)) {
        NeedsBoundsCheck = true;
        LValueBounds = S.CheckNonModifyingBounds(LValueBounds, E);
        BoundsCheckKind Kind = BCK_Normal;
        // Null-terminated array pointers have special semantics for
        // bounds checks.
        if (PtrType->isCheckedPointerNtArrayType()) {
          if (OpKind == OperationKind::Read)
            Kind = BCK_NullTermRead;
          else if (OpKind == OperationKind::Assign)
            Kind = BCK_NullTermWriteAssign;
          // Otherwise, use the default range check for bounds.
        }
        if (LValueBounds->isUnknown()) {
          S.Diag(E->getBeginLoc(), diag::err_expected_bounds) << E->getSourceRange();
          LValueBounds = S.CreateInvalidBoundsExpr();
        } else {
          CheckBoundsAtMemoryAccess(Deref, LValueBounds, Kind, CSS, EquivExprs);
        }
        if (UnaryOperator *UO = dyn_cast<UnaryOperator>(Deref)) {
          assert(!UO->hasBoundsExpr());
          UO->setBoundsExpr(LValueBounds);
          UO->setBoundsCheckKind(Kind);

        } else if (ArraySubscriptExpr *AS = dyn_cast<ArraySubscriptExpr>(Deref)) {
          assert(!AS->hasBoundsExpr());
          AS->setBoundsExpr(LValueBounds);
          AS->setBoundsCheckKind(Kind);
        } else
          llvm_unreachable("unexpected expression kind");
      }
      return NeedsBoundsCheck;
    }

    // Add bounds check to the base expression of a member reference, if the
    // base expression is an Array_ptr dereference.  Such base expressions
    // always need bounds checks, even though their lvalues are only used for an
    // address computation.
    bool AddMemberBaseBoundsCheck(MemberExpr *E, CheckedScopeSpecifier CSS,
                                  EquivExprSets *EquivExprs,
                                  BoundsExpr *BaseLValueBounds,
                                  BoundsExpr *BaseBounds) {
      Expr *Base = E->getBase();
      // E.F
      if (!E->isArrow()) {
        // The base expression only needs a bounds check if it is an lvalue.
        if (Base->isLValue())
          return AddBoundsCheck(Base, OperationKind::Other, CSS,
                                EquivExprs, BaseLValueBounds);
        return false;
      }

      // E->F.  This is equivalent to (*E).F.
      if (Base->getType()->isCheckedPointerArrayType()) {
        BoundsExpr *Bounds = S.CheckNonModifyingBounds(BaseBounds, Base);
        if (Bounds->isUnknown()) {
          S.Diag(Base->getBeginLoc(), diag::err_expected_bounds) << Base->getSourceRange();
          Bounds = S.CreateInvalidBoundsExpr();
        } else {
          CheckBoundsAtMemoryAccess(E, Bounds, BCK_Normal, CSS, EquivExprs);
        }
        E->setBoundsExpr(Bounds);
        return true;
      }

      return false;
    }


    // The result of trying to prove a statement about bounds declarations.
    // The proof system is incomplete, so there are will be statements that
    // cannot be proved true or false.  That's why "maybe" is a result.
    enum class ProofResult {
      True,  // Definitely provable.
      False, // Definitely false (an error)
      Maybe  // We're not sure yet.
    };

    // The kind of statement that we are trying to prove true or false.
    //
    // This enum is used in generating diagnostic messages. If you change the order,
    // update the messages in DiagnosticSemaKinds.td used in
    // ExplainProofFailure
    enum class ProofStmtKind : unsigned {
      BoundsDeclaration,
      StaticBoundsCast,
      ReturnStmt,
      MemoryAccess,
      MemberArrowBase
    };

    // ProofFailure: codes that explain why a statement is false.  This is a
    // bitmask because there may be multiple reasons why a statement false.
    enum class ProofFailure : unsigned {
      None = 0x0,
      LowerBound = 0x1,     // The destination lower bound is below the source lower bound.
      UpperBound = 0x2,     // The destination upper bound is above the source upper bound.
      SrcEmpty = 0x4,       // The source bounds are empty (LB == UB)
      SrcInvalid = 0x8,     // The source bounds are invalid (LB > UB).
      DstEmpty = 0x10,      // The destination bounds are empty (LB == UB).
      DstInvalid = 0x20,    // The destination bounds are invalid (LB > UB).
      Width = 0x40,         // The source bounds are narrower than the destination bounds.
      PartialOverlap = 0x80, // There was only partial overlap of the destination bounds with
                            // the source bounds.
      HasFreeVariables = 0x100 // Source or destination has free variables.
    };

    enum class DiagnosticNameForTarget {
      Destination = 0x0,
      Target = 0x1
    };

    enum class DiagnosticBoundsName {
      Declared,
      Inferred
    };

    enum class DiagnosticBoundsComponent {
      Lower,
      Upper,
      Base
    };

    // Combine proof failure codes.
    static constexpr ProofFailure CombineFailures(ProofFailure A,
                                                  ProofFailure B) {
      return static_cast<ProofFailure>(static_cast<unsigned>(A) |
                                       static_cast<unsigned>(B));
    }

    // Check that all the conditions in "Test" are in the failure code.
    static constexpr bool TestFailure(ProofFailure A, ProofFailure Test) {
      return ((static_cast<unsigned>(A) &  static_cast<unsigned>(Test)) ==
              static_cast<unsigned>(Test));
    }

    // Combine free variable positions.
    static constexpr FreeVariablePosition
    CombineFreeVariablePosition(FreeVariablePosition A,
                                FreeVariablePosition B) {
      return static_cast<FreeVariablePosition>(static_cast<unsigned>(A) |
                                               static_cast<unsigned>(B));
    }

    // Check that all the free variable positions in "Test" are in A.
    static constexpr bool TestFreeVariablePosition(FreeVariablePosition A,
                                                   FreeVariablePosition Test) {
      return ((static_cast<unsigned>(A) & static_cast<unsigned>(Test)) ==
              static_cast<unsigned>(Test));
    }

    static void DumpFailure(raw_ostream &OS, ProofFailure A) {
      OS << "[ ";
      if (TestFailure(A, ProofFailure::LowerBound)) OS << "LowerBound ";
      if (TestFailure(A, ProofFailure::UpperBound)) OS << "UpperBound ";
      if (TestFailure(A, ProofFailure::SrcEmpty)) OS << "SrcEmpty ";
      if (TestFailure(A, ProofFailure::SrcInvalid)) OS << "SrcInvalid ";
      if (TestFailure(A, ProofFailure::DstEmpty)) OS << "DstEmpty ";
      if (TestFailure(A, ProofFailure::DstInvalid)) OS << "DstInvalid ";
      if (TestFailure(A, ProofFailure::Width)) OS << "Width ";
      if (TestFailure(A, ProofFailure::PartialOverlap)) OS << "PartialOverlap ";
      OS << "]";
    }

    // Representation and operations on ranges.
    // A range has the form (e1 + e2, e1 + e3) where e1 is an expression.
    // A range can be either Constant- or Variable-sized.
    //
    // - If e2 and e3 are both constant integer expressions, the range is Constant-sized.
    //   For now, in this case, we represent e2 and e3 as signed (APSInt) integers.
    //   They must have the same bitsize.
    //   More specifically: (UpperOffsetVariable == nullptr && LowerOffsetVariable == nullptr)
    // - If one or both of e2 and e3 are non-constant expressions, the range is Variable-sized.
    //   More specifically: (UpperOffsetVariable != nullptr || LowerOffsetVariable != nullptr)
    class BaseRange {
    public:
      enum Kind {
        ConstantSized,
        VariableSized,
        Invalid
      };

    private:
      Sema &S;
      Expr *Base;
      llvm::APSInt LowerOffsetConstant;
      llvm::APSInt UpperOffsetConstant;
      Expr *LowerOffsetVariable;
      Expr *UpperOffsetVariable;

    public:
      BaseRange(Sema &S) : S(S), Base(nullptr), LowerOffsetConstant(1, true),
        UpperOffsetConstant(1, true), LowerOffsetVariable(nullptr), UpperOffsetVariable(nullptr) {
      }

      BaseRange(Sema &S, Expr *Base,
                         llvm::APSInt &LowerOffsetConstant,
                         llvm::APSInt &UpperOffsetConstant) :
        S(S), Base(Base), LowerOffsetConstant(LowerOffsetConstant), UpperOffsetConstant(UpperOffsetConstant),
        LowerOffsetVariable(nullptr), UpperOffsetVariable(nullptr) {
      }

      BaseRange(Sema &S, Expr *Base,
                         Expr *LowerOffsetVariable,
                         Expr *UpperOffsetVariable) :
        S(S), Base(Base), LowerOffsetConstant(1, true), UpperOffsetConstant(1, true),
        LowerOffsetVariable(LowerOffsetVariable), UpperOffsetVariable(UpperOffsetVariable) {
      }

    private:
      // CompareConstantFoldedUpperOffsets is a fallback method that attempts
      // to prove that R.UpperOffsetVariable <= this.UpperOffsetVariable.
      // It returns true if:
      // 1. this and R are both variable-sized ranges, and:
      // 2. The upper offsets of this and R can both be constant folded
      //    according to the definition of ConstantFoldUpperOffset above, and:
      // 3. The variable parts of the constant folded upper offsets are
      //    equivalent, and:
      // 4. The constant upper part of R <= the constant upper part of this.
      //
      // Since lexicographically comparing variable upper offsets will not
      // account for any constant folding, this method can be used to compare
      // upper offsets that are not lexicographically equivalent.
      //
      // TODO: this method is part of a temporary solution to enable bounds
      // checking to validate bounds such as (p, p + (len + 1) - 1). In the
      // future, we should handle constant folding, commutativity, and
      // associativity in bounds expressions in a more general way.
      bool CompareUpperOffsetsWithConstantFolding(BaseRange &R,
                                                  EquivExprSets *EquivExprs) {
        if (!IsUpperOffsetVariable() || !R.IsUpperOffsetVariable())
          return false;

        Expr *Variable = nullptr;
        llvm::APSInt Constant;
        bool ConstFolded = NormalizeUtil::ConstantFold(S, UpperOffsetVariable,
                                                       Base->getType(),
                                                       Variable, Constant);

        Expr *RVariable = nullptr;
        llvm::APSInt RConstant;
        bool RConstFolded = NormalizeUtil::ConstantFold(S, R.UpperOffsetVariable,
                                                        R.Base->getType(),
                                                        RVariable, RConstant);

        // If neither this nor R had their upper offsets constant folded, then
        // the variable parts will be the respective upper offsets and the
        // constant will both be 0. We already know the upper offsets are not
        // equal from comparing them in CompareUpperOffsets, so there is no
        // need for further comparison here.
        if (!ConstFolded && !RConstFolded)
          return false;

        // The variable parts of both upper offsets must have been set
        // by ConstantFoldUpperOffset in order to compare them.
        if (!Variable || !RVariable)
          return false;

        if (!ExprUtil::EqualValue(S.Context, Variable, RVariable, EquivExprs))
          return false;

        ExprUtil::EnsureEqualBitWidths(Constant, RConstant);
        return RConstant <= Constant;
      }

    public:
      // Is R a subrange of this range?
      ProofResult InRange(BaseRange &R, ProofFailure &Cause, EquivExprSets *EquivExprs,
                          std::pair<ComparisonSet, ComparisonSet>& Facts) {

        // We will warn on declaration of Invalid ranges (upperBound < lowerBound).
        // The following cases are handled by the callers of this function:
        // - Error on memory access to Invalid and Empty ranges
        if (R.IsInvalid()) {
          Cause = CombineFailures(Cause, ProofFailure::DstInvalid);
          return ProofResult::Maybe;
        }

        if (ExprUtil::EqualValue(S.Context, Base, R.Base, EquivExprs)) {
          ProofResult LowerBoundsResult = CompareLowerOffsetsImpl(R, Cause, EquivExprs, Facts);
          ProofResult UpperBoundsResult = CompareUpperOffsetsImpl(R, Cause, EquivExprs, Facts);

          if (LowerBoundsResult == ProofResult::True &&
              UpperBoundsResult == ProofResult::True)
            return ProofResult::True;
          if (LowerBoundsResult == ProofResult::False ||
              UpperBoundsResult == ProofResult::False)
            return ProofResult::False;
        }
        return ProofResult::Maybe;
      }

      // InRangeWithFreeVars is an extension of InRange.  It tries to prove
      // that R is within this range.  If R is not provably within this range,
      // it sets Cause to explain reasons why.  This function takes into
      // account variables that are free in one of the lower/upper bounds
      // expressions that are being compared.
      //
      // InRangeWithFreeVars compares the lower bounds from the two ranges
      // and then the upper bounds from the two ranges.  Given a pair of lower
      // (or upper) bounds expressions, a variable is free if it appears in
      // one expression but does not appear in the other expression, and there
      // is no known relation between that variable and variables in the other
      // expression.  In that situation, it is impossible to prove that the
      // comparison is true, given the facts that the compiler has.
      ProofResult
      InRangeWithFreeVars(BaseRange &R, ProofFailure &Cause,
                          EquivExprSets *EquivExprs,
                          std::pair<ComparisonSet, ComparisonSet> &Facts,
                          FreeVariableListTy &FreeVariables) {
        // If there is no relational information at all, then the compiler
        // didn't attempt to gather any.  To avoid confusing programmers,
        // don't try to take free variables into account.  There may be some
        // simple fact the compiler doesn't know that would cause confusion
        // about why the compiler is claiming that no fact is known.
        if (!EquivExprs)
          return InRange(R, Cause, nullptr, Facts);

        // We will warn on declaration of Invalid ranges (upperBound <
        // lowerBound). The following cases are handled by the callers of this
        // function:
        // - Error on memory access to Invalid and Empty ranges
        if (R.IsInvalid()) {
          Cause = CombineFailures(Cause, ProofFailure::DstInvalid);
          return ProofResult::Maybe;
        }

        FreeVariablePosition BasePos = CombineFreeVariablePosition(
            FreeVariablePosition::Lower, FreeVariablePosition::Upper);
        FreeVariablePosition DeclaredBasePos = CombineFreeVariablePosition(
            FreeVariablePosition::Declared, BasePos);
        FreeVariablePosition ObservedBasePos = CombineFreeVariablePosition(
            FreeVariablePosition::Observed, BasePos);

        if (ExprUtil::EqualValue(S.Context, Base, R.Base, EquivExprs)) {
          ProofResult LowerBoundsResult =
              CompareLowerOffsets(R, Cause, EquivExprs, Facts, FreeVariables);
          ProofResult UpperBoundsResult =
              CompareUpperOffsets(R, Cause, EquivExprs, Facts, FreeVariables);

          if (LowerBoundsResult == ProofResult::True &&
              UpperBoundsResult == ProofResult::True)
            return ProofResult::True;
          if (LowerBoundsResult == ProofResult::False ||
              UpperBoundsResult == ProofResult::False)
            return ProofResult::False;
        } else if (CheckFreeVarInExprs(R.Base, Base, DeclaredBasePos,
                                       ObservedBasePos, EquivExprs,
                                       FreeVariables)) {
          Cause = CombineFailures(Cause, ProofFailure::HasFreeVariables);
          return ProofResult::False;
        }
        return ProofResult::Maybe;
      }

      // This function proves whether this.LowerOffset <= R.LowerOffset.
      // Depending on whether these lower offsets are ConstantSized or VariableSized, various cases should be checked:
      // - If `this` and `R` both have constant lower offsets (i.e., if the following condition holds:
      //   `IsLowerOffsetConstant() && R.IsLowerOffsetConstant()`), the function returns
      //   true only if `LowerOffsetConstant <= R.LowerOffsetConstant`. Otherwise, it should return false.
      // - If `this` and `R` both have variable lower offsets (i.e., if the following condition holds:
      //   `IsLowerOffsetVariable() && R.IsLowerOffsetVariable()`), the function returns true if
      //   `EqualValue()` determines that `LowerOffsetVariable` and `R.LowerOffsetVariable` are equal.
      // - If `this` has a constant lower offset (i.e., `IsLowerOffsetConstant()` is true),
      //   but `R` has a variable lower offset (i.e., `R.IsLowerOffsetVariable()` is true), the function
      //   returns true only if `R.LowerOffsetVariable` has unsgined integer type and `this.LowerOffsetConstant`
      //   has value 0 when it is extended to int64_t.
      // - If none of the above cases happen, it means that the function has not been able to prove
      //   whether this.LowerOffset is less than or equal to R.LowerOffset, or not. Therefore,
      //   it returns maybe as the result.
      ProofResult CompareLowerOffsetsImpl(BaseRange &R, ProofFailure &Cause, EquivExprSets *EquivExprs,
                                      std::pair<ComparisonSet, ComparisonSet> &Facts) {
        if (IsLowerOffsetConstant() && R.IsLowerOffsetConstant()) {
          if (LowerOffsetConstant <= R.LowerOffsetConstant)
            return ProofResult::True;
          Cause = CombineFailures(Cause, ProofFailure::LowerBound);
          return ProofResult::False;
        }
        if (IsLowerOffsetVariable() && R.IsLowerOffsetVariable())
          if (LessThanOrEqualExtended(S.Context, Base, R.Base, LowerOffsetVariable, R.LowerOffsetVariable, EquivExprs, Facts))
            return ProofResult::True;
        if (R.IsLowerOffsetVariable() && IsLowerOffsetConstant() &&
            R.LowerOffsetVariable->getType()->isUnsignedIntegerType() && LowerOffsetConstant.getExtValue() == 0)
          return ProofResult::True;

        return ProofResult::Maybe;
      }

      // This function proves whether R.UpperOffset <= this.UpperOffset.
      // Depending on whether these upper offsets are ConstantSized or VariableSized, various cases should be checked:
      // - If `this` and `R` both have constant upper offsets (i.e., if the following condition holds:
      //   `IsUpperOffsetConstant() && R.IsUpperOffsetConstant()`), the function returns
      //   true only if `R.UpperOffsetConstant <= UpperOffsetConstant`. Otherwise, it should return false.
      // - If `this` and `R` both have variable upper offsets (i.e., if the following condition holds:
      //   `IsUpperOffsetVariable() && R.IsUpperOffsetVariable()`), the function returns true if
      //   `EqualValue()` determines that `UpperOffsetVariable` and `R.UpperOffsetVariable` are equal.
      // - If `R` has a constant upper offset (i.e., `R.IsUpperOffsetConstant()` is true),
      //   but `this` has a variable upper offset (i.e., `IsUpperOffsetVariable()` is true), the function
      //   returns true only if `UpperOffsetVariable` has unsgined integer type and `R.UpperOffsetConstant`
      //   has value 0 when it is extended to int64_t.
      // - If none of the above cases happen, it means that the function has not been able to prove
      //   whether R.UpperOffset is less than or equal to this.UpperOffset, or not. Therefore,
      //   it returns maybe as the result.
      ProofResult CompareUpperOffsetsImpl(BaseRange &R, ProofFailure &Cause, EquivExprSets *EquivExprs,
                                      std::pair<ComparisonSet, ComparisonSet>& Facts) {
        if (IsUpperOffsetConstant() && R.IsUpperOffsetConstant()) {
          if (R.UpperOffsetConstant <= UpperOffsetConstant)
            return ProofResult::True;
          Cause = CombineFailures(Cause, ProofFailure::UpperBound);
          return ProofResult::False;
        }
        if (IsUpperOffsetVariable() && R.IsUpperOffsetVariable())
          if (LessThanOrEqualExtended(S.Context, R.Base, Base, R.UpperOffsetVariable, UpperOffsetVariable, EquivExprs, Facts))
            return ProofResult::True;
        if (IsUpperOffsetVariable() && R.IsUpperOffsetConstant() &&
            UpperOffsetVariable->getType()->isUnsignedIntegerType() && R.UpperOffsetConstant.getExtValue() == 0)
          return ProofResult::True;

        // If we cannot prove that R.UpperOffset <= this.UpperOffset using
        // lexicographic comparison of expressions, attempt to perform simple
        // constant folding operations on the upper offsets.
        // TODO: this method is part of a temporary solution to enable bounds
        // checking to validate bounds such as (p, p + (len + 1) - 1). In the
        // future, we should handle constant folding, commutativity, and
        // associativity in bounds expressions in a more general way.
        if (CompareUpperOffsetsWithConstantFolding(R, EquivExprs))
          return ProofResult::True;

        return ProofResult::Maybe;
      }

      // CompareLowerOffsets first calls CompareLowerOffsetsImpl. If
      // CompareLowerOffsetsImpl returns Maybe, it continues to collect free
      // variables in the lower offsets of this and R.
      ProofResult CompareLowerOffsets(BaseRange &R, ProofFailure &Cause,
                                      EquivExprSets *EquivExprs,
                                      std::pair<ComparisonSet, ComparisonSet> &Facts,
                                      FreeVariableListTy &FreeVariables) {
        ProofResult Result =
            CompareLowerOffsetsImpl(R, Cause, EquivExprs, Facts);
        // If we get a definite result, no need to check free variables.
        if (Result != ProofResult::Maybe)
          return Result;

        FreeVariablePosition DeclaredLowerPos = CombineFreeVariablePosition(
            FreeVariablePosition::Declared, FreeVariablePosition::Lower);
        FreeVariablePosition ObservedLowerPos = CombineFreeVariablePosition(
            FreeVariablePosition::Observed, FreeVariablePosition::Lower);

        if (CheckFreeVarInExprs(R.LowerOffsetVariable, LowerOffsetVariable,
                                DeclaredLowerPos, ObservedLowerPos,
                                EquivExprs, FreeVariables)) {
          Cause = CombineFailures(Cause, ProofFailure::HasFreeVariables);
          return ProofResult::False;
        }

        return ProofResult::Maybe;
      }

      // CompareUpperOffsets first calls CompareUpperOffsetsImpl. If
      // CompareUpperOffsetsImpl returns Maybe, it continues to collect free
      // variables in the upper offsets of this and R.
      ProofResult CompareUpperOffsets(BaseRange &R, ProofFailure &Cause,
                                      EquivExprSets *EquivExprs,
                                      std::pair<ComparisonSet, ComparisonSet> &Facts,
                                      FreeVariableListTy &FreeVariables) {
        ProofResult Result =
            CompareUpperOffsetsImpl(R, Cause, EquivExprs, Facts);
        // If we get a definite result, no need to check free variables.
        if (Result != ProofResult::Maybe || !EquivExprs)
          return Result;

        FreeVariablePosition DeclaredUpperPos = CombineFreeVariablePosition(
            FreeVariablePosition::Declared, FreeVariablePosition::Upper);
        FreeVariablePosition ObservedUpperPos = CombineFreeVariablePosition(
            FreeVariablePosition::Observed, FreeVariablePosition::Upper);

        if (CheckFreeVarInExprs(R.UpperOffsetVariable, UpperOffsetVariable,
                                DeclaredUpperPos, ObservedUpperPos,
                                EquivExprs, FreeVariables)) {
          Cause = CombineFailures(Cause, ProofFailure::HasFreeVariables);
          return ProofResult::False;
        }

        return ProofResult::Maybe;
      }

      // CheckFreeVarInExprs appends any free variables in E1 and any free
      // variables in E2 to FreeVars, and returns true if there are any free
      // variables in either E1 or E2.  Pos1 and Pos2 are the positions in
      // which E1 and E2 appear in bounds expressions.  Free variables are
      // appended with the position of the expression in which they are free
      // (free variables in E1 are appended with Pos1, for example).
      bool CheckFreeVarInExprs(Expr *E1, Expr *E2,
                               FreeVariablePosition Pos1,
                               FreeVariablePosition Pos2,
                               EquivExprSets *EquivExprs,
                               FreeVariableListTy &FreeVars) {
        // If E1 or E2 accesses memory via a pointer, we skip because we cannot
        // determine aliases for two indirect accesses soundly yet.
        // We also skip checking free variables if E1 or E2 is or contains a
        // non-arrow member expression, since the compiler currently does
        // not track equality information for member expressions.
        if (ExprUtil::ReadsMemoryViaPointer(E1, true) ||
            ExprUtil::ReadsMemoryViaPointer(E2, true))
          return false;

        // If E1 or E2 is a _Return_value expression, we skip since we cannot
        // determine the set of variables that occur in these expressions.
        if (ExprUtil::IsReturnValueExpr(E1) || ExprUtil::IsReturnValueExpr(E2))
          return false;

        bool HasFreeVariables = false;
        EqualExprTy Vars1 = CollectVariableSet(S, E1);
        EqualExprTy Vars2 = CollectVariableSet(S, E2);

        if (AddFreeVariables(Vars1, Vars2, EquivExprs, Pos1, FreeVars))
          HasFreeVariables = true;

        if (AddFreeVariables(Vars2, Vars1, EquivExprs, Pos2, FreeVars))
          HasFreeVariables = true;

        return HasFreeVariables;
      }

      // AddFreeVariables creates a pair <Variable, Pos> for each free variable
      // in SrcVars w.r.t. DstVars and appends the pair to FreeVariablesWithPos.
      bool AddFreeVariables(const EqualExprTy &SrcVars,
                            const EqualExprTy &DstVars,
                            EquivExprSets *EquivExprs,
                            FreeVariablePosition Pos,
                            FreeVariableListTy &FreeVariablesWithPos) {
        EqualExprTy FreeVariables;
        if (GetFreeVariables(SrcVars, DstVars, EquivExprs, FreeVariables)) {
          for (const auto V : FreeVariables)
            FreeVariablesWithPos.push_back(std::make_pair(V, Pos));
          return true;
        }
        return false;
      }

      // GetFreeVariables gathers "free variables" in SrcVars.
      //
      // Given two variable sets SrcVars and DstVars, and a set of equivalent
      // sets of Expr EquivExprs. A variable V in SrcVars is *free* if these
      // conditions are met:
      //   1. V is not equal to an integer constant, i.e. there is no set in
      //      EquivExprs that contains V and an IntegerLiteral expression, and:
      //   2. For each variable U in DstVars, V is not equivalent to U, i.e.
      //      there is no set in EquivExprs that contains both V and U, and:
      //   3. For each variable U in DstVars, there is no indirect relationship
      //      between V and U, i.e. there is no set in EquivExprs that contains
      //      two different expressions e1 and e2, where e1 uses the value of
      //      V and e2 uses the value of U.
      //
      // GetFreeVariables returns true if any free variable is found in SrcVars,
      // and appends the free variables to FreeVariables.
      bool GetFreeVariables(const EqualExprTy &SrcVars,
                            const EqualExprTy &DstVars,
                            EquivExprSets *EquivExprs,
                            EqualExprTy &FreeVariables) {
        bool HasFreeVariables = false;

        // Gather free variables.
        for (const auto &SrcV : SrcVars) {
          DeclRefExpr *SrcVar = cast<DeclRefExpr>(SrcV);
          if (IsEqualToConstant(SrcVar, EquivExprs))
            continue;
          auto It = DstVars.begin();
          for (; It != DstVars.end(); It++) {
            if (ExprUtil::EqualValue(S.Context, SrcV, *It, EquivExprs))
              break;
          }

          if (It == DstVars.end()) {
            // If SrcV is not equal to a constant or a variable in DstVars,
            // check if there is an indirect relationship between SrcV and
            // a variable in DstVars. If there is, SrcV is not a free variable.
            if (!FindVarRelationship(SrcVar, DstVars, EquivExprs)) {
              HasFreeVariables = true;
              FreeVariables.push_back(SrcV);
            }
          }
        }
        return HasFreeVariables;
      }

      // FindVarRelationship returns true if there is any relationship
      // between the variable SrcV and any variable in the list DstVars.
      //
      // If EquivExprs contains a set { e1, e2 } where e1 uses the value
      // of SrcV and e2 uses the value of DstV, where DstV is a variable in
      // DstVars, then there is a relationship between SrcV and DstV.
      //
      // For example, if DstV is a variable in DstVars and EquivExprs
      // contains the set { SrcV + 1, &DstV }, then there is a relationship
      // between SrcV and DstV.
      bool FindVarRelationship(DeclRefExpr *SrcV,
                               const EqualExprTy &DstVars,
                               EquivExprSets *EquivExprs) {
        auto Begin = EquivExprs->begin(), End = EquivExprs->end();
        for (auto OuterList = Begin; OuterList != End; ++OuterList) {
          auto InnerList = *OuterList;
          int InnerListSize = InnerList.size();
          unsigned int SrcVarCount = 0;
          int SrcIndex = 0;

          // Search InnerList for an expression that uses the value of SrcV.
          for (; SrcIndex < InnerListSize; ++SrcIndex) {
            Expr *E = InnerList[SrcIndex];
            SrcVarCount = ExprUtil::VariableOccurrenceCount(S, SrcV, E);
            if (SrcVarCount > 0)
              break;
          }
          if (SrcVarCount == 0)
            continue;

          // Search InnerList (except for InnerList[SrcIndex]) for an
          // expression that uses the value of any variable in DstVars.
          // If InnerList[SrcIndex] uses the value of any variable in DstVars,
          // that is not sufficient to imply a relationship between SrcV and
          // any variable in DstVars.
          for (int DstIndex = 0; DstIndex < InnerListSize; ++DstIndex) {
            if (DstIndex == SrcIndex) 
              continue;
            for (auto I = DstVars.begin(); I != DstVars.end(); ++I) {
              DeclRefExpr *DstV = cast<DeclRefExpr>(*I);
              Expr *E = InnerList[DstIndex];
              if (ExprUtil::VariableOccurrenceCount(S, DstV, E) > 0)
                return true;
            }
          }
        }

        return false;
      }

      // IsEqualToConstant returns true if Variable has integer type and
      // produces the same value as some integer constant.
      //
      // Variable produces the same value as an integer constant if
      // EquivExprs contains a set that contains an rvalue cast of Variable
      // and an IntegerLiteral expression.
      bool IsEqualToConstant(DeclRefExpr *Variable,
                             const EquivExprSets *EquivExprs) {
        if (Variable->getType()->isPointerType() || !EquivExprs)
          return false;

        // Get the set (if any) in EquivExprs that contains an rvalue
        // cast of Variable.
        EqualExprTy EquivSet = { };
        for (auto OuterList = EquivExprs->begin(); OuterList != EquivExprs->end(); ++OuterList) {
          auto InnerList = *OuterList;
          for (auto I = InnerList.begin(); I != InnerList.end(); ++I) {
            Expr *E = *I;
            if (VariableUtil::IsRValueCastOfVariable(S, E, cast<DeclRefExpr>(Variable))) {
              EquivSet = InnerList;
              break;
            }
          }
        }

        for (const auto &E : EquivSet) {
          if (E->isIntegerConstantExpr(S.Context))
            return true;
        }

        return false;
      }

      bool IsConstantSizedRange() {
        return IsLowerOffsetConstant() && IsUpperOffsetConstant();
      }

      bool IsVariableSizedRange() {
        return IsLowerOffsetVariable() || IsUpperOffsetVariable();
      }

      bool IsLowerOffsetConstant() {
        return !LowerOffsetVariable;
      }

      bool IsLowerOffsetVariable() {
        return LowerOffsetVariable;
      }

      bool IsUpperOffsetConstant() {
        return !UpperOffsetVariable;
      }

      bool IsUpperOffsetVariable() {
        return UpperOffsetVariable;
      }

      // This function returns true if, when the range is ConstantSized,
      // `UpperOffsetConstant == LowerOffsetConstant`.
      // Currently, it returns false when the range is not ConstantSized.
      // However, this should be generalized in the future.
      bool IsEmpty() {
        if (IsConstantSizedRange())
          return UpperOffsetConstant == LowerOffsetConstant;
        // TODO: can we generalize IsEmpty to non-constant ranges?
        return false;
      }

      // This function returns true if, when the range is ConstantSized,
      // `UpperOffsetConstant < LowerOffsetConstant`.
      // Currently, it returns false when the range is not ConstantSized.
      // However, this should be generalized in the future.
      bool IsInvalid() {
        if (IsConstantSizedRange())
          return UpperOffsetConstant < LowerOffsetConstant;
        // TODO: can we generalize IsInvalid to non-constant ranges?
        return false;
      }

      // Does R partially overlap this range?
      ProofResult PartialOverlap(BaseRange &R) {
        if (Lexicographic(S.Context, nullptr).CompareExpr(Base, R.Base) ==
            Lexicographic::Result::Equal) {
          // TODO: can we generalize this function to non-constant ranges?
          if (IsConstantSizedRange() && R.IsConstantSizedRange()) {
            if (!IsEmpty() && !R.IsEmpty() && !IsInvalid() && !R.IsInvalid()) {
              // R.LowerOffset is within this range, but R.UpperOffset is above the range
              if (LowerOffsetConstant <= R.LowerOffsetConstant && R.LowerOffsetConstant < UpperOffsetConstant &&
                  UpperOffsetConstant < R.UpperOffsetConstant)
                return ProofResult::True;
              // Or R.UpperOffset is within this range, but R.LowerOffset is below the range.
              if (LowerOffsetConstant < R.UpperOffsetConstant && R.UpperOffsetConstant <= UpperOffsetConstant &&
                  R.LowerOffsetConstant < LowerOffsetConstant)
                return ProofResult::True;
            }
          }
          return ProofResult::False;
        }
        return ProofResult::Maybe;
      }

      bool AddToUpper(llvm::APSInt &Num) {
        bool Overflow;
        UpperOffsetConstant = UpperOffsetConstant.sadd_ov(Num, Overflow);
        return Overflow;
      }

      llvm::APSInt GetWidth() {
        return UpperOffsetConstant - LowerOffsetConstant;
      }

      void SetBase(Expr *B) {
        Base = B;
      }

      void SetLowerConstant(llvm::APSInt &Lower) {
        LowerOffsetConstant = Lower;
      }

      void SetUpperConstant(llvm::APSInt &Upper) {
        UpperOffsetConstant = Upper;
      }

      void SetLowerVariable(Expr *Lower) {
        LowerOffsetVariable = Lower;
      }

      void SetUpperVariable(Expr *Upper) {
        UpperOffsetVariable = Upper;
      }

      void Dump(raw_ostream &OS) {
        OS << "Range:\n";
        OS << "Base: ";
        if (Base)
          Base->dump(OS, S.getASTContext());
        else
          OS << "nullptr\n";
        if (IsLowerOffsetConstant()) {
          SmallString<12> Str;
          LowerOffsetConstant.toString(Str);
          OS << "Lower offset:" << Str << "\n";
        }
        if (IsUpperOffsetConstant()) {
          SmallString<12> Str;
          UpperOffsetConstant.toString(Str);
          OS << "Upper offset:" << Str << "\n";
        }
        if (IsLowerOffsetVariable()) {
          OS << "Lower offset:\n";
          LowerOffsetVariable->dump(OS, S.getASTContext());
        }
        if (IsUpperOffsetVariable()) {
          OS << "Upper offset:\n";
          UpperOffsetVariable->dump(OS, S.getASTContext());
        }
      }
    };



    // This function splits the expression `E` into an expression `Base`, and an offset.
    // The offset can be an integer constant or not. If it is an integer constant, the
    // extracted offset can be found in `OffsetConstant`, and `OffsetVariable` will be nullptr.
    // In this case, the return value is `BaseRange::Kind::ConstantSized`.
    // Otherwise, the extracted offset can be found in `OffsetVariable`, and `OffsetConstant`
    // will not be updated. In this case, the return value is `BaseRange::Kind::VariableSized`.
    //
    // Implementation details:
    // - If `E` is a BinaryOperator with an additive opcode, depending on whether the LHS or RHS
    //   is a pointer, Base and offset can get different values in different cases:
    //
    //    First, for extracting the `Base`,
    //     1a. if E.LHS is a pointer, Base = E.LHS.
    //     2a. if E.RHS is a pointer, Base = E.RHS.
    //     If (1a) and (2a) do not hold, Base = E and OffsetConstant = 0 and OffsetVariable = nullptr. Also,
    //     `BaseRange::Kind::ConstantSized` will be returned.
    //
    //    Next, for extracting the offset,
    //     1b. if E.LHS is a pointer and E.RHS is a constant integer,
    //         or, if E.RHS is a pointer and E.LHS is a constant integer, the function will set
    //        `OffsetConstant` to the constant integer and widen and/or normalize it if needed.
    //        Then, it returns `BaseRange::Kind::ConstantSized`. When manipulating the extracted
    //        constant integer, if an overflow occurres in any of the steps, `OffsetConstant = 0`
    //        and `OffsetVariable = nullptr`. Also, `BaseRange::Kind::ConstantSized` will be returned.
    //     If (1b) does not hold, we define the offset to be VariableSized. Therefore,
    //     `OffsetVariable = E.RHS` if E.LHS is a pointer, and `OffsetVariable = E.LHS` if E.RHS is
    //     a pointer. In this case, `BaseRange::Kind::VariableSized` will be returned.
    //
    // TODO: we use signed integers to represent the result of the OffsetConstant.
    // We can't represent unsigned offsets larger the the maximum signed
    // integer that will fit pointer width.
    BaseRange::Kind SplitIntoBaseAndOffset(Expr *E, Expr *&Base, llvm::APSInt &OffsetConstant,
                                Expr *&OffsetVariable) {
      if (const BinaryOperator *BO = dyn_cast<BinaryOperator>(E->IgnoreParens())) {
        if (BO->isAdditiveOp()) {
          Expr *Other = nullptr;
          if (BO->getLHS()->getType()->isPointerType()) {
            Base = BO->getLHS();
            Other = BO->getRHS();
          } else if (BO->getRHS()->getType()->isPointerType()) {
            Base = BO->getRHS();
            Other = BO->getLHS();
          } else
            goto exit;
          assert(Other->getType()->isIntegerType());
          Optional<llvm::APSInt> OptOffsetConstant = Other->getIntegerConstantExpr(S.Context);
          if (OptOffsetConstant) {
            OffsetConstant = *OptOffsetConstant;
            // Widen the integer to the number of bits in a pointer.
            bool Overflow;
            OffsetConstant = ExprUtil::ConvertToSignedPointerWidth(S.Context, OffsetConstant, Overflow);
            if (Overflow)
              goto exit;
            // Normalize the operation by negating the offset if necessary.
            if (BO->getOpcode() == BO_Sub) {
              OffsetConstant = llvm::APSInt(PointerWidth, false).ssub_ov(OffsetConstant, Overflow);
              if (Overflow)
                goto exit;
            }
            llvm::APSInt ElemSize;
            if (!ExprUtil::getReferentSizeInChars(S.Context, Base->getType(), ElemSize))
                goto exit;
            OffsetConstant = OffsetConstant.smul_ov(ElemSize, Overflow);
            if (Overflow)
              goto exit;
            OffsetVariable = nullptr;
            return BaseRange::Kind::ConstantSized;
          } else {
            OffsetVariable = Other;
            return BaseRange::Kind::VariableSized;
          }
        }
      }

    exit:
      // Return (E, 0).
      Base = E->IgnoreParens();
      OffsetConstant = llvm::APSInt(PointerWidth, false);
      OffsetVariable = nullptr;
      return BaseRange::Kind::ConstantSized;
    }

    // Given a `Base` and `Offset`, this function tries to convert it to a standard form `Base + (ConstantPart OP VariablePart)`,
    // where OP is either signed multiplication or unsigned multiplication.
    // The OP's signedness is stored in IsOpSigned. If the function fails to create the standard form, it returns false. Otherwise,
    // it returns true to indicate success, and stores each part of the standard form in a separate argument as follows:
    // `ConstantPart`: a signed integer
    // `IsOpSigned`: a boolean which is true if VariablePart is signed, and false otherwise
    // `VariablePart`: an integer expression that can be a either signed or unsigned integer
    //
    // Given array_ptr<T> p:
    // 1. For (char *)p + e1 or (unsigned char *)p + e1, ConstantPart = 1, VariablePart = e1.
    // 2. For p + e1, ConstantPart = sizeof(T), VariablePart = e1.
    //
    // Note that another way to interpret the functionality of this function is that it expands
    // pointer arithmetic to bytewise arithmetic.
    static bool CreateStandardForm(ASTContext &Ctx, Expr *Base, Expr *Offset, llvm::APSInt &ConstantPart, bool &IsOpSigned, Expr *&VariablePart) {
      bool Overflow;
      uint64_t PointerWidth = Ctx.getTargetInfo().getPointerWidth(0);
      if (!Base->getType()->isPointerType())
        return false;
      if (Base->getType()->getPointeeOrArrayElementType()->isCharType()) {
        if (BinaryOperator *BO = dyn_cast<BinaryOperator>(Offset->IgnoreParens())) {
          // Check to see if Offset is already of the form e * i or i * e. In this case,
          // ConstantPart = i, VariablePart = e.
          if (BinaryOperator::isMultiplicativeOp(BO->getOpcode())) {
            Optional<llvm::APSInt> OptConstantPart;
            if (OptConstantPart = BO->getRHS()->getIntegerConstantExpr(Ctx)) {
              ConstantPart = *OptConstantPart;
              VariablePart = BO->getLHS();
	    } else if (OptConstantPart = BO->getLHS()->getIntegerConstantExpr(Ctx)) {
              ConstantPart = *OptConstantPart;
              VariablePart = BO->getRHS();
	    } else
              goto fallback_std_form;
            IsOpSigned = VariablePart->getType()->isSignedIntegerType();
            ConstantPart = ExprUtil::ConvertToSignedPointerWidth(Ctx, ConstantPart, Overflow);
            if (Overflow)
              goto fallback_std_form;
          } else
            goto fallback_std_form;
        } else
          goto fallback_std_form;
        return true;

      fallback_std_form:
        VariablePart = Offset;
        ConstantPart = llvm::APSInt(llvm::APInt(PointerWidth, 1), false);
        IsOpSigned = VariablePart->getType()->isSignedIntegerType();
        return true;
      } else {
        VariablePart = Offset;
        IsOpSigned = VariablePart->getType()->isSignedIntegerType();
        if (!ExprUtil::getReferentSizeInChars(Ctx, Base->getType(), ConstantPart))
          return false;
        ConstantPart = ExprUtil::ConvertToSignedPointerWidth(Ctx, ConstantPart, Overflow);
        if (Overflow)
          return false;
        return true;
      }
    }

    // In this function, the goal is to compare two expressions: `Base1 + Offset1` and `Base2 + Offset2`.
    // The function returns true if they are equal, and false otherwise. Note that before checking equivalence
    // of expressions, the function expands pointer arithmetic to bytewise arithmetic.
    //
    // Steps in checking the equivalence:
    // 0. If `Offset1` or `Offset2` is null, return false.
    // 1. If `Base1` and `Base2` are not lexicographically equal, return false.
    // 2. Next, both bounds are converted into standard forms `Base + ConstantPart * VariablePart` as explained in `CreateStandardForm()`
    // 3. If any of the expressions cannot be converted successfully, return false.
    // 4. If VariableParts are not lexicographically equal, return false.
    // 5. If OP signs are not equivalent in both, return false.
    // 6. If ConstantParts are not equal, return false.
    // If the expressions pass all the above tests, then return true.
    //
    // Note that in all steps involving in checking the equality of the types or values of offsets, parentheses and
    // casts are ignored.
    static bool EqualExtended(ASTContext &Ctx, Expr *Base1, Expr *Base2, Expr *Offset1, Expr *Offset2, EquivExprSets *EquivExprs) {
      if (!Offset1 && !Offset2)
        return false;

      if (!ExprUtil::EqualValue(Ctx, Base1, Base2, EquivExprs))
        return false;

      llvm::APSInt ConstantPart1, ConstantPart2;
      bool IsOpSigned1, IsOpSigned2;
      Expr *VariablePart1, *VariablePart2;

      bool CreatedStdForm1 = CreateStandardForm(Ctx, Base1, Offset1, ConstantPart1, IsOpSigned1, VariablePart1);
      bool CreatedStdForm2 = CreateStandardForm(Ctx, Base2, Offset2, ConstantPart2, IsOpSigned2, VariablePart2);
      
      if (!CreatedStdForm1 || !CreatedStdForm2)
        return false;
      if (!ExprUtil::EqualValue(Ctx, VariablePart1, VariablePart2, EquivExprs))
        return false;
      if (IsOpSigned1 != IsOpSigned2)
        return false;
      if (ConstantPart1 != ConstantPart2)
        return false;

      return true;
    }

    // This function is an extension of EqualExtended. It looks into the provided `Facts`
    // in order to prove `Base1 + Offset1 <= Base2 + Offset2`. Note that in order to prove this,
    // Base1 must equal Base2 (as in EqualExtended), and the fact that
    // "Offset1 <= Offset2" must exist in `Facts`.
    //
    // TODO: we are ignoring the possibility of overflow in the addition.
    static bool LessThanOrEqualExtended(ASTContext &Ctx, Expr *Base1, Expr *Base2, Expr *Offset1, Expr *Offset2,
                                        EquivExprSets *EquivExprs, std::pair<ComparisonSet, ComparisonSet>& Facts) {
      if (!Offset1 && !Offset2)
        return false;

      if (!ExprUtil::EqualValue(Ctx, Base1, Base2, EquivExprs))
        return false;

      llvm::APSInt ConstantPart1, ConstantPart2;
      bool IsOpSigned1, IsOpSigned2;
      Expr *VariablePart1, *VariablePart2;

      bool CreatedStdForm1 = CreateStandardForm(Ctx, Base1, Offset1, ConstantPart1, IsOpSigned1, VariablePart1);
      bool CreatedStdForm2 = CreateStandardForm(Ctx, Base2, Offset2, ConstantPart2, IsOpSigned2, VariablePart2);

      if (!CreatedStdForm1 || !CreatedStdForm2)
        return false;
      if (IsOpSigned1 != IsOpSigned2)
        return false;
      if (ConstantPart1 != ConstantPart2)
        return false;

      if (ExprUtil::EqualValue(Ctx, VariablePart1, VariablePart2, EquivExprs))
        return true;
      if (FactExists(Ctx, VariablePart1, VariablePart2, EquivExprs, Facts))
        return true;

      return false;
    }

    // Given `Facts`, `E1`, and `E2`, this function looks for the fact `E1 <= E2` inside
    // the fatcs and returns true if it is able to find it. Otherwise, it returns false.
    static bool FactExists(ASTContext &Ctx, Expr *E1, Expr *E2, EquivExprSets *EquivExprs,
                           std::pair<ComparisonSet, ComparisonSet>& Facts) {
      bool ExistsIn = false, ExistsKill = false;
      for (auto InFact : Facts.first) {
        if (Lexicographic(Ctx, EquivExprs).CompareExpr(E1, InFact.first) == Lexicographic::Result::Equal &&
            Lexicographic(Ctx, EquivExprs).CompareExpr(E2, InFact.second) == Lexicographic::Result::Equal) {
          ExistsIn = true;
          break;
        }
      }
      for (auto KillFact : Facts.second) {
        if (Lexicographic(Ctx, EquivExprs).CompareExpr(E1, KillFact.first) == Lexicographic::Result::Equal &&
            Lexicographic(Ctx, EquivExprs).CompareExpr(E2, KillFact.second) == Lexicographic::Result::Equal) {
          ExistsKill = true;
          break;
        }
      }
      return ExistsIn && !ExistsKill;
    }

    // Convert the bounds expression `Bounds` to a range `R`. This function returns true
    // if the conversion is successful, and false otherwise.
    // Currently, this function only performs the conversion for bounds expression of
    // kind Range and returns false for other kinds.
    //
    // Implementation details:
    // - First, SplitIntoBaseAndOffset is called on lower and upper fields in BoundsExpr to extract
    //   the bases and offsets. Note that offsets can be either ConstantSized or VariablesSized.
    // - Next, if the extracted lower base and upper base are equal, the function sets the base and
    //   the offsets of `R` based on the extracted values. Finally, it returns true to indicate success.
    //   If bases are not equal, R's fields will not be updated and the function returns false.
    bool CreateBaseRange(const BoundsExpr *Bounds, BaseRange *R,
                             EquivExprSets *EquivExprs) {
      switch (Bounds->getKind()) {
        case BoundsExpr::Kind::Invalid:
        case BoundsExpr::Kind::Unknown:
        case BoundsExpr::Kind::Any:
          return false;
        case BoundsExpr::Kind::ByteCount:
        case BoundsExpr::Kind::ElementCount:
          // TODO: fill these cases in.
          return false;
        case BoundsExpr::Kind::Range: {
          const RangeBoundsExpr *RB = cast<RangeBoundsExpr>(Bounds);
          Expr *Lower = RB->getLowerExpr();
          Expr *Upper = RB->getUpperExpr();
          Expr *LowerBase, *UpperBase;
          llvm::APSInt LowerOffsetConstant(1, true);
          llvm::APSInt  UpperOffsetConstant(1, true);
          Expr *LowerOffsetVariable = nullptr;
          Expr *UpperOffsetVariable = nullptr;
          SplitIntoBaseAndOffset(Lower, LowerBase, LowerOffsetConstant, LowerOffsetVariable);
          SplitIntoBaseAndOffset(Upper, UpperBase, UpperOffsetConstant, UpperOffsetVariable);

          // If both of the offsets are constants, the range is considered constant-sized.
          // Otherwise, it is a variable-sized range.
          if (ExprUtil::EqualValue(S.Context, LowerBase, UpperBase, EquivExprs)) {
            R->SetBase(LowerBase);
            R->SetLowerConstant(LowerOffsetConstant);
            R->SetLowerVariable(LowerOffsetVariable);
            R->SetUpperConstant(UpperOffsetConstant);
            R->SetUpperVariable(UpperOffsetVariable);
            return true;
          }
        }
      }
      return false;
    }

    // Methods to try to prove that an inferred bounds expression implies
    // the validity of a target bounds expression.

    // Try to prove that SrcBounds implies the validity of DeclaredBounds.
    //
    // If Kind is StaticBoundsCast, check whether a static cast between Ptr
    // types from SrcBounds to DestBounds is legal.
    // 
    // If any free variable is found in SrcBounds or DeclaredBounds, return
    // False and add the free variables to FreeVariables.
    ProofResult ProveBoundsDeclValidity(
                const BoundsExpr *DeclaredBounds, 
                const BoundsExpr *SrcBounds,
                ProofFailure &Cause, EquivExprSets *EquivExprs,
                FreeVariableListTy &FreeVariables,
                ProofStmtKind Kind = ProofStmtKind::BoundsDeclaration) {
      assert(BoundsUtil::IsStandardForm(DeclaredBounds) &&
        "declared bounds not in standard form");
      assert(BoundsUtil::IsStandardForm(SrcBounds) &&
        "src bounds not in standard form");
      Cause = ProofFailure::None;

      // Ignore invalid bounds.
      if (SrcBounds->isInvalid() || DeclaredBounds->isInvalid())
        return ProofResult::True;

     // source bounds(any) implies that any other bounds is valid.
      if (SrcBounds->isAny())
        return ProofResult::True;

      // target bounds(unknown) implied by any other bounds.
      if (DeclaredBounds->isUnknown())
        return ProofResult::True;

      if (S.Context.EquivalentBounds(DeclaredBounds, SrcBounds, EquivExprs))
        return ProofResult::True;

      BaseRange DeclaredRange(S);
      BaseRange SrcRange(S);

      if (CreateBaseRange(DeclaredBounds, &DeclaredRange, EquivExprs) &&
          CreateBaseRange(SrcBounds, &SrcRange, EquivExprs)) {

#ifdef TRACE_RANGE
        llvm::outs() << "Found constant ranges:\n";
        llvm::outs() << "Declared bounds";
        DeclaredBounds->dump(llvm::outs(), Context);
        llvm::outs() << "\nSource bounds";
        SrcBounds->dump(llvm::outs(), Context);
        llvm::outs() << "\nDeclared range:";
        DeclaredRange.Dump(llvm::outs());
        llvm::outs() << "\nSource range:";
        SrcRange.Dump(llvm::outs());
#endif
        ProofResult R = SrcRange.InRangeWithFreeVars(
            DeclaredRange, Cause, EquivExprs, Facts, FreeVariables);
        if (R == ProofResult::True)
          return R;
        if (R == ProofResult::False || R == ProofResult::Maybe) {
          if (R == ProofResult::False && SrcRange.IsEmpty())
            Cause = CombineFailures(Cause, ProofFailure::SrcEmpty);
          if (SrcRange.IsInvalid())
            Cause = CombineFailures(Cause, ProofFailure::SrcInvalid);
          if (DeclaredRange.IsConstantSizedRange() && SrcRange.IsConstantSizedRange()) {
            if (DeclaredRange.GetWidth() > SrcRange.GetWidth()) {
              Cause = CombineFailures(Cause, ProofFailure::Width);
              R = ProofResult::False;
            } else if (Kind == ProofStmtKind::StaticBoundsCast) {
              // For checking static casts between Ptr types, we only need to
              // prove that the declared width <= the source width.
              return ProofResult::True;
            }
          }
        }
        return R;
      } else if (CompareNormalizedBounds(DeclaredBounds, SrcBounds, EquivExprs))
        return ProofResult::True;
      return ProofResult::Maybe;
    }

    // Try to prove that RetExprBounds implies the validity of ReturnBounds
    // (the declared return bounds for the enclosing function).
    ProofResult ProveReturnBoundsValidity(Expr *RetExpr,
                                          BoundsExpr *RetExprBounds,
                                          const EquivExprSets EQ,
                                          const EqualExprTy G,
                                          ProofFailure &Cause,
                                          FreeVariableListTy &FreeVariables) {
      // Check some basic properties of the declared ReturnBounds and the
      // source RetExprBounds. Even though these checks will also be done
      // in ProveBoundsDeclValidity, if any of these checks result in an
      // early return, we can avoid constructing a modified EquivExprs set
      // that records equality between RetVal and RetExpr.
      
      // Null declared return bounds or declared return bounds(unknown) 
      // implied by any other bounds.
      if (!ReturnBounds || ReturnBounds->isUnknown())
        return ProofResult::True;

      // Ignore invalid bounds.
      if (RetExprBounds->isInvalid() || ReturnBounds->isInvalid())
        return ProofResult::True;

      // Return expression bounds(any) implies any declared return bounds.
      if (RetExprBounds->isAny())
        return ProofResult::True;

      // Return expression bounds(unknown) cannot imply any non-unknown
      // declared return bounds.
      if (RetExprBounds->isUnknown())
        return ProofResult::False;

      // Record equality between ReturnVal and all expressions that produce
      // the same value as RetExpr. This allows ProveBoundsDeclValidity to
      // validate bounds that depend on the return value of the function.
      // For example, if the declared function bounds are count(1), then the
      // expanded ReturnBounds will be bounds(RetVal, RetVal + 1).
      EquivExprSets EquivExprs = EQ;

      // Determine the set of expressions that produce the same value as
      // RetExpr.
      // If G (the set of expressions that produce the same value as RetExpr)
      // is empty, then RetExpr may be an expression that is not allowed to
      // be recorded in State.EquivExprs (e.g. an expression that reads memory
      // via a pointer). The EquivExprs set that we construct here is temporary
      // and is only used to check the return bounds for one return statement,
      // so we can add RetExpr to EquivExprs in this case.
      EqualExprTy RetSameValue = G;
      if (RetSameValue.size() == 0)
        RetSameValue.push_back(RetExpr);

      bool FoundRetExpr = false;
      for (auto F = EquivExprs.begin(); F != EquivExprs.end(); ++F) {
        if (DoExprSetsIntersect(*F, RetSameValue)) {
          // Add all expressions in RetSameValue to F that are not already in F.
          for (Expr *E : RetSameValue)
            if (!EqualExprsContainsExpr(*F, E))
              F->push_back(E);
          F->push_back(ReturnVal);
          FoundRetExpr = true;
          break;
        }
      }
      if (!FoundRetExpr) {
        EqualExprTy F = RetSameValue;
        F.push_back(ReturnVal);
        EquivExprs.push_back(F);
      }

      return ProveBoundsDeclValidity(ReturnBounds, RetExprBounds, Cause,
                                     &EquivExprs, FreeVariables,
                                     ProofStmtKind::ReturnStmt);
    }

    // CompareNormalizedBounds returns true if SrcBounds implies DeclaredBounds
    // after applying certain transformations to the upper bound expressions
    // of both bounds.
    bool CompareNormalizedBounds(const BoundsExpr *DeclaredBounds,
                                 const BoundsExpr *SrcBounds,
                                 EquivExprSets *EquivExprs) {
      // DeclaredBounds and SrcBounds must both be range bounds in order
      // to normalize their upper bound expression.
      const RangeBoundsExpr *DeclaredRangeBounds =
        dyn_cast<RangeBoundsExpr>(DeclaredBounds);
      if (!DeclaredRangeBounds)
        return false;
      const RangeBoundsExpr *SrcRangeBounds =
        dyn_cast<RangeBoundsExpr>(SrcBounds);
      if (!SrcRangeBounds)
        return false;

      // The lower bound expressions must be equivalent.
      if (!ExprUtil::EqualValue(S.Context, DeclaredRangeBounds->getLowerExpr(),
                                SrcRangeBounds->getLowerExpr(), EquivExprs))
        return false;

      // Attempt to get a variable part and a constant part from the
      // declared upper bound and the source upper bound.
      Expr *DeclaredVariable = nullptr;
      llvm::APSInt DeclaredConstant;
      bool DeclaredNormalized =
        NormalizeUtil::GetVariableAndConstant(S, DeclaredRangeBounds->getUpperExpr(),
                                              DeclaredVariable, DeclaredConstant);
      Expr *SrcVariable = nullptr;
      llvm::APSInt SrcConstant;
      bool SrcNormalized =
        NormalizeUtil::GetVariableAndConstant(S, SrcRangeBounds->getUpperExpr(),
                                              SrcVariable, SrcConstant);

      // We must be able to normalize at least one of the upper bounds in
      // order to compare them.
      if (!DeclaredNormalized && !SrcNormalized)
        return false;

      // Both upper bounds must have a Variable part.
      if (!DeclaredVariable || !SrcVariable)
        return false;

      // The variable parts of the upper bounds must be equivalent.
      if (!ExprUtil::EqualValue(S.Context, DeclaredVariable, SrcVariable, EquivExprs))
        return false;

      // SrcBounds implies DeclaredBounds if and only if the declared upper
      // constant part is less than or equal to the source upper constant part.
      ExprUtil::EnsureEqualBitWidths(DeclaredConstant, SrcConstant);
      return DeclaredConstant <= SrcConstant;
    }

// Try to prove that PtrBase + Offset is within Bounds, where PtrBase has pointer type.
// Offset is optional and may be a nullptr.
    ProofResult ProveMemoryAccessInRange(Expr *PtrBase, Expr *Offset, BoundsExpr *Bounds,
                                         BoundsCheckKind Kind, EquivExprSets *EquivExprs,
                                         ProofFailure &Cause) {
#ifdef TRACE_RANGE
      llvm::outs() << "Examining:\nPtrBase\n";
      PtrBase->dump(llvm::outs(), Context);
      llvm::outs() << "Offset = ";
      if (Offset != nullptr) {
        Offset->dump(llvm::outs(), Context);
      } else
        llvm::outs() << "nullptr\n";
      llvm::outs() << "Bounds\n";
      Bounds->dump(llvm::outs(), Context);
#endif
      assert(BoundsUtil::IsStandardForm(Bounds) &&
             "bounds not in standard form");
      Cause = ProofFailure::None;
      BaseRange ValidRange(S);

      // Currently, we do not try to prove whether the memory access is in range for non-constant ranges
      // TODO: generalize memory access range check to non-constants
      if (!CreateBaseRange(Bounds, &ValidRange, nullptr))
        return ProofResult::Maybe;
      if (ValidRange.IsVariableSizedRange())
        return ProofResult::Maybe;

      bool Overflow;
      llvm::APSInt ElementSize;
      if (!ExprUtil::getReferentSizeInChars(S.Context, PtrBase->getType(), ElementSize))
          return ProofResult::Maybe;
      if (Kind == BoundsCheckKind::BCK_NullTermRead || Kind == BoundsCheckKind::BCK_NullTermWriteAssign) {
        Overflow = ValidRange.AddToUpper(ElementSize);
        if (Overflow)
          return ProofResult::Maybe;
      }

      Expr *AccessBase;
      llvm::APSInt AccessStartOffset;
      Expr *DummyOffset;
      // Currently, we do not try to prove whether the memory access is in range for non-constant ranges
      // TODO: generalize memory access range check to non-constants
      if (SplitIntoBaseAndOffset(PtrBase, AccessBase, AccessStartOffset, DummyOffset) != BaseRange::Kind::ConstantSized)
        return ProofResult::Maybe;

      // The access base for bounds_cast(e) should be a temporary binding of e.
      if (isa<BoundsCastExpr>(AccessBase))
        AccessBase = GetTempBinding(AccessBase);

      if (Offset) {
        llvm::APSInt IntVal;
        Optional<llvm::APSInt> OptIntVal = Offset->getIntegerConstantExpr(S.Context);
        if (!OptIntVal)
          return ProofResult::Maybe;
        IntVal = ExprUtil::ConvertToSignedPointerWidth(S.Context, *OptIntVal, Overflow);
        if (Overflow)
          return ProofResult::Maybe;
        IntVal = IntVal.smul_ov(ElementSize, Overflow);
        if (Overflow)
          return ProofResult::Maybe;
        AccessStartOffset = AccessStartOffset.sadd_ov(IntVal, Overflow);
        if (Overflow)
          return ProofResult::Maybe;
      }
      BaseRange MemoryAccessRange(S, AccessBase, AccessStartOffset,
                                           AccessStartOffset);
      Overflow = MemoryAccessRange.AddToUpper(ElementSize);
      if (Overflow)
        return ProofResult::Maybe;
#ifdef TRACE_RANGE
      llvm::outs() << "Memory access range:\n";
      MemoryAccessRange.Dump(llvm::outs());
      llvm::outs() << "Valid range:\n";
      ValidRange.Dump(llvm::outs());
#endif
      if (MemoryAccessRange.IsEmpty()) {
        Cause = CombineFailures(Cause, ProofFailure::DstEmpty);
        return ProofResult::False;
      }
      else if (MemoryAccessRange.IsInvalid()) {
        Cause = CombineFailures(Cause, ProofFailure::DstInvalid);
        return ProofResult::False;
      }
      std::pair<ComparisonSet, ComparisonSet> EmptyFacts;
      ProofResult R = ValidRange.InRange(MemoryAccessRange, Cause, EquivExprs, EmptyFacts);
      if (R == ProofResult::True)
        return R;
      if (R == ProofResult::False || R == ProofResult::Maybe) {
        if (R == ProofResult::False &&
            ValidRange.PartialOverlap(MemoryAccessRange) == ProofResult::True)
          Cause = CombineFailures(Cause, ProofFailure::PartialOverlap);
        if (ValidRange.IsEmpty()) {
          Cause = CombineFailures(Cause, ProofFailure::SrcEmpty);
          R = ProofResult::False;
        }
        if (ValidRange.IsInvalid()) {
          Cause = CombineFailures(Cause, ProofFailure::SrcInvalid);
          R = ProofResult::False;
        }
        if (MemoryAccessRange.GetWidth() > ValidRange.GetWidth()) {
          Cause = CombineFailures(Cause, ProofFailure::Width);
          R = ProofResult::False;
        }
      }
      return R;
    }

    // Convert ProofFailure codes into diagnostic notes explaining why the
    // statement involving bounds is false.
    void ExplainProofFailure(SourceLocation Loc, ProofFailure Cause,
                             ProofStmtKind Kind) {
      // Prefer diagnosis of empty bounds over bounds being too narrow.
      if (TestFailure(Cause, ProofFailure::SrcEmpty))
        S.Diag(Loc, diag::note_source_bounds_empty);
      else if (TestFailure(Cause, ProofFailure::DstEmpty))
        S.Diag(Loc, diag::note_destination_bounds_empty);
      else if (TestFailure(Cause, ProofFailure::SrcInvalid))
        S.Diag(Loc, diag::note_source_bounds_invalid);
      else if (TestFailure(Cause, ProofFailure::DstInvalid))
        S.Diag(Loc, diag::note_destination_bounds_invalid);
      else if (Kind != ProofStmtKind::StaticBoundsCast &&
               TestFailure(Cause, ProofFailure::Width))
        S.Diag(Loc, diag::note_bounds_too_narrow) << (unsigned)Kind;

      // Memory access/struct base error message.
      if (Kind == ProofStmtKind::MemoryAccess || Kind == ProofStmtKind::MemberArrowBase) {
        if (TestFailure(Cause, ProofFailure::PartialOverlap)) {
          S.Diag(Loc, diag::note_bounds_partially_overlap);
        }
      }

      if (TestFailure(Cause, ProofFailure::LowerBound))
        S.Diag(Loc, diag::note_lower_out_of_bounds) << (unsigned) Kind;
      if (TestFailure(Cause, ProofFailure::UpperBound))
        S.Diag(Loc, diag::note_upper_out_of_bounds) << (unsigned) Kind;
    }

    // Prints a note for each free variable in FreeVars at Loc.
    void DiagnoseFreeVariables(unsigned DiagId,
                               SourceLocation Loc,
                               FreeVariableListTy &FreeVars) {
      for (const auto &Pair : FreeVars) {
        unsigned DeclOrInferred =
            TestFreeVariablePosition(Pair.second, FreeVariablePosition::Declared)
                ? (unsigned)DiagnosticBoundsName::Declared
                : (unsigned)DiagnosticBoundsName::Inferred;

        FreeVariablePosition BasePos = CombineFreeVariablePosition(
            FreeVariablePosition::Lower, FreeVariablePosition::Upper);

        unsigned LowerOrUpper =
            TestFreeVariablePosition(Pair.second, BasePos)
                ? (unsigned)DiagnosticBoundsComponent::Base
                : (TestFreeVariablePosition(Pair.second, FreeVariablePosition::Lower)
                       ? (unsigned)DiagnosticBoundsComponent::Lower
                       : (unsigned)DiagnosticBoundsComponent::Upper);
        S.Diag(Loc, DiagId) << DeclOrInferred << LowerOrUpper << Pair.first;
      }
    }

    CHKCBindTemporaryExpr *GetTempBinding(Expr *E) {
      // Bounds casts should always have a temporary binding.
      if (BoundsCastExpr *BCE = dyn_cast<BoundsCastExpr>(E)) {
        CHKCBindTemporaryExpr *Result = dyn_cast<CHKCBindTemporaryExpr>(BCE->getSubExpr());
        return Result;
      }

      CHKCBindTemporaryExpr *Result =
        dyn_cast<CHKCBindTemporaryExpr>(E->IgnoreParenNoopCasts(S.getASTContext()));
      return Result;
    }

    // Given an assignment target = e, where target has declared bounds
    // DeclaredBounds and and e has inferred bounds SrcBounds, make sure
    // that SrcBounds implies that DeclaredBounds are provably true.
    //
    // CheckBoundsDeclAtAssignment is currently used only to check the bounds
    // for assignments to non-variable target expressions.  Variable bounds
    // are checked after each top-level statement in ValidateBoundsContext.
    void CheckBoundsDeclAtAssignment(SourceLocation ExprLoc, Expr *Target,
                                     BoundsExpr *DeclaredBounds, Expr *Src,
                                     BoundsExpr *SrcBounds,
                                     EquivExprSets EquivExprs,
                                     CheckedScopeSpecifier CSS) {
      // Record expression equality implied by assignment.
      // EquivExprs may not already contain equality implied by assignment.
      // For example, EquivExprs will not contain equality implied by
      // assignments to non-variables, e.g. *p = e, a[i] = e, s.f = e.
      // EquivExprs also will not contain equality implied by assignment
      // for certain kinds of source expressions.  For example, EquivExprs
      // will not contain equality implied by the assignments v = *e, v = a[i],
      // or v = s->f, since the sets of expressions that produce the same value
      // as *e, a[i], and s->f are empty.
      if (S.CheckIsNonModifying(Target, Sema::NonModifyingContext::NMC_Unknown,
                                Sema::NonModifyingMessage::NMM_None)) {
         CHKCBindTemporaryExpr *Temp = GetTempBinding(Src);
         // TODO: make sure assignment to lvalue doesn't modify value used in Src.
         bool SrcIsNonModifying =
           S.CheckIsNonModifying(Src, Sema::NonModifyingContext::NMC_Unknown,
                                 Sema::NonModifyingMessage::NMM_None);
         if (Temp || SrcIsNonModifying) {
           SmallVector<Expr *, 4> EqualExpr;
           Expr *TargetExpr = ExprCreatorUtil::CreateImplicitCast(S, Target,
                                                 CK_LValueToRValue,
                                                 Target->getType());
           EqualExpr.push_back(TargetExpr);
           if (Temp)
             EqualExpr.push_back(CreateTemporaryUse(Temp));
           else
             EqualExpr.push_back(Src);
           EquivExprs.push_back(EqualExpr);
         }
      }

      ProofFailure Cause;
      FreeVariableListTy FreeVars;
      ProofResult Result = ProveBoundsDeclValidity(DeclaredBounds, SrcBounds, Cause, &EquivExprs, FreeVars);
      if (Result != ProofResult::True) {
        // Which diagnostic message to print?
        unsigned DiagId =
            (Result == ProofResult::False)
                ? (TestFailure(Cause, ProofFailure::HasFreeVariables)
                       ? diag::error_bounds_declaration_unprovable
                       : diag::error_bounds_declaration_invalid)
                : (CSS != CheckedScopeSpecifier::CSS_Unchecked
                       ? diag::warn_checked_scope_bounds_declaration_invalid
                       : diag::warn_bounds_declaration_invalid);

        S.Diag(ExprLoc, DiagId)
          << Sema::BoundsDeclarationCheck::BDC_Assignment << Target
          << Target->getSourceRange() << Src->getSourceRange();
        if (Result == ProofResult::False)
          ExplainProofFailure(ExprLoc, Cause, ProofStmtKind::BoundsDeclaration);

        if (TestFailure(Cause, ProofFailure::HasFreeVariables))
          DiagnoseFreeVariables(diag::note_free_variable_decl_or_inferred,
                                ExprLoc, FreeVars);

        S.Diag(Target->getExprLoc(), diag::note_declared_bounds)
          << DeclaredBounds << DeclaredBounds->getSourceRange();
        S.Diag(Src->getExprLoc(), diag::note_expanded_inferred_bounds)
          << SrcBounds << Src->getSourceRange();
      }
    }

    // Given an increment/decrement operator ++e, e++, --e, or e--, where
    // e has declared bounds DeclaredBounds and e +/- 1 has inferred bounds
    // SrcBounds, make sure that SrcBounds implies that DeclaredBounds are
    // provably true.
    void CheckBoundsDeclAtIncrementDecrement(UnaryOperator *E,
                                             BoundsExpr *DeclaredBounds,
                                             BoundsExpr *SrcBounds,
                                             EquivExprSets EquivExprs,
                                             CheckedScopeSpecifier CSS) {
      if (!UnaryOperator::isIncrementDecrementOp(E->getOpcode()))
        return;

      ProofFailure Cause;
      FreeVariableListTy FreeVars;
      ProofResult Result = ProveBoundsDeclValidity(DeclaredBounds, SrcBounds, Cause, &EquivExprs, FreeVars);

      if (Result != ProofResult::True) {
        Expr *Target = E->getSubExpr();
        Expr *Src = E;
        // Which diagnostic message to print?
        unsigned DiagId =
            (Result == ProofResult::False)
                ? (TestFailure(Cause, ProofFailure::HasFreeVariables)
                       ? diag::error_bounds_declaration_unprovable
                       : diag::error_bounds_declaration_invalid)
                : (CSS != CheckedScopeSpecifier::CSS_Unchecked
                       ? diag::warn_checked_scope_bounds_declaration_invalid
                       : diag::warn_bounds_declaration_invalid);

        S.Diag(E->getExprLoc(), DiagId)
          << Sema::BoundsDeclarationCheck::BDC_Assignment << Target
          << Target->getSourceRange() << Src->getSourceRange();
        if (Result == ProofResult::False)
          ExplainProofFailure(E->getExprLoc(), Cause, ProofStmtKind::BoundsDeclaration);

        if (TestFailure(Cause, ProofFailure::HasFreeVariables))
          DiagnoseFreeVariables(diag::note_free_variable_decl_or_inferred,
                                E->getExprLoc(), FreeVars);

        S.Diag(Target->getExprLoc(), diag::note_declared_bounds)
          << DeclaredBounds << DeclaredBounds->getSourceRange();
        S.Diag(Src->getExprLoc(), diag::note_expanded_inferred_bounds)
          << SrcBounds << Src->getSourceRange();
      }
    }

    // Check that the bounds for an argument imply the expected
    // bounds for the argument.   The expected bounds are computed
    // by substituting the arguments into the bounds expression for
    // the corresponding parameter.
    void CheckBoundsDeclAtCallArg(unsigned ParamNum,
                                  BoundsExpr *ExpectedArgBounds, Expr *Arg,
                                  BoundsExpr *ArgBounds,
                                  CheckedScopeSpecifier CSS,
                                  EquivExprSets EquivExprs) {
      SourceLocation ArgLoc = Arg->getBeginLoc();
      ProofFailure Cause;
      FreeVariableListTy FreeVars;
      ProofResult Result = ProveBoundsDeclValidity(ExpectedArgBounds, ArgBounds, Cause, &EquivExprs, FreeVars);
      if (Result != ProofResult::True) {
        // Which diagnostic message to print?
        unsigned DiagId =
            (Result == ProofResult::False)
                ? (TestFailure(Cause, ProofFailure::HasFreeVariables)
                       ? diag::error_argument_bounds_unprovable
                       : diag::error_argument_bounds_invalid)
                : (CSS != CheckedScopeSpecifier::CSS_Unchecked
                       ? diag::warn_checked_scope_argument_bounds_invalid
                       : diag::warn_argument_bounds_invalid);

        S.Diag(ArgLoc, DiagId) << (ParamNum + 1) << Arg->getSourceRange();
        if (Result == ProofResult::False)
          ExplainProofFailure(ArgLoc, Cause, ProofStmtKind::BoundsDeclaration);

        if (TestFailure(Cause, ProofFailure::HasFreeVariables))
          DiagnoseFreeVariables(diag::note_free_variable_in_expected_args,
                                ArgLoc, FreeVars);

        S.Diag(ArgLoc, diag::note_expected_argument_bounds) << ExpectedArgBounds;
        S.Diag(Arg->getExprLoc(), diag::note_expanded_inferred_bounds)
          << ArgBounds << Arg->getSourceRange();
      }
    }

    // Given an initializer v = e, where v is a variable that has declared
    // bounds DeclaredBounds and and e has inferred bounds SrcBounds, make sure
    // that SrcBounds implies that DeclaredBounds are provably true.
    void CheckBoundsDeclAtInitializer(SourceLocation ExprLoc, VarDecl *D,
                                      BoundsExpr *DeclaredBounds, Expr *Src,
                                      BoundsExpr *SrcBounds,
                                      EquivExprSets EquivExprs,
                                      CheckedScopeSpecifier CSS) {
      // Record expression equality implied by initialization (see
      // CheckBoundsDeclAtAssignment).
      
      // Record equivalence between expressions implied by initializion.
      // If D declares a variable V, and
      // 1. Src binds a temporary variable T, record equivalence
      //    beteween V and T.
      // 2. Otherwise, if Src is a non-modifying expression, record
      //    equivalence between V and Src.
      CHKCBindTemporaryExpr *Temp = GetTempBinding(Src);
      // TODO: make sure variable being initialized isn't read by Src.
      DeclRefExpr *TargetDeclRef = ExprCreatorUtil::CreateVarUse(S, D);
      if (Temp ||  S.CheckIsNonModifying(Src, Sema::NonModifyingContext::NMC_Unknown,
                                         Sema::NonModifyingMessage::NMM_None)) {
        CastKind Kind;
        QualType TargetTy;
        if (D->getType()->isArrayType()) {
          Kind = CK_ArrayToPointerDecay;
          TargetTy = S.getASTContext().getArrayDecayedType(D->getType());
        } else {
          Kind = CK_LValueToRValue;
          TargetTy = D->getType();
        }
        SmallVector<Expr *, 4> EqualExpr;
        Expr *TargetExpr =
          ExprCreatorUtil::CreateImplicitCast(S, TargetDeclRef, Kind, TargetTy);
        EqualExpr.push_back(TargetExpr);
        if (Temp)
          EqualExpr.push_back(CreateTemporaryUse(Temp));
        else
          EqualExpr.push_back(Src);
        EquivExprs.push_back(EqualExpr);
        /*
        llvm::outs() << "Dumping target/src equality relation\n";
        for (Expr *E : EqualExpr)
          E->dump(llvm::outs());
        */
      }
      ProofFailure Cause;
      FreeVariableListTy FreeVars;
      ProofResult Result = ProveBoundsDeclValidity(
          DeclaredBounds, SrcBounds, Cause, &EquivExprs, FreeVars);
      if (Result != ProofResult::True) {
        // Which diagnostic message to print?
        unsigned DiagId =
            (Result == ProofResult::False)
                ? (TestFailure(Cause, ProofFailure::HasFreeVariables)
                       ? diag::error_bounds_declaration_unprovable
                       : diag::error_bounds_declaration_invalid)
                : (CSS != CheckedScopeSpecifier::CSS_Unchecked
                       ? diag::warn_checked_scope_bounds_declaration_invalid
                       : diag::warn_bounds_declaration_invalid);

        S.Diag(ExprLoc, DiagId)
          << Sema::BoundsDeclarationCheck::BDC_Initialization << TargetDeclRef
          << D->getLocation() << Src->getSourceRange();
        if (Result == ProofResult::False)
          ExplainProofFailure(ExprLoc, Cause, ProofStmtKind::BoundsDeclaration);

        if (TestFailure(Cause, ProofFailure::HasFreeVariables))
          DiagnoseFreeVariables(diag::note_free_variable_decl_or_inferred,
                                ExprLoc, FreeVars);

        S.Diag(D->getLocation(), diag::note_declared_bounds)
          << DeclaredBounds << D->getLocation();
        S.Diag(Src->getExprLoc(), diag::note_expanded_inferred_bounds)
          << SrcBounds << Src->getSourceRange();
      }
    }

    // Given a static cast to a Ptr type, where the Ptr type has
    // TargetBounds and the source has SrcBounds, make sure that (1) SrcBounds
    // implies Targetbounds or (2) the SrcBounds is at least as wide as
    // the TargetBounds.
    void CheckBoundsDeclAtStaticPtrCast(CastExpr *Cast,
                                        BoundsExpr *TargetBounds,
                                        Expr *Src,
                                        BoundsExpr *SrcBounds,
                                        CheckedScopeSpecifier CSS) {
      ProofFailure Cause;
      bool IsStaticPtrCast = (Src->getType()->isCheckedPointerPtrType() &&
                              Cast->getType()->isCheckedPointerPtrType());
      ProofStmtKind Kind = IsStaticPtrCast ? ProofStmtKind::StaticBoundsCast :
                             ProofStmtKind::BoundsDeclaration;
      FreeVariableListTy FreeVars;
      ProofResult Result = ProveBoundsDeclValidity(
          TargetBounds, SrcBounds, Cause, nullptr, FreeVars, Kind);
      if (Result != ProofResult::True) {
        // Which diagnostic message to print?
        unsigned DiagId =
            (Result == ProofResult::False)
                ? diag::error_static_cast_bounds_invalid
                : (CSS != CheckedScopeSpecifier::CSS_Unchecked
                       ? diag::warn_checked_scopestatic_cast_bounds_invalid
                       : diag::warn_static_cast_bounds_invalid);

        SourceLocation ExprLoc = Cast->getExprLoc();
        S.Diag(ExprLoc, DiagId) << Cast->getType() << Cast->getSourceRange();
        if (Result == ProofResult::False)
          ExplainProofFailure(ExprLoc, Cause,
                              ProofStmtKind::StaticBoundsCast);

        S.Diag(ExprLoc, diag::note_required_bounds) << TargetBounds;
        S.Diag(ExprLoc, diag::note_expanded_inferred_bounds) << SrcBounds;
      }
    }

    void CheckBoundsAtMemoryAccess(Expr *Deref, BoundsExpr *ValidRange,
                                   BoundsCheckKind CheckKind,
                                   CheckedScopeSpecifier CSS,
                                   EquivExprSets *EquivExprs) {

      // If we are running the 3C (AST only) tool, then disable
      // bounds checking.
      if (S.getLangOpts()._3C)
        return;

      ProofFailure Cause;
      ProofResult Result;
      ProofStmtKind ProofKind;
      #ifdef TRACE_RANGE
      llvm::outs() << "CheckBoundsMemAccess: Deref Expr: ";
      Deref->dumpPretty(S.Context);
      llvm::outs() << "\n";
      #endif
      if (UnaryOperator *UO = dyn_cast<UnaryOperator>(Deref)) {
        ProofKind = ProofStmtKind::MemoryAccess;
        Result = ProveMemoryAccessInRange(UO->getSubExpr(), nullptr, ValidRange,
                                          CheckKind, EquivExprs, Cause);
      } else if (ArraySubscriptExpr *AS = dyn_cast<ArraySubscriptExpr>(Deref)) {
        ProofKind = ProofStmtKind::MemoryAccess;
        Result = ProveMemoryAccessInRange(AS->getBase(), AS->getIdx(),
                                          ValidRange, CheckKind, EquivExprs, Cause);
      } else if (MemberExpr *ME = dyn_cast<MemberExpr>(Deref)) {
        assert(ME->isArrow());
        ProofKind = ProofStmtKind::MemberArrowBase;
        Result = ProveMemoryAccessInRange(ME->getBase(), nullptr, ValidRange, CheckKind, EquivExprs, Cause);
      } else {
        llvm_unreachable("unexpected expression kind");
      }

      if (Result == ProofResult::False) {
        #ifdef TRACE_RANGE
        llvm::outs() << "Memory access Failure Causes:";
        DumpFailure(llvm::outs(), Cause);
        llvm::outs() << "\n";
        #endif
        unsigned DiagId = diag::error_out_of_bounds_access;
        SourceLocation ExprLoc = Deref->getExprLoc();
        S.Diag(ExprLoc, DiagId) << (unsigned) ProofKind << Deref->getSourceRange();
        ExplainProofFailure(ExprLoc, Cause, ProofKind);
        S.Diag(ExprLoc, diag::note_expanded_inferred_bounds) << ValidRange;
      }
    }


  public:
    CheckBoundsDeclarations(Sema &SemaRef, PrepassInfo &Info, Stmt *Body, CFG *Cfg, FunctionDecl *FD, std::pair<ComparisonSet, ComparisonSet> &Facts) : S(SemaRef),
      DumpBounds(SemaRef.getLangOpts().DumpInferredBounds),
      DumpState(SemaRef.getLangOpts().DumpCheckingState),
      DumpSynthesizedMembers(SemaRef.getLangOpts().DumpSynthesizedMembers),
      PointerWidth(SemaRef.Context.getTargetInfo().getPointerWidth(0)),
      Body(Body),
      Cfg(Cfg),
      FunctionDeclaration(FD),
      ReturnVal(nullptr),
      ReturnBounds(nullptr),
      Context(SemaRef.Context),
      Facts(Facts),
      BoundsWideningAnalyzer(BoundsWideningAnalysis(SemaRef, Cfg,
                                                    Info.BoundsVarsLower,
                                                    Info.BoundsVarsUpper)),
      AbstractSetMgr(AbstractSetManager(SemaRef, Info.VarUses)),
      BoundsSiblingFields(Info.BoundsSiblingFields),
      IncludeNullTerminator(false) {
        if (FD) {
          ReturnVal =
            new (S.Context) BoundsValueExpr(SourceLocation(),
                                            FD->getReturnType(),
                                            BoundsValueExpr::Kind::Return);
          ReturnBounds =
            BoundsUtil::ExpandToRange(S, ReturnVal, FD->getBoundsExpr());
        }
      }

    CheckBoundsDeclarations(Sema &SemaRef, PrepassInfo &Info, std::pair<ComparisonSet, ComparisonSet> &Facts) : S(SemaRef),
      DumpBounds(SemaRef.getLangOpts().DumpInferredBounds),
      DumpState(SemaRef.getLangOpts().DumpCheckingState),
      DumpSynthesizedMembers(SemaRef.getLangOpts().DumpSynthesizedMembers),
      PointerWidth(SemaRef.Context.getTargetInfo().getPointerWidth(0)),
      Body(nullptr),
      Cfg(nullptr),
      FunctionDeclaration(nullptr),
      ReturnVal(nullptr),
      ReturnBounds(nullptr),
      Context(SemaRef.Context),
      Facts(Facts),
      BoundsWideningAnalyzer(BoundsWideningAnalysis(SemaRef, nullptr,
                                                    Info.BoundsVarsLower,
                                                    Info.BoundsVarsUpper)),
      AbstractSetMgr(AbstractSetManager(SemaRef, Info.VarUses)),
      BoundsSiblingFields(Info.BoundsSiblingFields),
      IncludeNullTerminator(false) {}

    void IdentifyChecked(Stmt *S, StmtSetTy &MemoryCheckedStmts, StmtSetTy &BoundsCheckedStmts, CheckedScopeSpecifier CSS) {
      if (!S)
        return;

      if (CSS == CheckedScopeSpecifier::CSS_Memory)
        if (isa<Expr>(S) || isa<DeclStmt>(S) || isa<ReturnStmt>(S))
          MemoryCheckedStmts.insert(S);

      if (CSS == CheckedScopeSpecifier::CSS_Bounds)
        if (isa<Expr>(S) || isa<DeclStmt>(S) || isa<ReturnStmt>(S))
          BoundsCheckedStmts.insert(S);

      if (const CompoundStmt *CS = dyn_cast<CompoundStmt>(S))
        CSS = CS->getCheckedSpecifier();

      auto Begin = S->child_begin(), End = S->child_end();
      for (auto I = Begin; I != End; ++I)
        IdentifyChecked(*I, MemoryCheckedStmts, BoundsCheckedStmts, CSS);
    }

    // Add any subexpressions of S that occur in TopLevelElems to NestedExprs.
    void MarkNested(const Stmt *S, StmtSetTy &NestedExprs, StmtSetTy &TopLevelElems) {
      auto Begin = S->child_begin(), End = S->child_end();
      for (auto I = Begin; I != End; ++I) {
        const Stmt *Child = *I;
        if (!Child)
          continue;
        if (TopLevelElems.find(Child) != TopLevelElems.end())
          NestedExprs.insert(Child);
        MarkNested(Child, NestedExprs, TopLevelElems);
      }
   }

  // Identify CFG elements that are statements that are substatements of other
  // CFG elements.  (CFG elements are the components of basic blocks).  When a
  // CFG is constructed, subexpressions of top-level expressions may be placed
  // in separate CFG elements.  This is done for subexpressions of expressions
  // with control-flow, for example. When checking bounds declarations, we want
  // to process a subexpression with its enclosing expression. We want to
  // ignore CFG elements that are substatements of other CFG elements.
  //
  // As an example, given a conditional expression, all subexpressions will
  // be made into separate CFG elements.  The expression
  //    x = (cond == 0) ? f1() : f2(),
  // has a CFG of the form:
  //    B1:
  //     1: cond == 0
  //     branch cond == 0 B2, B3
  //   B2:
  //     1: f1();
  //     jump B4
  //   B3:
  //     1: f2();
  //     jump B4
  //   B4:
  //     1: x = (cond == 0) ? f1 : f2();
  //
  // For now, we want to skip B1.1, B2.1, and B3.1 because they will be processed
  // as part of B4.1.
   void FindNestedElements(StmtSetTy &NestedStmts) {
      // Create the set of top-level CFG elements.
      StmtSetTy TopLevelElems;
      for (const CFGBlock *Block : *Cfg) {
        for (CFGElement Elem : *Block) {
          if (Elem.getKind() == CFGElement::Statement) {
            CFGStmt CS = Elem.castAs<CFGStmt>();
            const Stmt *S = CS.getStmt();
            TopLevelElems.insert(S);
          }
        }
      }

      // Create the set of top-level elements that are subexpressions
      // of other top-level elements.
      for (const CFGBlock *Block : *Cfg) {
        for (CFGElement Elem : *Block) {
          if (Elem.getKind() == CFGElement::Statement) {
            CFGStmt CS = Elem.castAs<CFGStmt>();
            const Stmt *S = CS.getStmt();
            MarkNested(S, NestedStmts, TopLevelElems);
          }
        }
      }
   }

   void UpdateWidenedBounds(BoundsWideningAnalysis &BA, const CFGBlock *Block,
                            Stmt *CurrStmt, CheckingState &State) {
     // Get the bounds widened before the current statement.
     BoundsMapTy WidenedBounds = BA.GetStmtIn(Block, CurrStmt);

     // BoundsWideningAnalysis currently uses VarDecls as keys in the widened
     // bounds data structure, so we get the AbstractSet for each VarDecl in
     // the widened bounds.
     // TODO: use AbstractSets as keys in BoundsWideningAnalysis
     // (checkedc-clang issue #1015).

     // Update the bounds of each variable in ObservedBounds to the bounds
     // computed by the bounds widening analysis.
     // Note: Bounds widening analysis resets killed bounds of a variable to
     // its declared bounds. So we do not need to explicitly reset killed
     // bounds here.
     for (const auto VarBoundsPair : WidenedBounds) {
       const VarDecl *V = VarBoundsPair.first;
       BoundsExpr *Bounds = VarBoundsPair.second;
       if (!Bounds)
         continue;

       const AbstractSet *A = AbstractSetMgr.GetOrCreateAbstractSet(V);

       auto I = State.ObservedBounds.find(A);
       if (I != State.ObservedBounds.end())
         I->second = Bounds;
     }
   }

   // When a variable goes out of scope:
   // 1) it has to be removed from ObservedBounds in the CheckingState
   //    if it is a checked pointer variable because we no longer want
   //    to validate its bounds, and
   // 2) the expressions in EquivExprs that use it have to be removed
   //    because the expressions are now undefined.
   void UpdateStateForVariableOutOfScope(CheckingState &State, VarDecl *V) {
     if (V->hasBoundsExpr()) {
       const AbstractSet *A = AbstractSetMgr.GetOrCreateAbstractSet(V);
       auto I = State.ObservedBounds.find(A);
       if (I != State.ObservedBounds.end())
         State.ObservedBounds.erase(A);
     }

     EquivExprSets CrntEquivExprs(State.EquivExprs);
     State.EquivExprs.clear();
     for (auto I = CrntEquivExprs.begin(), E = CrntEquivExprs.end();
                                                         I != E; ++I) {
       ExprSetTy ExprList;
       for (auto InnerList = (*I).begin(); InnerList != (*I).end();
                                                         ++InnerList) {
         Expr *E = *InnerList;
         if (!ExprUtil::VariableOccurrenceCount(S, V, E))
           ExprList.push_back(E);
       }
       if (ExprList.size() > 1)
         State.EquivExprs.push_back(ExprList);
     }
   }

   // Return true if Statement S is the first statement in a bundled block.
   // A bundled block can contain only declarations or expression statements.
   // Therefore only the classes DeclStmt and ValueStmt (which wraps an
   // expression statement) contain flags that indicate if a statement is the
   // first or the last statement of a bundled block.
   bool IsFirstStmtOfBundledBlk(Stmt *S) {
     if (auto VS = dyn_cast<ValueStmt>(S)) {
       if (VS->isFirstStmtOfBundledBlk())
         return true;
     }
     else if (auto DS = dyn_cast<DeclStmt>(S)) {
       if (DS->isFirstStmtOfBundledBlk())
         return true;
     }
     return false;
   }

   // Return true if Statement S is the last statement in a bundled block.
   // See the description of the above function for more details.
   bool IsLastStmtOfBundledBlk(Stmt *S) {
     if (auto VS = dyn_cast<ValueStmt>(S)) {
       if (VS->isLastStmtOfBundledBlk())
         return true;
     }
     else if (auto DS = dyn_cast<DeclStmt>(S)) {
       if (DS->isLastStmtOfBundledBlk())
         return true;
     }
     return false;
   }

   // Walk the CFG, traversing basic blocks in reverse post-oder.
   // For each element of a block, check bounds declarations.  Skip
   // CFG elements that are subexpressions of other CFG elements.
   void TraverseCFG(AvailableFactsAnalysis& AFA, FunctionDecl *FD) {
     assert(Cfg && "expected CFG to exist");
#if TRACE_CFG
     llvm::outs() << "Dumping AST";
     Body->dump(llvm::outs(), Context);
     llvm::outs() << "Dumping CFG:\n";
     Cfg->print(llvm::outs(), S.getLangOpts(), true);
     llvm::outs() << "Traversing CFG:\n";
#endif

     // Reset the AbstractSetMgr at the beginning of each function, since
     // the storage of AbstractSets should only persist for one function.
     AbstractSetMgr.Clear();

     // Map each function parameter to its declared bounds (if any),
     // normalized to range bounds, before checking the body of the function.
     // The context formed by the declared parameter bounds is the initial
     // observed bounds context for checking the function body.
     CheckingState ParamsState;
     for (auto I = FD->param_begin(); I != FD->param_end(); ++I) {
       ParmVarDecl *Param = *I;
       if (!Param->hasBoundsExpr())
         continue;
       if (BoundsExpr *Bounds = S.NormalizeBounds(Param)) {
         const AbstractSet *A = AbstractSetMgr.GetOrCreateAbstractSet(Param);
         ParamsState.ObservedBounds[A] = Bounds;
       }
     }

     // Store a checking state for each CFG block in order to track
     // the variables with bounds declarations that are in scope.
     llvm::DenseMap<unsigned int, CheckingState> BlockStates;
     BlockStates[Cfg->getEntry().getBlockID()] = ParamsState;

     StmtSetTy NestedElements;
     FindNestedElements(NestedElements);
     StmtSetTy MemoryCheckedStmts;
     StmtSetTy BoundsCheckedStmts;
     IdentifyChecked(Body, MemoryCheckedStmts, BoundsCheckedStmts, CheckedScopeSpecifier::CSS_Unchecked);
     BoundsContextTy InitialObservedBounds;
     bool InBundledBlock = false;

     // Run the bounds widening analysis on this function.
     BoundsWideningAnalyzer.WidenBounds(FD, NestedElements);
     if (S.getLangOpts().DumpWidenedBounds)
       BoundsWideningAnalyzer.DumpWidenedBounds(FD, 0);
     if (S.getLangOpts().DumpWidenedBoundsDataflowSets)
       BoundsWideningAnalyzer.DumpWidenedBounds(FD, 1);

     PostOrderCFGView POView = PostOrderCFGView(Cfg);
     ResetFacts();
     for (const CFGBlock *Block : POView) {
       AFA.GetFacts(Facts);
       CheckingState BlockState = GetIncomingBlockState(Block, BlockStates);

       for (CFGElement Elem : *Block) {
         if (Elem.getKind() == CFGElement::Statement) {
           CFGStmt CS = Elem.castAs<CFGStmt>();
           // We may attach a bounds expression to Stmt, so drop the const
           // modifier.
           Stmt *S = const_cast<Stmt *>(CS.getStmt());

           // Skip top-level elements that are nested in
           // another top-level element.
           if (NestedElements.find(S) != NestedElements.end())
             continue;

           // Update the observed bounds with the widened bounds computed
           // above.
           UpdateWidenedBounds(BoundsWideningAnalyzer, Block, S, BlockState);

           CheckedScopeSpecifier CSS = CheckedScopeSpecifier::CSS_Unchecked;
           const Stmt *Statement = S;
           if (DeclStmt *DS = dyn_cast<DeclStmt>(S))
             // CFG construction will synthesize decl statements so that
             // each declarator is a separate CFGElem.  To see if we are in
             // a checked scope, look at the original decl statement.
             Statement = Cfg->getSourceDeclStmt(DS);
           if (MemoryCheckedStmts.find(Statement) != MemoryCheckedStmts.end())
             CSS = CheckedScopeSpecifier::CSS_Memory;
           else if (BoundsCheckedStmts.find(Statement) != BoundsCheckedStmts.end())
             CSS = CheckedScopeSpecifier::CSS_Bounds;

#if TRACE_CFG
            llvm::outs() << "Visiting ";
            S->dump(llvm::outs(), Context);
            llvm::outs().flush();
#endif
            // Modify the ObservedBounds context to include any variables with
            // bounds that are declared in S.  Before checking S, the observed
            // bounds for each variable v that is in scope are the widened
            // bounds for v (if any), or the declared bounds for v (if any).
            GetDeclaredBounds(this->S, BlockState.ObservedBounds, S, AbstractSetMgr);

            if (!InBundledBlock) {
                InitialObservedBounds = BlockState.ObservedBounds;
                BlockState.Reset();
            }

            if (IsFirstStmtOfBundledBlk(S)) InBundledBlock = true;

            Check(S, CSS, BlockState);

            if (DumpState)
              DumpCheckingState(llvm::outs(), S, BlockState);

            if (IsLastStmtOfBundledBlk(S)) InBundledBlock = false;

            // If a bundled block is detected, then the ObservedBounds are
            // updated for each statement in the bundled block. However,
            // the ObservedBounds are checked against the declared bounds
            // only at the end of a bundled block.
            if (!InBundledBlock) {

              // For each AbstractSet A in ObservedBounds, check that the
              // observed bounds of A imply the declared bounds of A.
              ValidateBoundsContext(S, BlockState, CSS, Block);

              // The observed bounds that were updated after checking S should
              // only be used to check that the updated observed bounds imply
              // the declared variable bounds.  After checking the observed and
              // declared bounds, the observed bounds for each AbstractSet should
              // be reset to their observed bounds from before checking S.
              BlockState.ObservedBounds = InitialObservedBounds;
            }
         }
         else if (Elem.getKind() == CFGElement::LifetimeEnds) {
            // Every variable going out of scope is indicated by a LifetimeEnds
            // CFGElement. When a variable goes out of scope, ObservedBounds and
            // EquivExprs in the CheckingState have to be updated.
            CFGLifetimeEnds LE = Elem.castAs<CFGLifetimeEnds>();
            VarDecl *V = const_cast<VarDecl *>(LE.getVarDecl());
            if (V)
              UpdateStateForVariableOutOfScope(BlockState, V);
         }
       }
       if (Block->getBlockID() != Cfg->getEntry().getBlockID())
         BlockStates[Block->getBlockID()] = BlockState;
       AFA.Next();
     }
    }

  // Methods for inferring bounds expressions for C expressions.

  // C has an interesting semantics for expressions that differentiates between
  // lvalue and value expressions and inserts implicit conversions from lvalues
  // to values.  Value expressions are usually called rvalue expressions.  This
  // semantics is represented directly in the clang IR by having some
  // expressions evaluate to lvalues and having implicit conversions that convert
  // those lvalues to rvalues.
  //
  // Using ths representation directly would make it clumsy to compute bounds
  // expressions.  For an expression that evaluates to an lvalue, we would have
  // to compute and carry along two bounds expressions: the bounds expression
  // for the lvalue and the bounds expression for the value at which the lvalue
  // points.
  //
  // We address this by having two methods for computing bounds.  One method
  // (Check) computes the bounds for an rvalue expression. For lvalue
  // expressions, we have one method that compute two kinds of bounds.
  // CheckLValue computes the bounds for the lvalue produced by an expression
  // and the bounds for the target of the lvalue produced by the expression.
  //
  // There are only a few contexts where an lvalue expression can occur, so it
  // is straightforward to determine which method to use. Also, the clang IR
  // makes it explicit when an lvalue is converted to an rvalue by an lvalue
  // cast operation.
  //
  // An expression denotes an lvalue if it occurs in the following contexts:
  // 1. As the left-hand side of an assignment operator.
  // 2. As the operand to a postfix or prefix incrementation operators (which
  //    implicitly do assignment).
  // 3. As the operand of the address-of (&) operator.
  // 4. If a member access operation e1.f denotes an lvalue, e1 denotes an
  //    lvalue.
  // 5. In clang IR, as an operand to an LValueToRValue cast operation.
  // Otherwise an expression denotes an rvalue.

  public:
    BoundsExpr *Check(Stmt *S, CheckedScopeSpecifier CSS) {
      CheckingState State;
      BoundsExpr *Bounds = Check(S, CSS, State);
      if (DumpState)
        DumpCheckingState(llvm::outs(), S, State);
      return Bounds;
    }

    // If e is an rvalue, Check checks e and its children, performing any
    // necessary side effects, and returns the bounds for the value
    // produced by e.
    // If e is an lvalue, Check checks e and its children, performing any
    // necessary side effects, and returns unknown bounds.
    //
    // The returned bounds expression may contain a modifying expression within
    // it. It is the caller's responsibility to validate that the bounds
    // expression is non-modifying.
    //
    // Check recursively checks the children of e and performs any
    // necessary side effects on e.  Check and CheckLValue work together
    // to traverse each expression in a CFG exactly once.
    //
    // State is an out parameter that holds the result of Check.
    BoundsExpr *Check(Stmt *S, CheckedScopeSpecifier CSS,
                      CheckingState &State) {
      if (!S)
        return CreateBoundsEmpty();

      if (Expr *E = dyn_cast<Expr>(S)) {
        if (E->containsErrors())
          return CreateBoundsEmpty();
        E = E->IgnoreParens();
        S = E;
        if (E->isLValue()) {
          CheckLValue(E, CSS, State);
          return BoundsUtil::CreateBoundsAlwaysUnknown(this->S);
        }
      }

      BoundsExpr *ResultBounds =
        BoundsUtil::CreateBoundsAlwaysUnknown(this->S);

      switch (S->getStmtClass()) {
        case Expr::UnaryOperatorClass:
          ResultBounds = CheckUnaryOperator(cast<UnaryOperator>(S),
                                            CSS, State);
          break;
        case Expr::CallExprClass:
          ResultBounds = CheckCallExpr(cast<CallExpr>(S), CSS, State);
          break;
        case Expr::ImplicitCastExprClass:
        case Expr::CStyleCastExprClass:
        case Expr::BoundsCastExprClass:
          ResultBounds = CheckCastExpr(cast<CastExpr>(S), CSS, State);
          break;
        case Expr::BinaryOperatorClass:
        case Expr::CompoundAssignOperatorClass:
          ResultBounds = CheckBinaryOperator(cast<BinaryOperator>(S),
                                             CSS, State);
          break;
        case Stmt::CompoundStmtClass: {
          CompoundStmt *CS = cast<CompoundStmt>(S);
          CSS = CS->getCheckedSpecifier();
          // Check may be called on a CompoundStmt if a CFG could not be
          // constructed, so check the children of a CompoundStmt.
          CheckChildren(CS, CSS, State);
          break;
        }
        case Stmt::DeclStmtClass: {
          DeclStmt *DS = cast<DeclStmt>(S);
          auto BeginDecls = DS->decl_begin(), EndDecls = DS->decl_end();
          for (auto I = BeginDecls; I != EndDecls; ++I) {
            Decl *D = *I;
            // If an initializer expression is present, it is visited
            // during the traversal of the variable declaration.
            if (VarDecl *VD = dyn_cast<VarDecl>(D))
              ResultBounds = CheckVarDecl(VD, CSS, State);
          }
          break;
        }
        case Stmt::ReturnStmtClass:
          ResultBounds = CheckReturnStmt(cast<ReturnStmt>(S), CSS, State);
          break;
        case Stmt::CHKCBindTemporaryExprClass: {
          CHKCBindTemporaryExpr *Binding = cast<CHKCBindTemporaryExpr>(S);
          ResultBounds = CheckTemporaryBinding(Binding, CSS, State);
          break;
        }
        case Expr::ConditionalOperatorClass:
        case Expr::BinaryConditionalOperatorClass: {
          AbstractConditionalOperator *ACO = cast<AbstractConditionalOperator>(S);
          ResultBounds = CheckConditionalOperator(ACO, CSS, State);
          break;
        }
        case Expr::BoundsValueExprClass:
          ResultBounds = CheckBoundsValueExpr(cast<BoundsValueExpr>(S),
                                              CSS, State);
          break;
        default:
          CheckChildren(S, CSS, State);
          break;
      }

      if (Expr *E = dyn_cast<Expr>(S)) {
        if (E->isValueDependent())
          return ResultBounds;

        // Null ptrs always have bounds(any).
        // This is the correct way to detect all the different ways that
        // C can make a null ptr.
        if (E->isNullPointerConstant(Context, Expr::NPC_NeverValueDependent))
          return CreateBoundsAny();
      }

      return ResultBounds;
    }

    // Infer the bounds for an lvalue.
    //
    // The lvalue bounds determine whether it is valid to access memory
    // using the lvalue.  The bounds should be the range of an object in
    // memory or a subrange of an object.
    //
    // The returned bounds expressions may contain a modifying expression within
    // them. It is the caller's responsibility to validate that the bounds
    // expressions are non-modifying.
    //
    // CheckLValue recursively checks the children of e and performs any
    // necessary side effects on e.  Check and CheckLValue work together
    // to traverse each expression in a CFG exactly once.
    //
    // State is an out parameter that holds the result of Check.
    BoundsExpr *CheckLValue(Expr *E, CheckedScopeSpecifier CSS,
                            CheckingState &State) {
      if (!E->isLValue())
        return BoundsUtil::CreateBoundsInferenceError(S);

      if (E->containsErrors())
        return BoundsUtil::CreateBoundsInferenceError(S);

      E = E->IgnoreParens();

      switch (E->getStmtClass()) {
        case Expr::DeclRefExprClass:
          return CheckDeclRefExpr(cast<DeclRefExpr>(E), CSS, State);
        case Expr::UnaryOperatorClass:
          return CheckUnaryLValue(cast<UnaryOperator>(E), CSS, State);
        case Expr::ArraySubscriptExprClass:
          return CheckArraySubscriptExpr(cast<ArraySubscriptExpr>(E),
                                         CSS, State);
        case Expr::MemberExprClass:
          return CheckMemberExpr(cast<MemberExpr>(E), CSS, State);
        case Expr::ImplicitCastExprClass:
          return CheckCastLValue(cast<CastExpr>(E), CSS, State);
        case Expr::CHKCBindTemporaryExprClass:
          return CheckTempBindingLValue(cast<CHKCBindTemporaryExpr>(E),
                                        CSS, State);
        default: {
          CheckChildren(E, CSS, State);
          return BoundsUtil::CreateBoundsAlwaysUnknown(S);
        }
      }
    }

    // Infer bounds for the target of an lvalue expression.
    // Values assigned through the lvalue must satisfy the target bounds.
    // Values read through the lvalue will meet the target bounds.
    BoundsExpr *GetLValueTargetBounds(Expr *E, CheckedScopeSpecifier CSS) {
      if (!E->isLValue())
        return BoundsUtil::CreateBoundsInferenceError(S);

      // The type for inferring the target bounds cannot ever be an array
      // type, as these are dealt with by an array conversion, not an lvalue
      // conversion. The bounds for an array conversion are the same as the
      // lvalue bounds of the array-typed expression.
      if (E->getType()->isArrayType())
        return BoundsUtil::CreateBoundsInferenceError(S);

      E = E->IgnoreParens();

      switch (E->getStmtClass()) {
        case Expr::DeclRefExprClass:
          return DeclRefExprTargetBounds(cast<DeclRefExpr>(E), CSS);
        case Expr::UnaryOperatorClass:
          return UnaryOperatorTargetBounds(cast<UnaryOperator>(E), CSS);
        case Expr::ArraySubscriptExprClass:
          return ArraySubscriptExprTargetBounds(cast<ArraySubscriptExpr>(E),
                                                CSS);
        case Expr::MemberExprClass:
          return MemberExprTargetBounds(cast<MemberExpr>(E), CSS);
        case Expr::ImplicitCastExprClass:
          return LValueCastTargetBounds(cast<ImplicitCastExpr>(E), CSS);
        case Expr::CHKCBindTemporaryExprClass:
          return LValueTempBindingTargetBounds(cast<CHKCBindTemporaryExpr>(E),
                                               CSS);
        default:
          return BoundsUtil::CreateBoundsInferenceError(S);
      }
    }

    // CheckChildren recursively checks and performs any side effects on the
    // children of a statement or expression, throwing away the resulting
    // bounds.
    void CheckChildren(Stmt *S, CheckedScopeSpecifier CSS,
                       CheckingState &State) {
      ExprEqualMapTy SubExprSameValueSets;
      auto Begin = S->child_begin(), End = S->child_end();

      for (auto I = Begin; I != End; ++I) {
        Stmt *Child = *I;
        if (!Child) continue;
        // Accumulate the EquivExprs from checking each child into the
        // EquivExprs for S.
        Check(Child, CSS, State);

        // Store the set SameValue_i for each subexpression S_i.
        if (Expr *SubExpr = dyn_cast<Expr>(Child))
          SubExprSameValueSets[SubExpr] = State.SameValue;
      }

      // Use the stored sets SameValue_i for each subexpression S_i
      // to update the set SameValue for the expression S.
      if (Expr *E = dyn_cast<Expr>(S))
        UpdateSameValue(E, SubExprSameValueSets, State.SameValue);
    }

    // Traverse a top-level variable declaration.  If there is an
    // initializer, it will be traversed in CheckVarDecl.
    void TraverseTopLevelVarDecl(VarDecl *VD, CheckedScopeSpecifier CSS) {
      ResetFacts();
      CheckingState State;
      CheckVarDecl(VD, CSS, State, /*CheckBounds=*/true);
    }

    void ResetFacts() {
      std::pair<ComparisonSet, ComparisonSet> EmptyFacts;
      Facts = EmptyFacts;
    }

    bool IsBoundsSafeInterfaceAssignment(QualType DestTy, Expr *E) {
      if (DestTy->isUncheckedPointerType()) {
        ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(E);
        if (ICE)
          return ICE && ICE->getCastKind() == CK_BitCast &&
                 ICE->getSubExpr()->getType()->isCheckedPointerType();
      }
      return false;
    }

  // Methods to infer bounds for an expression that produces an rvalue.

  private:
    // CheckBinaryOperator returns the bounds for the value produced by e.
    // e is an rvalue.
    BoundsExpr *CheckBinaryOperator(BinaryOperator *E,
                                    CheckedScopeSpecifier CSS,
                                    CheckingState &State) {
      Expr *LHS = E->getLHS();
      Expr *RHS = E->getRHS();
      ExprEqualMapTy SubExprSameValueSets;

      // Infer the bounds for the target of the LHS.
      BoundsExpr *LHSTargetBounds = GetLValueTargetBounds(LHS, CSS);

      // Infer the lvalue or rvalue bounds of the LHS, saving the set
      // SameValue of expressions that produce the same value as the LHS.
      BoundsExpr *LHSLValueBounds, *LHSBounds;
      InferBounds(LHS, CSS, LHSLValueBounds, LHSBounds, State);
      SubExprSameValueSets[LHS] = State.SameValue;

      // Infer the rvalue bounds of the RHS, saving the set SameValue
      // of expressions that produce the same value as the RHS.
      BoundsExpr *RHSBounds = Check(RHS, CSS, State);
      SubExprSameValueSets[RHS] = State.SameValue;

      BinaryOperatorKind Op = E->getOpcode();

      // Bounds of the binary operator.
      BoundsExpr *ResultBounds = CreateBoundsEmpty();

      // Floating point expressions have empty bounds.
      if (E->getType()->isFloatingType())
        ResultBounds = CreateBoundsEmpty();

      // `e1 = e2` has the bounds of `e2`. `e2` is an RValue.
      else if (Op == BinaryOperatorKind::BO_Assign)
        ResultBounds = RHSBounds;

      // `e1, e2` has the bounds of `e2`. Both `e1` and `e2`
      // are RValues.
      else if (Op == BinaryOperatorKind::BO_Comma)
        ResultBounds = RHSBounds;
      
      else {
        // Compound Assignments function like assignments mostly,
        // except the LHS is an L-Value, so we'll use the bounds for the
        // value produced by the LHS. These are either:
        // 1. The observed bounds as recorded in State.ObservedBounds, or:
        // 2. The target bounds for the LHS.
        BoundsExpr *LHSRValueBounds = LHSTargetBounds;
        bool IsCompoundAssignment = false;
        if (BinaryOperator::isCompoundAssignmentOp(Op)) {
          Op = BinaryOperator::getOpForCompoundAssignment(Op);
          IsCompoundAssignment = true;
          if (BoundsExpr *ObservedBounds = GetLValueObservedBounds(LHS, State))
            LHSRValueBounds = ObservedBounds;
        }

        // Pointer arithmetic.
        //
        // `p + i` has the bounds of `p`. `p` is an RValue.
        // `p += i` has the lvalue target bounds of `p`. `p` is an LValue. `p += i` is an RValue
        // same applies for `-` and `-=` respectively
        if (LHS->getType()->isPointerType() &&
            RHS->getType()->isIntegerType() &&
            BinaryOperator::isAdditiveOp(Op)) {
          ResultBounds = IsCompoundAssignment ?
            LHSRValueBounds : LHSBounds;
        }
        // `i + p` has the bounds of `p`. `p` is an RValue.
        // `i += p` has the bounds of `p`. `p` is an RValue.
        else if (LHS->getType()->isIntegerType() &&
            RHS->getType()->isPointerType() &&
            Op == BinaryOperatorKind::BO_Add) {
          ResultBounds = RHSBounds;
        }
        // `e - p` has empty bounds, regardless of the bounds of p.
        // `e -= p` has empty bounds, regardless of the bounds of p.
        else if (RHS->getType()->isPointerType() &&
            Op == BinaryOperatorKind::BO_Sub) {
          ResultBounds = CreateBoundsEmpty();
        }

        // Arithmetic on integers with bounds.
        //
        // `e1 @ e2` has the bounds of whichever of `e1` or `e2` has bounds.
        // if both `e1` and `e2` have bounds, then they must be equal.
        // Both `e1` and `e2` are RValues
        //
        // `e1 @= e2` has the bounds of whichever of `e1` or `e2` has bounds.
        // if both `e1` and `e2` have bounds, then they must be equal.
        // `e1` is an LValue, its bounds are the lvalue target bounds.
        // `e2` is an RValue
        //
        // @ can stand for: +, -, *, /, %, &, |, ^, >>, <<
        else if (LHS->getType()->isIntegerType() &&
            RHS->getType()->isIntegerType() &&
            (BinaryOperator::isAdditiveOp(Op) ||
              BinaryOperator::isMultiplicativeOp(Op) ||
              BinaryOperator::isBitwiseOp(Op) ||
              BinaryOperator::isShiftOp(Op))) {
          BoundsExpr *LeftBounds = IsCompoundAssignment ?
            LHSRValueBounds : LHSBounds;
          if (LeftBounds->isUnknown() && !RHSBounds->isUnknown())
            ResultBounds = RHSBounds;
          else if (!LeftBounds->isUnknown() && RHSBounds->isUnknown())
            ResultBounds = LeftBounds;
          else if (!LeftBounds->isUnknown() && !RHSBounds->isUnknown()) {
            // TODO: Check if LeftBounds and RHSBounds are equal.
            // if so, return one of them. If not, return bounds(unknown)
            ResultBounds = BoundsUtil::CreateBoundsAlwaysUnknown(S);
          }
          else if (LeftBounds->isUnknown() && RHSBounds->isUnknown())
            ResultBounds = CreateBoundsEmpty();
        }
      }

      // Determine whether the checking state is updated for an assignment.
      bool StateUpdated = false;

      // Update the checking state.  The result bounds may also be updated
      // for assignments to a variable, member expression, pointer dereference,
      // or array subscript.
      if (E->isAssignmentOp()) {
        Expr *Target =
          ExprCreatorUtil::CreateImplicitCast(S, LHS, CK_LValueToRValue,
                                              LHS->getType());
        Expr *Src = RHS;

        // A compound assignment `e1 @= e2` implies an assignment `e1 = e1 @ e2`.
        if (E->isCompoundAssignmentOp()) {
          // Create the RHS of the implied assignment `e1 = e1 @ e2`.
          Src = ExprCreatorUtil::CreateBinaryOperator(S, Target, RHS, Op);

          // Update State.SameValue to be the set of expressions that produce
          // the same value as the source `e1 @ e2` of the assignment
          // `e1 = e1 @ e2`.
          UpdateSameValue(Src, SubExprSameValueSets, State.SameValue);
        }

        // Update the checking state and result bounds to reflect the
        // assignment to `e1`.
        ResultBounds = UpdateAfterAssignment(LHS, LHSTargetBounds, E, Target,
                                             Src, ResultBounds, CSS, State,
                                             StateUpdated);

        // SameValue is empty for assignments to a non-variable. This
        // conservative approach avoids recording false equality facts for
        // assignments where the LHS appears on the RHS, e.g. *p = *p + 1.
        if (!VariableUtil::GetLValueVariable(S, LHS))
          State.SameValue.clear();
      } else if (BinaryOperator::isLogicalOp(Op)) {
        // TODO: update State for logical operators `e1 && e2` and `e1 || e2`.
      } else if (Op == BinaryOperatorKind::BO_Comma) {
        // Do nothing for comma operators `e1, e2`. State already contains the
        // the correct EquivExprs and SameValue sets as a result of checking
        // `e1` and `e2`.
      } else
        // For all other binary operators `e1 @ e2`, use the SameValue sets for
        // `e1` and `e2` stored in SubExprSameValueSets to update
        // State.SameValue for `e1 @ e2`.
        UpdateSameValue(E, SubExprSameValueSets, State.SameValue);

      // TODO: checkedc-clang issue #873: combine this E->isAssignmentOp()
      // block with the earlier E->isAssignmentOp() block for updating the
      // checking state.
      if (E->isAssignmentOp()) {
        QualType LHSType = LHS->getType();
        // Bounds of the right-hand side of the assignment
        BoundsExpr *RightBounds = nullptr;

        if (!E->isCompoundAssignmentOp() &&
            LHSType->isCheckedPointerPtrType() &&
            RHS->getType()->isCheckedPointerPtrType()) {
          // ptr<T> to ptr<T> assignment, no obligation to check assignment bounds
        }
        else if (LHSType->isCheckedPointerType() ||
                  LHSType->isIntegerType() ||
                  IsBoundsSafeInterfaceAssignment(LHSType, RHS)) {
          // Check that the value being assigned has bounds if the
          // target of the LHS lvalue has bounds.
          LHSTargetBounds = S.CheckNonModifyingBounds(LHSTargetBounds, LHS);
          if (!LHSTargetBounds->isUnknown()) {
            if (E->isCompoundAssignmentOp())
              RightBounds = S.CheckNonModifyingBounds(ResultBounds, E);
            else
              RightBounds = S.CheckNonModifyingBounds(ResultBounds, RHS);

            // If RightBounds are invalid bounds, it is because the bounds for
            // the RHS contained a modifying expression. Update the observed
            // bounds of the LHS to be InvalidBounds to avoid extraneous errors
            // during bounds declaration validation.
            if (StateUpdated && RightBounds->isInvalid()) {
              const AbstractSet *A = AbstractSetMgr.GetOrCreateAbstractSet(LHS);
              State.ObservedBounds[A] = RightBounds;
            }

            // Check bounds declarations for assignments where the state was
            // not updated in UpdateAfterAssignment.
            // If the state was updated in UpdateAfterAssignment, the bounds
            // will be checked after checking the current top-level statement.
            if (!StateUpdated) {
              if (RightBounds->isUnknown()) {
                S.Diag(RHS->getBeginLoc(),
                       diag::err_expected_bounds_for_assignment)
                    << RHS->getSourceRange();
                RightBounds = S.CreateInvalidBoundsExpr();
              }
              CheckBoundsDeclAtAssignment(E->getExprLoc(), LHS, LHSTargetBounds,
                                          RHS, RightBounds, State.EquivExprs,
                                          CSS);
            }
          }
        }

        // Check that the LHS lvalue of the assignment has bounds, if it is an
        // lvalue that was produced by dereferencing an _Array_ptr.
        bool LHSNeedsBoundsCheck = false;
        OperationKind OpKind = (E->getOpcode() == BO_Assign) ?
          OperationKind::Assign : OperationKind::Other;
        LHSNeedsBoundsCheck = AddBoundsCheck(LHS, OpKind, CSS,
                                             &State.EquivExprs,
                                             LHSLValueBounds);
        if (DumpBounds && (LHSNeedsBoundsCheck ||
                            (LHSTargetBounds && !LHSTargetBounds->isUnknown()))) {
          if (RightBounds && RightBounds->isUnknown())
            RightBounds = S.CreateInvalidBoundsExpr();
          DumpAssignmentBounds(llvm::outs(), E, LHSTargetBounds, RightBounds);
        }
      }

      return ResultBounds;
    }

    // CheckCallExpr returns the bounds for the value produced by e.
    // e is an rvalue.
    BoundsExpr *CheckCallExpr(CallExpr *E, CheckedScopeSpecifier CSS,
                              CheckingState &State,
                              CHKCBindTemporaryExpr *Binding = nullptr) {
      BoundsExpr *ResultBounds = CallExprBounds(E, Binding, CSS);

      QualType CalleeType = E->getCallee()->getType();
      // Extract the pointee type.  The caller type could be a regular pointer
      // type or a block pointer type.
      QualType PointeeType;
      if (const PointerType *FuncPtrTy = CalleeType->getAs<PointerType>())
        PointeeType = FuncPtrTy->getPointeeType();
      else if (const BlockPointerType *BlockPtrTy = CalleeType->getAs<BlockPointerType>())
        PointeeType = BlockPtrTy->getPointeeType();
      else {
        llvm_unreachable("Unexpected callee type");
        return BoundsUtil::CreateBoundsInferenceError(S);
      }

      const FunctionType *FuncTy = PointeeType->getAs<FunctionType>();
      assert(FuncTy);
      const FunctionProtoType *FuncProtoTy = FuncTy->getAs<FunctionProtoType>();

      // If the callee and arguments will not be checked during
      // the bounds declaration checking below, check them here.
      if (!FuncProtoTy) {
        CheckChildren(E, CSS, State);
        return ResultBounds;
      }
      if (!FuncProtoTy->hasParamAnnots()) {
        CheckChildren(E, CSS, State);
        return ResultBounds;
      }

      // Check the callee since CheckCallExpr should check
      // all its children.  The arguments will be checked below.
      Check(E->getCallee(), CSS, State);

      unsigned NumParams = FuncProtoTy->getNumParams();
      unsigned NumArgs = E->getNumArgs();
      unsigned Count = (NumParams < NumArgs) ? NumParams : NumArgs;
      ArrayRef<Expr *> ArgExprs = llvm::makeArrayRef(const_cast<Expr**>(E->getArgs()), E->getNumArgs());

      for (unsigned i = 0; i < Count; i++) {
        // Check each argument.
        Expr *Arg = E->getArg(i);
        BoundsExpr *ArgBounds = Check(Arg, CSS, State);

        QualType ParamType = FuncProtoTy->getParamType(i);
        // Skip checking bounds for unchecked pointer parameters, unless
        // the argument was subject to a bounds-safe interface cast.
        if (ParamType->isUncheckedPointerType() && !IsBoundsSafeInterfaceAssignment(ParamType, E->getArg(i))) {
          continue;
        }
        // We want to check the argument expression implies the desired parameter bounds.
        // To compute the desired parameter bounds, we substitute the arguments for
        // parameters in the parameter bounds expression.
        const BoundsAnnotations ParamAnnots = FuncProtoTy->getParamAnnots(i);
        const BoundsExpr *ParamBounds = ParamAnnots.getBoundsExpr();
        const InteropTypeExpr *ParamIType = ParamAnnots.getInteropTypeExpr();
        if (!ParamBounds && !ParamIType)
          continue;

        bool UsedIType = false;
        if (!ParamBounds && ParamIType) {
          ParamBounds = CreateTypeBasedBounds(nullptr, ParamIType->getType(),
                                                true, true);
          UsedIType = true;
        }

        // Check after handling the interop type annotation, not before, because
        // handling the interop type annotation could make the bounds known.
        if (ParamBounds->isUnknown())
          continue;

        ArgBounds = S.CheckNonModifyingBounds(ArgBounds, Arg);
        if (ArgBounds->isUnknown()) {
          S.Diag(Arg->getBeginLoc(),
                  diag::err_expected_bounds_for_argument) << (i + 1) <<
            Arg->getSourceRange();
          ArgBounds = S.CreateInvalidBoundsExpr();
          continue;
        } else if (ArgBounds->isInvalid())
          continue;

        // Concretize parameter bounds with argument expressions. This fails
        // and returns null if an argument expression is a modifying
        // expression,  We issue an error during concretization about that.
        BoundsExpr *SubstParamBounds =
          S.ConcretizeFromFunctionTypeWithArgs(
            const_cast<BoundsExpr *>(ParamBounds),
            ArgExprs,
            Sema::NonModifyingContext::NMC_Function_Parameter,
            Sema::NonModifyingMessage::NMM_Error);

        if (!SubstParamBounds)
          continue;

        // Put the parameter bounds in a standard form if necessary.
        if (SubstParamBounds->isElementCount() ||
            SubstParamBounds->isByteCount()) {
          // TODO: turn this check on as part of adding temporary variables for
          // calls.
          // Turning it on now would cause errors to be issued for arguments
          // that are calls.
          if (true /* S.CheckIsNonModifying(Arg,
                              Sema::NonModifyingContext::NMC_Function_Parameter,
                                    Sema::NonModifyingMessage::NMM_Error) */) {
            Expr *TypedArg = Arg;
            // The bounds expression is for an interface type. Retype the
            // argument to the interface type.
            if (UsedIType) {
              TypedArg = ExprCreatorUtil::CreateExplicitCast(
                S, ParamIType->getType(), CK_BitCast, Arg, true);
            }
            SubstParamBounds = BoundsUtil::ExpandToRange(S, TypedArg,
                                    const_cast<BoundsExpr *>(SubstParamBounds));
            } else
              continue;
        }

        if (DumpBounds) {
          DumpCallArgumentBounds(llvm::outs(), FuncProtoTy->getParamAnnots(i).getBoundsExpr(), Arg, SubstParamBounds, ArgBounds);
        }

        CheckBoundsDeclAtCallArg(i, SubstParamBounds, Arg, ArgBounds, CSS, State.EquivExprs);
      }

      // Check any arguments that are beyond the number of function
      // parameters.
      for (unsigned i = Count; i < NumArgs; i++) {
        Expr *Arg = E->getArg(i);
        Check(Arg, CSS, State);
      }

      // State.SameValue is empty for call expressions.
      State.SameValue.clear();

      return ResultBounds;
    }

    // If e is an rvalue, CheckCastExpr returns the bounds for
    // the value produced by e.
    // If e is an lvalue, it returns unknown bounds (CheckCastLValue
    // should be called instead).
    // This includes both ImplicitCastExprs and CStyleCastExprs.
    BoundsExpr *CheckCastExpr(CastExpr *E, CheckedScopeSpecifier CSS,
                              CheckingState &State) {
      // If the rvalue bounds for e cannot be determined,
      // e may be an lvalue (or may have unknown rvalue bounds).
      BoundsExpr *ResultBounds = BoundsUtil::CreateBoundsUnknown(S);

      Expr *SubExpr = E->getSubExpr();
      CastKind CK = E->getCastKind();

      bool IncludeNullTerm =
          E->getType()->getPointeeOrArrayElementType()->isNtCheckedArrayType();
      bool PreviousIncludeNullTerminator = IncludeNullTerminator;
      IncludeNullTerminator = IncludeNullTerm;

      // Infer the bounds for the target of the subexpression e1.
      BoundsExpr *SubExprTargetBounds = GetLValueTargetBounds(SubExpr, CSS);

      // Infer the lvalue or rvalue bounds of the subexpression e1,
      // setting State to contain the results for e1.
      BoundsExpr *SubExprLValueBounds, *SubExprBounds;
      InferBounds(SubExpr, CSS, SubExprLValueBounds,
                  SubExprBounds, State);

      IncludeNullTerminator = PreviousIncludeNullTerminator;

      // Update the set State.SameValue of expressions that produce the
      // same value as e.
      if (CK == CastKind::CK_ArrayToPointerDecay) {
        // State.SameValue = { e } for lvalues with array type.
        if (!CreatesNewObject(E) && ExprUtil::CheckIsNonModifying(S, E))
          State.SameValue = { E };
      } else if (CK == CastKind::CK_LValueToRValue) {
        if (E->getType()->isArrayType()) {
          // State.SameValue = { e } for lvalues with array type.
          if (!CreatesNewObject(E) && ExprUtil::CheckIsNonModifying(S, E))
            State.SameValue = { E };
        }
        else {
          // If e appears in some set F in State.EquivExprs,
          // State.SameValue = F.
          State.SameValue = GetEqualExprSetContainingExpr(E, State.EquivExprs);
          if (State.SameValue.size() == 0) {
            // Otherwise, if e is nonmodifying and does not read memory via a
            // pointer, State.SameValue = { e }.  Otherwise, State.SameValue
            // is empty.
            if (ExprUtil::CheckIsNonModifying(S, E) &&
                !ExprUtil::ReadsMemoryViaPointer(E) &&
                !CreatesNewObject(E))
              State.SameValue.push_back(E);
          }
        }
      } else
        // Use the default rules to update State.SameValue for e using
        // the current State.SameValue for the subexpression e1.
        UpdateSameValue(E, State.SameValue, State.SameValue);

      // Casts to _Ptr narrow the bounds.  If the cast to
      // _Ptr is invalid, that will be diagnosed separately.
      if (E->getStmtClass() == Stmt::ImplicitCastExprClass ||
          E->getStmtClass() == Stmt::CStyleCastExprClass) {
        if (E->getType()->isCheckedPointerPtrType())
          ResultBounds = CreateTypeBasedBounds(E, E->getType(), false, false);
        else
          ResultBounds = RValueCastBounds(E, SubExprTargetBounds,
                                          SubExprLValueBounds,
                                          SubExprBounds, State);
      }

      CheckDisallowedFunctionPtrCasts(E);

      if (CK == CK_LValueToRValue && !E->getType()->isArrayType()) {
        bool NeedsBoundsCheck = AddBoundsCheck(SubExpr, OperationKind::Read,
                                               CSS, &State.EquivExprs,
                                               SubExprLValueBounds);
        if (NeedsBoundsCheck && DumpBounds)
          DumpExpression(llvm::outs(), E);
        return ResultBounds;
      }

      // Handle dynamic_bounds_casts.
      //
      // If the inferred bounds of the subexpression are:
      // - bounds(unknown), this is a compile-time error.
      // - bounds(any), there is no runtime checks.
      // - bounds(lb, ub):  If the declared bounds of the cast operation are
      // (e2, e3),  a runtime check that lb <= e2 && e3 <= ub is inserted
      // during code generation.
      if (CK == CK_DynamicPtrBounds || CK == CK_AssumePtrBounds) {
        CHKCBindTemporaryExpr *TempExpr = dyn_cast<CHKCBindTemporaryExpr>(SubExpr);
        assert(TempExpr);

        // These bounds may be computed and tested at runtime.  Don't
        // recompute any expressions computed to temporaries already.
        Expr *TempUse = CreateTemporaryUse(TempExpr);

        Expr *SubExprAtNewType =
          ExprCreatorUtil::CreateExplicitCast(S, E->getType(),
                                              CastKind::CK_BitCast,
                                              TempUse, true);

        if (CK == CK_AssumePtrBounds)
          return BoundsUtil::ExpandToRange(S, SubExprAtNewType, E->getBoundsExpr());

        BoundsExpr *DeclaredBounds = E->getBoundsExpr();
        BoundsExpr *NormalizedBounds = BoundsUtil::ExpandToRange(S,
                                                      SubExprAtNewType,
                                                      DeclaredBounds);

        SubExprBounds = S.CheckNonModifyingBounds(SubExprBounds, SubExpr);
        if (SubExprBounds->isUnknown()) {
          S.Diag(SubExpr->getBeginLoc(), diag::err_expected_bounds);
        }

        assert(NormalizedBounds);

        E->setNormalizedBoundsExpr(NormalizedBounds);
        E->setSubExprBoundsExpr(SubExprBounds);

        if (DumpBounds)
          DumpBoundsCastBounds(llvm::outs(), E, DeclaredBounds, NormalizedBounds, SubExprBounds);
        
        return BoundsUtil::ExpandToRange(S, SubExprAtNewType, E->getBoundsExpr());
      }

      // Casts to _Ptr type must have a source for which we can infer bounds.
      if ((CK == CK_BitCast || CK == CK_IntegralToPointer) &&
          E->getType()->isCheckedPointerPtrType() &&
          !E->getType()->isFunctionPointerType()) {
        SubExprBounds = S.CheckNonModifyingBounds(SubExprBounds, SubExpr);
        if (SubExprBounds->isUnknown()) {
          S.Diag(SubExpr->getBeginLoc(),
                  diag::err_expected_bounds_for_ptr_cast)
                  << SubExpr->getSourceRange();
          SubExprBounds = S.CreateInvalidBoundsExpr();
        } else {
          BoundsExpr *TargetBounds =
            CreateTypeBasedBounds(E, E->getType(), false, false);
          CheckBoundsDeclAtStaticPtrCast(E, TargetBounds, SubExpr,
                                         SubExprBounds, CSS);
        }
        assert(SubExprBounds);
        assert(!E->getSubExprBoundsExpr());
        E->setSubExprBoundsExpr(SubExprBounds);
        if (DumpBounds)
          DumpExpression(llvm::outs(), E);
      }

      return ResultBounds;
    }

    // If e is an rvalue, CheckUnaryOperator returns the bounds for
    // the value produced by e.
    // If e is an lvalue, CheckUnaryLValue should be called instead.
    BoundsExpr *CheckUnaryOperator(UnaryOperator *E, CheckedScopeSpecifier CSS,
                                   CheckingState &State) {
      UnaryOperatorKind Op = E->getOpcode();
      Expr *SubExpr = E->getSubExpr();

      // Infer the bounds for the target of the subexpression e1.
      BoundsExpr *SubExprTargetBounds = GetLValueTargetBounds(SubExpr, CSS);

      // Infer the lvalue or rvalue bounds of the subexpression e1,
      // setting State to contain the results for e1.
      BoundsExpr *SubExprLValueBounds, *SubExprBounds;
      InferBounds(SubExpr, CSS, SubExprLValueBounds,
                  SubExprBounds, State);

      if (Op == UO_AddrOf)
        S.CheckAddressTakenMembers(E);

      if (E->isIncrementDecrementOp()) {
        bool NeedsBoundsCheck = AddBoundsCheck(SubExpr, OperationKind::Other,
                                               CSS, &State.EquivExprs,
                                               SubExprLValueBounds);
        if (NeedsBoundsCheck && DumpBounds)
          DumpExpression(llvm::outs(), E);
      }

      // `*e` is not an rvalue.
      if (Op == UnaryOperatorKind::UO_Deref)
        return BoundsUtil::CreateBoundsInferenceError(S);

      // Check inc/dec operators `++e1`, `e1++`, `--e1`, `e1--`.
      // At this point, State contains EquivExprs and SameValue for `e1`.
      if (UnaryOperator::isIncrementDecrementOp(Op)) {
        // `++e1`, `e1++`, `--e1`, `e1--` all have bounds of `e1`.
        // `e1` is an lvalue, so its bounds are either:
        // 1. The observed bounds for the value produced by `e1` as recorded
        // in State.ObservedBounds (if any), or:
        // 2. The target bounds of `e1`.
        // These bounds may be updated if `e1` is a variable, member
        // expression, pointer dereference, or array subscript.
        BoundsExpr *IncDecResultBounds = SubExprTargetBounds;
        if (BoundsExpr *ObservedBounds = GetLValueObservedBounds(SubExpr, State))
          IncDecResultBounds = ObservedBounds;

        // Create the target of the implied assignment `e1 = e1 +/- 1`.
        CastExpr *Target = ExprCreatorUtil::CreateImplicitCast(S, SubExpr,
                                              CK_LValueToRValue,
                                              SubExpr->getType());

        // Only use the RHS `e1 +/1 ` of the implied assignment to update
        // the checking state if the integer constant 1 can be created, which
        // is only true if `e1` has integer or pointer type.
        IntegerLiteral *One = ExprCreatorUtil::CreateIntegerLiteral(
                                Context, 1, SubExpr->getType());
        Expr *RHS = nullptr;
        if (One) {
          BinaryOperatorKind RHSOp = UnaryOperator::isIncrementOp(Op) ?
                                      BinaryOperatorKind::BO_Add :
                                      BinaryOperatorKind::BO_Sub;
          RHS = ExprCreatorUtil::CreateBinaryOperator(S, SubExpr, One, RHSOp);
        }

        // Update the checking state and result bounds.
        BoundsExpr *RHSBounds = IncDecResultBounds;
        if (VariableUtil::GetLValueVariable(S, SubExpr)) {
          // Update SameValue to be the set of expressions that produce the
          // same value as the RHS `e1 +/- 1` (if the RHS could be created).
          UpdateSameValue(E, State.SameValue, State.SameValue, RHS);
          // The bounds of the RHS `e1 +/- 1` are the rvalue bounds of the
          // rvalue cast `e1`.
          RHSBounds = RValueCastBounds(Target, SubExprTargetBounds,
                                       SubExprLValueBounds,
                                       SubExprBounds, State);
        }
        bool StateUpdated = false;
        IncDecResultBounds = UpdateAfterAssignment(SubExpr, SubExprTargetBounds,
                                                   E, Target, RHS, RHSBounds,
                                                   CSS, State, StateUpdated);

        // Update the set SameValue of expressions that produce the same
        // value as `e`.
        if (One) {
          // For integer or pointer-typed expressions, create the expression
          // Val that is equivalent to `e` in the program state after the
          // increment/decrement expression `e` has executed.
          // (The call to UpdateSameValue will only add Val to SameValue if
          // Val is a non-modifying expression).

          // `++e1` and `--e1` produce the same value as the rvalue cast of
          // `e1` after executing `++e1` or `--e1`.
          Expr *Val = Target;
          // `e1++` produces the same value as `e1 - 1` after executing `e1++`.
          if (Op == UnaryOperatorKind::UO_PostInc)
            Val = ExprCreatorUtil::CreateBinaryOperator(S, SubExpr, One,
                                    BinaryOperatorKind::BO_Sub);
          // `e1--` produces the same value as `e1 + 1` after executing `e1--`.
          else if (Op == UnaryOperatorKind::UO_PostDec)
            Val = ExprCreatorUtil::CreateBinaryOperator(S, SubExpr, One,
                                    BinaryOperatorKind::BO_Add);
          UpdateSameValue(E, State.SameValue, State.SameValue, Val);
        } else {
          // SameValue is empty for expressions where the integer constant 1
          // could not be constructed (e.g. floating point expressions).
          State.SameValue.clear();
        }

        return IncDecResultBounds;
      }

      // `&e` has the bounds of `e`.
      // `e` is an lvalue, so its bounds are its lvalue bounds.
      // State.SameValue for `&e` remains the same as State.SameValue for `e`.
      if (Op == UnaryOperatorKind::UO_AddrOf) {

        // Functions have bounds corresponding to the empty range.
        if (SubExpr->getType()->isFunctionType())
          return CreateBoundsEmpty();

        return SubExprLValueBounds;
      }

      // Update State.SameValue for `!e`, `+e`, `-e`, and `~e`
      // using the current State.SameValue for `e`.
      UpdateSameValue(E, State.SameValue, State.SameValue);

      // `!e` has empty bounds.
      if (Op == UnaryOperatorKind::UO_LNot)
        return CreateBoundsEmpty();

      // `+e`, `-e`, `~e` all have bounds of `e`. `e` is an rvalue.
      if (Op == UnaryOperatorKind::UO_Plus ||
          Op == UnaryOperatorKind::UO_Minus ||
          Op == UnaryOperatorKind::UO_Not)
        return SubExprBounds;

      // We cannot infer the bounds of other unary operators.
      return BoundsUtil::CreateBoundsAlwaysUnknown(S);
    }

    // CheckVarDecl returns empty bounds.
    BoundsExpr *CheckVarDecl(VarDecl *D, CheckedScopeSpecifier CSS,
                             CheckingState &State, bool CheckBounds = false) {
      BoundsExpr *ResultBounds = CreateBoundsEmpty();

      Expr *Init = D->getInit();
      if (Init && Init->containsErrors())
        return ResultBounds;
      BoundsExpr *InitBounds = nullptr;
      const AbstractSet *A = nullptr;

      // If there is an initializer, check it, and update the state to record
      // expression equality implied by initialization. After checking Init,
      // State.SameValue will contain non-modifying expressions that produce
      // values equivalent to the value produced by Init.
      if (Init) {
        InitBounds = Check(Init, CSS, State);

        // Create an rvalue expression for v. v could be an array or
        // non-array variable.
        DeclRefExpr *TargetDeclRef = ExprCreatorUtil::CreateVarUse(S, D);
        A = AbstractSetMgr.GetOrCreateAbstractSet(TargetDeclRef);
        CastKind Kind;
        QualType TargetTy;
        if (D->getType()->isArrayType()) {
          Kind = CK_ArrayToPointerDecay;
          TargetTy = S.getASTContext().getArrayDecayedType(D->getType());
        } else {
          Kind = CK_LValueToRValue;
          TargetTy = D->getType();
        }
        Expr *TargetExpr =
          ExprCreatorUtil::CreateImplicitCast(S, TargetDeclRef, Kind, TargetTy);

        // Record equality between the target and initializer.
        RecordEqualityWithTarget(TargetDeclRef, TargetExpr, Init,
                                 /*AllowTempEquality=*/true, State);
      }

      if (D->isInvalidDecl())
        return ResultBounds;

      if (isa<ParmVarDecl>(D))
        return ResultBounds;

      VarDecl::DefinitionKind defKind = D->isThisDeclarationADefinition();
      if (defKind == VarDecl::DefinitionKind::DeclarationOnly)
        return ResultBounds;

      // Handle variables with bounds declarations
      BoundsExpr *DeclaredBounds = D->getBoundsExpr();
      if (!DeclaredBounds || DeclaredBounds->isInvalid() ||
          DeclaredBounds->isUnknown())
        return ResultBounds;

      // TODO: checkedc-clang issue #862: for array types, check that any
      // declared bounds at the point of initialization are true based on
      // the array size.

      // If there is a scalar initializer, record the initializer bounds as the
      // observed bounds for the variable and check that the initializer meets
      // the bounds requirements for the variable.  For non-scalar types
      // arrays, structs, and unions), the amount of storage allocated depends
      // on the type, so we don't need to check the initializer bounds.
      if (Init && D->getType()->isScalarType()) {
        assert(D->getInitStyle() == VarDecl::InitializationStyle::CInit);
        InitBounds = S.CheckNonModifyingBounds(InitBounds, Init);
        State.ObservedBounds[A] = InitBounds;
        if (InitBounds->isUnknown()) {
          if (CheckBounds)
            // TODO: need some place to record the initializer bounds
            S.Diag(Init->getBeginLoc(), diag::err_expected_bounds_for_initializer)
                << Init->getSourceRange();
          InitBounds = S.CreateInvalidBoundsExpr();
        } else if (CheckBounds) {
          BoundsExpr *NormalizedDeclaredBounds = S.NormalizeBounds(D);
          CheckBoundsDeclAtInitializer(D->getLocation(), D, NormalizedDeclaredBounds,
            Init, InitBounds, State.EquivExprs, CSS);
        }
        if (DumpBounds)
          DumpInitializerBounds(llvm::outs(), D, DeclaredBounds, InitBounds);
      }

      return ResultBounds;
    }

    // CheckReturnStmt returns empty bounds.
    BoundsExpr *CheckReturnStmt(ReturnStmt *RS, CheckedScopeSpecifier CSS,
                                CheckingState &State) {
      BoundsExpr *ResultBounds = CreateBoundsEmpty();

      Expr *RetValue = RS->getRetValue();

      if (!RetValue)
        // We already issued an error message for this case.
        return ResultBounds;

      // Check the return value if it exists.
      BoundsExpr *RetValueBounds = Check(RetValue, CSS, State);

      // Check that the return expression bounds imply the return bounds.
      ValidateReturnBounds(RS, RetValue, RetValueBounds, State.EquivExprs,
                           State.SameValue, CSS);

      // TODO: Check that any parameters used in the return bounds are
      // unmodified.
      return ResultBounds;
    }

    // If e is an rvalue, CheckTemporaryBinding returns the bounds for
    // the value produced by e.
    // If e is an lvalue, CheckTempBindingLValue should be called instead.
    BoundsExpr *CheckTemporaryBinding(CHKCBindTemporaryExpr *E,
                                      CheckedScopeSpecifier CSS,
                                      CheckingState &State) {
      Expr *Child = E->getSubExpr();

      BoundsExpr *SubExprBounds = nullptr;
      if (CallExpr *CE = dyn_cast<CallExpr>(Child))
        SubExprBounds = CheckCallExpr(CE, CSS, State, E);
      else
        SubExprBounds = Check(Child, CSS, State);

      UpdateSameValue(E, State.SameValue, State.SameValue);
      return SubExprBounds;
    }

    // CheckBoundsValueExpr returns the bounds for the value produced by e.
    // e is an rvalue.
    BoundsExpr *CheckBoundsValueExpr(BoundsValueExpr *E,
                                     CheckedScopeSpecifier CSS,
                                     CheckingState &State) {
      Expr *Binding = E->getTemporaryBinding();
      return Check(Binding, CSS, State);
    }

    // CheckConditionalOperator returns the bounds for the value produced by e.
    // e is an rvalue of the form `e1 ? e2 : e3`.
    BoundsExpr *CheckConditionalOperator(AbstractConditionalOperator *E,
                                         CheckedScopeSpecifier CSS,
                                         CheckingState &State) {
      // Check the condition `e1`.
      Check(E->getCond(), CSS, State);

      // Check the "true" arm `e2`.
      CheckingState StateTrueArm;
      StateTrueArm.EquivExprs = State.EquivExprs;
      StateTrueArm.ObservedBounds = State.ObservedBounds;
      BoundsExpr *BoundsTrueArm = Check(E->getTrueExpr(), CSS, StateTrueArm);

      // Check the "false" arm `e3`.
      CheckingState StateFalseArm;
      StateFalseArm.EquivExprs = State.EquivExprs;
      StateFalseArm.ObservedBounds = State.ObservedBounds;
      BoundsExpr *BoundsFalseArm = Check(E->getFalseExpr(), CSS, StateFalseArm);

      // TODO: handle uses of temporaries bounds in only one arm.

      if (EqualContexts(StateTrueArm.ObservedBounds,
                        StateFalseArm.ObservedBounds)) {
        // If checking each arm produces two identical bounds contexts,
        // the final context is the context from checking the true arm.
        State.ObservedBounds = StateTrueArm.ObservedBounds;
      } else {
        // If checking each arm produces two different bounds contexts,
        // validate each arm's context separately.

        // Get the bounds that were updated in each arm.
        BoundsContextTy TrueBounds = ContextDifference(
                                        StateTrueArm.ObservedBounds,
                                        State.ObservedBounds);
        BoundsContextTy FalseBounds = ContextDifference(
                                        StateFalseArm.ObservedBounds,
                                        State.ObservedBounds);

        // For any AbstractSet A whose bounds were updated in the false arm
        // but not in the true arm, the bounds of A in the true arm should
        // be validated as well. These bounds may be invalid, e.g. if the
        // bounds of A were updated in the condition `e1`.
        for (const auto &Pair : FalseBounds) {
          const AbstractSet *A = Pair.first;
          if (TrueBounds.find(A) == TrueBounds.end())
            TrueBounds[A] = StateTrueArm.ObservedBounds[A];
        }
        StateTrueArm.ObservedBounds = TrueBounds;

        // For any variable A whose bounds were updated in the true arm
        // but not in the false arm, the bounds of A in the false arm should
        // be validated as well.
        for (const auto &Pair : TrueBounds) {
          const AbstractSet *A = Pair.first;
          if (FalseBounds.find(A) == FalseBounds.end())
            FalseBounds[A] = StateFalseArm.ObservedBounds[A];
        }
        StateFalseArm.ObservedBounds = FalseBounds;

        // Validate the bounds that were updated in either arm.
        ValidateBoundsContext(E->getTrueExpr(), StateTrueArm, CSS);
        ValidateBoundsContext(E->getFalseExpr(), StateFalseArm, CSS);

        // For each variable v whose bounds were updated in the true or false arm,
        // reset the observed bounds of v to the declared bounds of v.
        for (const auto &Pair : StateTrueArm.ObservedBounds) {
          const AbstractSet *A = Pair.first;
          BoundsExpr *DeclaredBounds =
            S.GetLValueDeclaredBounds(A->GetRepresentative(), CSS);
          State.ObservedBounds[A] = DeclaredBounds;
        }
        for (const auto &Pair : StateFalseArm.ObservedBounds) {
          const AbstractSet *A = Pair.first;
          BoundsExpr *DeclaredBounds =
            S.GetLValueDeclaredBounds(A->GetRepresentative(), CSS);
          State.ObservedBounds[A] = DeclaredBounds;
        }
      }

      State.EquivExprs = IntersectEquivExprs(StateTrueArm.EquivExprs,
                                             StateFalseArm.EquivExprs);

      State.SameValue = IntersectExprSets(StateTrueArm.SameValue,
                                          StateFalseArm.SameValue);
      if (!CreatesNewObject(E) && ExprUtil::CheckIsNonModifying(S, E) &&
          !EqualExprsContainsExpr(State.SameValue, E))
        State.SameValue.push_back(E);

      // The bounds of `e` are the greatest lower bound of the bounds of `e2`
      // and the bounds of `e3`.

      // If bounds expressions B1 and B2 are equivalent, the greatest lower
      // bound of B1 and B2 is B1.
      if (S.Context.EquivalentBounds(BoundsTrueArm, BoundsFalseArm, &State.EquivExprs))
        return BoundsTrueArm;

      // The greatest lower bound of bounds(any) and B is B, where B is an
      // arbitrary bounds expression.
      if (BoundsTrueArm->isAny())
        return BoundsFalseArm;
      if (BoundsFalseArm->isAny())
        return BoundsTrueArm;

      // If the bounds for `e2` and `e3` are not equivalent, and neither is
      // bounds(any), the bounds for `e` cannot be determined.
      return BoundsUtil::CreateBoundsAlwaysUnknown(S);
    }

  // Methods to infer both:
  // 1. Bounds for an expression that produces an lvalue, and
  // 2. Bounds for the target of an expression that produces an lvalue.

  private:

    // CheckDeclRefExpr returns the lvalue and target bounds of e.
    // e is an lvalue.
    BoundsExpr *CheckDeclRefExpr(DeclRefExpr *E, CheckedScopeSpecifier CSS,
                                 CheckingState &State) {
      CheckChildren(E, CSS, State);
      State.SameValue.clear();

      VarDecl *VD = dyn_cast<VarDecl>(E->getDecl());
      BoundsExpr *B = nullptr;
      if (VD)
        B = VD->getBoundsExpr();

      if (E->getType()->isArrayType()) {
        if (!VD) {
          llvm_unreachable("declref with array type not a vardecl");
          return BoundsUtil::CreateBoundsInferenceError(S);
        }

        // Update SameValue for variables with array type.
        const ConstantArrayType *CAT = Context.getAsConstantArrayType(E->getType());
        if (CAT) {
          if (E->getType()->isCheckedArrayType())
            State.SameValue.push_back(E);
          else if (VD->hasLocalStorage() || VD->hasExternalStorage())
            State.SameValue.push_back(E);
        }

        // Declared bounds override the bounds based on the array type.
        if (B) {
          Expr *Base = ExprCreatorUtil::CreateImplicitCast(S, E,
                         CastKind::CK_ArrayToPointerDecay,
                         Context.getDecayedType(E->getType()));
          return BoundsUtil::ExpandToRange(S, Base, B);
        }

        // If B is an interop type annotation, the type must be identical
        // to the declared type, modulo checkedness.  So it is OK to
        // compute the array bounds based on the original type.
        return ArrayExprBounds(E);
      }

      if (E->getType()->isFunctionType()) {
        // Only function decl refs should have function type.
        assert(isa<FunctionDecl>(E->getDecl()));
        return CreateBoundsEmpty();
      }

      Expr *AddrOf = CreateAddressOfOperator(E);
      // SameValue = { &v } for variables v that do not have array type.
      State.SameValue.push_back(AddrOf);
      return CreateSingleElementBounds(AddrOf);
    }

    // If e is an lvalue, CheckUnaryLValue returns the
    // lvalue and target bounds of e.
    // If e is an rvalue, CheckUnaryOperator should be called instead.
    BoundsExpr *CheckUnaryLValue(UnaryOperator *E, CheckedScopeSpecifier CSS,
                                 CheckingState &State) {
      BoundsExpr *SubExprBounds = Check(E->getSubExpr(), CSS, State);

      if (E->getOpcode() == UnaryOperatorKind::UO_Deref) {
        // SameValue is empty for pointer dereferences.
        State.SameValue.clear();

        // The lvalue bounds of *e are the rvalue bounds of e.
        return SubExprBounds;
      }

      return BoundsUtil::CreateBoundsInferenceError(S);
    }

    // CheckArraySubscriptExpr returns the lvalue and target bounds of e.
    // e is an lvalue.
    BoundsExpr *CheckArraySubscriptExpr(ArraySubscriptExpr *E,
                                        CheckedScopeSpecifier CSS,
                                        CheckingState &State) {
      // e1[e2] is a synonym for *(e1 + e2).  The bounds are
      // the bounds of e1 + e2, which reduces to the bounds
      // of whichever subexpression has pointer type.
      // getBase returns the pointer-typed expression.
      BoundsExpr *Bounds = Check(E->getBase(), CSS, State);
      Check(E->getIdx(), CSS, State);

      // SameValue is empty for array subscript expressions.
      State.SameValue.clear();

      return Bounds;
    }

    // CheckMemberExpr returns the lvalue and target bounds of e.
    // e is an lvalue.
    //
    // A member expression is a narrowing operator that shrinks the range of
    // memory to which the base refers to a specific member.  We always bounds
    // check the base.  That way we know that the lvalue produced by the
    // member points to a valid range of memory given by
    // (lvalue, lvalue + 1).   The lvalue is interpreted as a pointer to T,
    // where T is the type of the member.
    BoundsExpr *CheckMemberExpr(MemberExpr *E, CheckedScopeSpecifier CSS,
                                CheckingState &State) {
      // The lvalue bounds must be inferred before performing any side
      // effects on the base, since inferring these bounds may call
      // PruneTemporaryBindings.
      BoundsExpr *Bounds = MemberExprBounds(E, CSS);

      // Infer the lvalue or rvalue bounds of the base.
      Expr *Base = E->getBase();
      BoundsExpr *BaseLValueBounds, *BaseBounds;
      InferBounds(Base, CSS, BaseLValueBounds, BaseBounds, State);

      // Clear State.SameValue to avoid adding false equality information.
      // TODO: implement updating state for member expressions.
      State.SameValue.clear();

      bool NeedsBoundsCheck = AddMemberBaseBoundsCheck(E, CSS,
                                                       &State.EquivExprs,
                                                       BaseLValueBounds,
                                                       BaseBounds);
      if (NeedsBoundsCheck && DumpBounds)
        DumpExpression(llvm::outs(), E);
      return Bounds;
    }

    // If e is an lvalue, CheckCastLValue returns the
    // lvalue and target bounds of e.
    // If e is an rvalue, CheckCastExpr should be called instead.
    BoundsExpr *CheckCastLValue(CastExpr *E, CheckedScopeSpecifier CSS,
                                CheckingState &State) {
      // An LValueBitCast adjusts the type of the lvalue.  The bounds are not
      // changed, except that their relative alignment may change (the bounds 
      // may only cover a partial object).  TODO: When we add relative
      // alignment support to the compiler, adjust the relative alignment.
      if (E->getCastKind() == CastKind::CK_LValueBitCast)
        return CheckLValue(E->getSubExpr(), CSS, State);

      CheckChildren(E, CSS, State);

      // Cast kinds other than LValueBitCast do not have lvalue bounds.
      return BoundsUtil::CreateBoundsAlwaysUnknown(S);
    }

    // If e is an lvalue, CheckTempBindingLValue returns the
    // lvalue and target bounds of e.
    // If e is an rvalue, CheckTemporaryBinding should be called instead.
    BoundsExpr *CheckTempBindingLValue(CHKCBindTemporaryExpr *E,
                                       CheckedScopeSpecifier CSS,
                                       CheckingState &State) {
      CheckChildren(E, CSS, State);

      Expr *SubExpr = E->getSubExpr()->IgnoreParens();

      if (isa<CompoundLiteralExpr>(SubExpr)) {
        // The lvalue bounds of a struct-typed compound literal expression e
        // are bounds(&value(temp(e), &value(temp(e)) + 1).
        if (E->getType()->isStructureType()) {
          Expr *TempUse = CreateTemporaryUse(E);
          Expr *Addr = CreateAddressOfOperator(TempUse);
          return BoundsUtil::ExpandToRange(S, Addr,
                                           Context.getPrebuiltCountOne());
        }

        // The lvalue bounds of an array-typed compound literal expression e
        // are based on the dimension size of e.
        if (E->getType()->isArrayType()) {
          BoundsExpr *BE = CreateBoundsForArrayType(E->getType());
          QualType PtrType = Context.getDecayedType(E->getType());
          Expr *ArrLValue = CreateTemporaryUse(E);
          Expr *Base = ExprCreatorUtil::CreateImplicitCast(S, ArrLValue,
                                          CastKind::CK_ArrayToPointerDecay,
                                          PtrType);
          return BoundsUtil::ExpandToRange(S, Base, BE);
        }

        // All other types of compound literals do not have lvalue bounds.
        return BoundsUtil::CreateBoundsAlwaysUnknown(S);
      }

      if (auto *SL = dyn_cast<StringLiteral>(SubExpr))
        return InferBoundsForStringLiteral(E, SL, E);

      if (auto *PE = dyn_cast<PredefinedExpr>(SubExpr)) {
        auto *SL = PE->getFunctionName();
        return InferBoundsForStringLiteral(E, SL, E);
      }

      return BoundsUtil::CreateBoundsAlwaysUnknown(S);
    }

  private:
    // Sets the bounds expressions based on whether e is an lvalue or an
    // rvalue expression.
    void InferBounds(Expr *E, CheckedScopeSpecifier CSS,
                     BoundsExpr *&LValueBounds,
                     BoundsExpr *&RValueBounds,
                     CheckingState &State) {
      LValueBounds = BoundsUtil::CreateBoundsUnknown(S);
      RValueBounds = BoundsUtil::CreateBoundsUnknown(S);
      if (E->isLValue())
        LValueBounds = CheckLValue(E, CSS, State);
      else if (E->isRValue())
        RValueBounds = Check(E, CSS, State);
    }

    // Methods to validate observed and declared bounds.

    // ValidateBoundsContext checks that, after checking a top-level CFG
    // statement S, for each variable v in the checking state observed bounds
    // context, the observed bounds of v imply the declared bounds of v.
    void ValidateBoundsContext(Stmt *S, CheckingState State,
                               CheckedScopeSpecifier CSS,
                               const CFGBlock *Block = nullptr) {
      // Construct a set of sets of equivalent expressions that contains all
      // the equality facts in State.EquivExprs, as well as any equality facts
      // implied by State.TargetSrcEquality.  These equality facts will only
      // be used to validate the bounds context and will not persist across
      // CFG statements.  The source expressions in State.TargetSrcEquality
      // do not meet the criteria for persistent inclusion in State.EquivExprs:
      // for example, they may create new objects or read memory via pointers.
      EquivExprSets EquivExprs = State.EquivExprs;
      for (auto const &Pair : State.TargetSrcEquality) {
        Expr *Target = Pair.first;
        Expr *Src = Pair.second;
        bool FoundTarget = false;
        for (auto I = EquivExprs.begin(); I != EquivExprs.end(); ++I) {
          if (EqualExprsContainsExpr(*I, Target)) {
            FoundTarget = true;
            I->push_back(Src);
            break;
          }
        }
        if (!FoundTarget)
          EquivExprs.push_back({Target, Src});
      }

      BoundsMapTy BoundsWidenedAndNotKilled =
        BoundsWideningAnalyzer.GetBoundsWidenedAndNotKilled(Block, S);

      for (auto const &Pair : State.ObservedBounds) {
        const AbstractSet *A = Pair.first;
        BoundsExpr *ObservedBounds = Pair.second;
        BoundsExpr *DeclaredBounds =
          this->S.GetLValueDeclaredBounds(A->GetRepresentative(), CSS);
        if (!DeclaredBounds || DeclaredBounds->isUnknown())
          continue;
        if (SkipBoundsValidation(A, CSS, State))
          continue;
        if (ObservedBounds->isUnknown())
          DiagnoseUnknownObservedBounds(S, A, DeclaredBounds, State);
        else {
          // If the lvalue expressions in A are variables represented by a
          // declaration Var, we should issue diagnostics for observed bounds
          // if Var is not in the set BoundsWidenedAndKilled which represents
          // variables whose bounds are widened in this block before statement
          // S and not killed by statement S.
          bool DiagnoseObservedBounds = true;
          if (const NamedDecl *V = A->GetDecl()) {
            if (const VarDecl *Var = dyn_cast<VarDecl>(V))
              DiagnoseObservedBounds = BoundsWidenedAndNotKilled.find(Var) ==
                                        BoundsWidenedAndNotKilled.end();
          }
          CheckObservedBounds(S, A, DeclaredBounds, ObservedBounds, State,
                              &EquivExprs, CSS, Block, DiagnoseObservedBounds);
        }
      }
    }

    // SkipBoundsValidation returns true if the observed bounds of A should
    // not be validated after the current top-level statement.
    //
    // The bounds of A should not be validated if the current statement is
    // in an unchecked scope, and A represents lvalue expressions that:
    // 1. Have unchecked type (i.e. their declared bounds were specified using
    //    a bounds-safe interface), and:
    // 2. Were not assigned a checked pointer at any point in the current
    //    top-level statement.
    bool SkipBoundsValidation(const AbstractSet *A, CheckedScopeSpecifier CSS,
                              CheckingState State) {
      if (CSS != CheckedScopeSpecifier::CSS_Unchecked)
        return false;

      if (A->GetRepresentative()->getType()->isCheckedPointerType() ||
          A->GetRepresentative()->getType()->isCheckedArrayType())
        return false;

      // State.LValuesAssignedChecked contains AbstractSets with unchecked type
      // that were assigned a checked pointer at some point in the current
      // statement. If A belongs to this set, we must validate the bounds of A.
      if (State.LValuesAssignedChecked.contains(A))
        return false;

      return true;
    }

    // DiagnoseUnknownObservedBounds emits an error message for an AbstractSet
    // A whose observed bounds are unknown after checking the top-level CFG
    // statement St.
    //
    // State contains information that is used to provide more context in
    // the diagnostic messages.
    void DiagnoseUnknownObservedBounds(Stmt *St, const AbstractSet *A,
                                       BoundsExpr *DeclaredBounds,
                                       CheckingState State) {

      SourceLocation Loc = BlameAssignmentWithinStmt(St, A, State,
                            diag::err_unknown_inferred_bounds);
      EmitDeclaredBoundsNote(A, DeclaredBounds, Loc);

      // The observed bounds of A are unknown because the original observed
      // bounds B of A used the value of an lvalue expression E, and there
      // was an assignment to E where E had no original value.
      auto LostLValueIt = State.LostLValues.find(A);
      if (LostLValueIt != State.LostLValues.end()) {
        std::pair<BoundsExpr *, Expr *> Lost = LostLValueIt->second;
        BoundsExpr *InitialObservedBounds = Lost.first;
        Expr *LostLValue = Lost.second;
        S.Diag(LostLValue->getBeginLoc(), diag::note_lost_expression)
          << LostLValue << InitialObservedBounds
          << A->GetRepresentative() << LostLValue->getSourceRange();
      }

      // The observed bounds of A are unknown because at least one expression
      // e with unknown bounds was assigned to an lvalue expression in A.
      // Emit a note for the first expression with unknown bounds that was
      // assigned to A (this expression is the only one that is tracked in
      // State.UnknownSrcBounds).
      auto BlameSrcIt = State.UnknownSrcBounds.find(A);
      if (BlameSrcIt != State.UnknownSrcBounds.end()) {
        Expr *Src = BlameSrcIt->second;
        S.Diag(Src->getBeginLoc(), diag::note_unknown_source_bounds)
            << Src << A->GetRepresentative() << Src->getSourceRange();
      }
    }

    // CheckObservedBounds checks that the observed bounds for the AbstractSet
    // A imply that the declared bounds for A are provably true after checking
    // the top-level CFG statement St.
    //
    // EquivExprs contains all equality facts contained in State.EquivExprs,
    // as well as any equality facts implied by State.TargetSrcEquality.
    void CheckObservedBounds(Stmt *St, const AbstractSet *A,
                             BoundsExpr *DeclaredBounds,
                             BoundsExpr *ObservedBounds, CheckingState State,
                             EquivExprSets *EquivExprs,
                             CheckedScopeSpecifier CSS,
                             const CFGBlock *Block,
                             bool DiagnoseObservedBounds) {
      ProofFailure Cause;
      FreeVariableListTy FreeVars;
      ProofResult Result = ProveBoundsDeclValidity(
          DeclaredBounds, ObservedBounds, Cause, EquivExprs, FreeVars);
      if (Result == ProofResult::True)
        return;

      // If A currently has widened bounds and the widened bounds of A are not
      // killed by the statement St, then the proof failure was caused by not
      // being able to prove the widened bounds of A imply the declared bounds
      // of A. Diagnostics should not be emitted in this case. Otherwise,
      // statements that make no changes to A or any expressions used in the
      // bounds of A would cause diagnostics to be emitted.
      // For example, the widened bounds (p, (p + 0) + 1) do not provably imply
      // the declared bounds (p, p + 0) due to the left-associativity of the
      // observed upper bound (p + 0) + 1.
      // TODO: checkedc-clang issue #867: the widened bounds of a variable
      // should provably imply the declared bounds of a variable.
      if (!DiagnoseObservedBounds)
        return;
      
      // Which diagnostic message to print?
      unsigned DiagId =
          (Result == ProofResult::False)
              ? (TestFailure(Cause, ProofFailure::HasFreeVariables)
                     ? diag::error_bounds_declaration_unprovable
                     : diag::error_bounds_declaration_invalid)
              : (CSS != CheckedScopeSpecifier::CSS_Unchecked
                     ? diag::warn_checked_scope_bounds_declaration_invalid
                     : diag::warn_bounds_declaration_invalid);

      SourceLocation Loc = BlameAssignmentWithinStmt(St, A, State, DiagId);
      if (Result == ProofResult::False)
        ExplainProofFailure(Loc, Cause, ProofStmtKind::BoundsDeclaration);
      
      if (TestFailure(Cause, ProofFailure::HasFreeVariables))
        DiagnoseFreeVariables(diag::note_free_variable_decl_or_inferred, Loc,
                              FreeVars);

      EmitDeclaredBoundsNote(A, DeclaredBounds, Loc);
      S.Diag(Loc, diag::note_expanded_inferred_bounds)
        << ObservedBounds << ObservedBounds->getSourceRange();
    }

    // BlameAssignmentWithinStmt prints a diagnostic message that highlights the
    // assignment expression in St that causes A's observed bounds to be unknown
    // or not provably valid.  If St is a DeclStmt, St itself and the
    // representative lvalue expression of A are highlighted.
    // BlameAssignmentWithinStmt returns the source location of the blamed
    // assignment.
    SourceLocation BlameAssignmentWithinStmt(Stmt *St, const AbstractSet *A,
                                             CheckingState State,
                                             unsigned DiagId) const {
      assert(St);
      const NamedDecl *V = A->GetDecl();
      SourceRange SrcRange = St->getSourceRange();
      auto BDCType = Sema::BoundsDeclarationCheck::BDC_Statement;

      // For a declaration, show the diagnostic message that starts at the
      // location of v rather than the beginning of St and return.  If the
      // message starts at the beginning of a declaration T v = e, then extra
      // diagnostics may be emitted for T.
      SourceLocation Loc = St->getBeginLoc();
      if (V && isa<DeclStmt>(St)) {
        Loc = V->getLocation();
        BDCType = Sema::BoundsDeclarationCheck::BDC_Initialization;
        S.Diag(Loc, DiagId) << BDCType << A->GetRepresentative()
          << SrcRange << SrcRange;
        return Loc;
      }

      // If not a declaration, find the assignment (if it exists) in St to blame
      // for the error or warning.
      auto It = State.BlameAssignments.find(A);
      if (It != State.BlameAssignments.end()) {
        Expr *BlameExpr = It->second;
        Loc = BlameExpr->getBeginLoc();
        SrcRange = BlameExpr->getSourceRange();

        // Choose the type of assignment E to show in the diagnostic messages
        // from: assignment (=), decrement (--) or increment (++). If none of
        // these cases match, the diagnostic message reports that the error is
        // for a statement.
        if (UnaryOperator *UO = dyn_cast<UnaryOperator>(BlameExpr)) {
          if (UO->isIncrementOp())
            BDCType = Sema::BoundsDeclarationCheck::BDC_Increment;
          else if (UO->isDecrementOp())
            BDCType = Sema::BoundsDeclarationCheck::BDC_Decrement;
        } else if (isa<BinaryOperator>(BlameExpr)) {
          // Must be an assignment or a compound assignment, because E is
          // modifying.
          BDCType = Sema::BoundsDeclarationCheck::BDC_Assignment;
        }
      }
      S.Diag(Loc, DiagId) << BDCType << A->GetRepresentative()
        << SrcRange << SrcRange;
      return Loc;
    }

    // EmitDeclaredBoundsNote emits a diagnostic message containing the
    // declared bounds for the lvalue expressions in A.
    // If the expressions in A are associated with a NamedDecl (e.g. if
    // the expressions in A are variables or member expressions), the note
    // is emitted at the declaration. Otherwise (e.g. the expressions in
    // A are pointer dereferences or array subscripts), the note is emitted
    // at the location of the last assignment expression that updated the
    // observed bounds of the expressions in A.
    void EmitDeclaredBoundsNote(const AbstractSet *A,
                                BoundsExpr *DeclaredBounds,
                                SourceLocation AssignmentLoc) {
      const NamedDecl *V = A->GetDecl();
      SourceLocation Loc = V ? V->getLocation() : AssignmentLoc;
      S.Diag(Loc, diag::note_declared_bounds)
        << DeclaredBounds << DeclaredBounds->getSourceRange();
    }

    // ValidateReturnBounds checks that the observed bounds for the return
    // value RetExpr imply the declared bounds for the enclosing function.
    void ValidateReturnBounds(ReturnStmt *RS, Expr *RetExpr,
                              BoundsExpr *RetExprBounds,
                              const EquivExprSets EquivExprs,
                              const EqualExprTy RetSameValue,
                              CheckedScopeSpecifier CSS) {
      // In an unchecked scope, if the enclosing function has a bounds-safe
      // interface, and the return value has not been implicitly converted
      // to an unchecked pointer, we skip checking the return value bounds.
      if (CSS == CheckedScopeSpecifier::CSS_Unchecked) {
        if (FunctionDeclaration->hasBoundsSafeInterface(Context)) {
          if (!IsBoundsSafeInterfaceAssignment(FunctionDeclaration->getReturnType(), RetExpr))
            return;
        }
      }

      ProofFailure Cause;
      FreeVariableListTy FreeVars;
      ProofResult Result = ProveReturnBoundsValidity(RetExpr, RetExprBounds,
                                                     EquivExprs, RetSameValue,
                                                     Cause, FreeVars);

      if (Result == ProofResult::True)
        return;

      if (RetExprBounds->isUnknown()) {
        DiagnoseUnknownReturnBounds(RS, RetExpr);
        return;
      }

      // Which diagnostic message to print?
      unsigned DiagId =
          (Result == ProofResult::False)
              ? (TestFailure(Cause, ProofFailure::HasFreeVariables)
                     ? diag::error_return_bounds_unprovable
                     : diag::error_return_bounds_invalid)
              : (CSS != CheckedScopeSpecifier::CSS_Unchecked
                     ? diag::warn_checked_scope_return_bounds_invalid
                     : diag::warn_return_bounds_invalid);

      SourceLocation Loc = RetExpr->getBeginLoc();
      S.Diag(Loc, DiagId) << FunctionDeclaration << RS->getSourceRange();
      if (Result == ProofResult::False)
        ExplainProofFailure(Loc, Cause, ProofStmtKind::ReturnStmt);
      
      if (TestFailure(Cause, ProofFailure::HasFreeVariables))
        DiagnoseFreeVariables(diag::note_free_variable_decl_or_inferred, Loc,
                              FreeVars);

      SourceLocation DeclaredLoc = FunctionDeclaration->getLocation();
      S.Diag(DeclaredLoc, diag::note_declared_return_bounds)
        << ReturnBounds << ReturnBounds->getSourceRange();
      S.Diag(Loc, diag::note_inferred_return_bounds)
        << RetExprBounds << RetExprBounds->getSourceRange();
    }

    // DiagnoseUnknownReturnBounds emits an error message at the return
    // statement RS and return expression RetExpr whose inferred bounds are
    // unknown, where the enclosing function has declared bounds ReturnBounds.
    void DiagnoseUnknownReturnBounds(ReturnStmt *RS, Expr *RetExpr) {
      SourceLocation Loc = RetExpr->getBeginLoc();
      S.Diag(Loc, diag::err_expected_bounds_for_return)
        << FunctionDeclaration << RS->getSourceRange();

      SourceLocation DeclaredLoc = FunctionDeclaration->getLocation();
      S.Diag(DeclaredLoc, diag::note_declared_return_bounds)
        << ReturnBounds << ReturnBounds->getSourceRange();
    }

    // Methods to update the checking state.

    // UpdateAfterAssignment updates the checking state after the lvalue
    // expression LValue is updated in an assignment E of the form
    // LValue = Src.
    // UpdateAfterAssignment also returns updated bounds for Src.
    //
    // TargetBounds are the bounds for the target of LValue.
    //
    // Target is an rvalue expression that is the value of LValue.
    //
    // SrcBounds are the original bounds for the source of the assignment.
    BoundsExpr *UpdateAfterAssignment(Expr *LValue,
                                      BoundsExpr *TargetBounds,
                                      Expr *E, Expr *Target,
                                      Expr *Src, BoundsExpr *SrcBounds,
                                      CheckedScopeSpecifier CSS,
                                      CheckingState &State,
                                      bool &StateUpdated) {
      if (!LValue)
        return SrcBounds;
      Lexicographic Lex(S.Context, nullptr);
      LValue = Lex.IgnoreValuePreservingOperations(S.Context, LValue);

      // We only update the checking state after assignments to a variable,
      // member expression, pointer dereference, or array subscript.
      if (!isa<DeclRefExpr>(LValue) && !isa<MemberExpr>(LValue) &&
          !ExprUtil::IsDereferenceOrSubscript(LValue)) {
        StateUpdated = false;
        return SrcBounds;
      }

      // Account for uses of LValue in the declared return bounds (if any)
      // for the enclosing function.
      CheckIfLValueIsUsedInReturnBounds(LValue, E, CSS);

      // Get the original value (if any) of LValue before the assignment,
      // and determine whether the original value uses the value of LValue.
      // OriginalValue is named OV in the Checked C spec.
      bool OriginalValueUsesLValue = false;
      Expr *OriginalValue = GetOriginalValue(LValue, Target, Src,
                              State.EquivExprs, OriginalValueUsesLValue);

      // If LValue has target bounds, get the AbstractSet that contains LValue.
      // LValueAbstractSet will be used in UpdateBoundsAfterAssignment to
      // record the observed bounds of all lvalue expressions in this set.
      // If LValue belongs to an LValueAbstractSet, then the rvalue expression
      // used to record equality between the target and source of the
      // assignment should be LValueAbstractSet's representative expression.
      // In ValidateBoundsContext, the target bounds for all expressions in
      // LValueAbstractSet are constructed using its representative expression.
      // Therefore, the equality information used to validate bounds should
      // also be based on this representative expression. Consider:
      // void f(_Array_ptr<_Nt_array_ptr<char>> arr : count(10)) {
      //   *arr = "abc";
      //   arr[0] = "xyz";
      // }
      // At the assignment to arr[0], the representative expression for the
      // LValueAbstractSet containing *arr and arr[0] is *arr. When validating
      // the bounds context after this assignment, the target bounds for arr[0]
      // are bounds(*arr, *arr + 0). Therefore, (temporary) equality should be
      // recorded between *arr and "xyz", rather than between arr[0] and "xyz".
      const AbstractSet *LValueAbstractSet = nullptr;
      Expr *EqualityTarget = Target;
      if (TargetBounds && !TargetBounds->isUnknown()) {
        LValueAbstractSet = AbstractSetMgr.GetOrCreateAbstractSet(LValue);
        Expr *Rep = LValueAbstractSet->GetRepresentative();
        EqualityTarget =
          ExprCreatorUtil::CreateImplicitCast(S, Rep, CK_LValueToRValue,
                                              Rep->getType());
      }

      BoundsExpr *ResultBounds =
        UpdateBoundsAfterAssignment(LValue, LValueAbstractSet, E, Src,
                                    SrcBounds, OriginalValue, CSS, State);
      UpdateEquivExprsAfterAssignment(LValue, OriginalValue, CSS, State);
      // We can only record temporary equality between Target and Src in
      // State.TargetSrcEquality if Src does not use the value of LValue.
      // If UpdateSameValueAfterAssignment did not remove any expressions from
      // State.SameValue, then no expressions equivalent to Src use the value
      // of LValue, so we can record temporary equality between Target and Src.
      bool AllowTempEquality =
        UpdateSameValueAfterAssignment(LValue, OriginalValue,
                                       OriginalValueUsesLValue, CSS, State);
      RecordEqualityWithTarget(LValue, EqualityTarget, Src,
                               AllowTempEquality, State);

      StateUpdated = true;
      return ResultBounds;
    }

    // CheckIfLValueIsUsedInReturnBounds computes the observed bounds for
    // the return value of the enclosing function (if any) by replacing all
    // uses of LValue within the declared return bounds with null. If the
    // declared return bounds use the value of LValue, the observed return
    // bounds will be bounds(unknown) and an error will be emitted.
    //
    // The current implementation does not use invertibility of lvalue
    // expressions. This simplifies the implementation so that the updated
    // return bounds can be computed locally at each assignment statement,
    // rather than keeping track of a return bounds expression across the
    // entire function body.
    // TODO: track an observed return bounds expression as a global property
    // of the function body so that invertibility of lvalue expressions can
    // be taken into account.
    void CheckIfLValueIsUsedInReturnBounds(Expr *LValue, Expr *E,
                                           CheckedScopeSpecifier CSS) {
      if (!ReturnBounds || ReturnBounds->isUnknown() || ReturnBounds->isAny())
        return;

      // In unchecked scopes, if the enclosing function has its bounds declared
      // via a bounds-safe interface, we do not check that expressions used in
      // the declared function bounds are not modified.
      if (CSS == CheckedScopeSpecifier::CSS_Unchecked) {
        if (FunctionDeclaration->hasBoundsSafeInterface(Context))
          return;
      }

      BoundsExpr *UpdatedReturnBounds =
        BoundsUtil::ReplaceLValueInBounds(S, ReturnBounds, LValue,
                                          nullptr, CSS);
      if (!UpdatedReturnBounds->isUnknown())
        return;

      S.Diag(LValue->getBeginLoc(), diag::error_modified_return_bounds)
          << LValue << FunctionDeclaration << E->getSourceRange();

      SourceLocation DeclaredLoc = FunctionDeclaration->getLocation();
      S.Diag(DeclaredLoc, diag::note_declared_return_bounds)
        << ReturnBounds << ReturnBounds->getSourceRange();
    }

    // UpdateBoundsAfterAssignment updates the observed bounds context after
    // an lvalue expression LValue is modified via an assignment E of the form
    // LValue = Src, based on the state before the assignment.
    // It also returns updated bounds for Src.
    //
    // LValueAbstractSet is the AbstractSet (if any) that contains LValue.
    // This AbstractSet will only be non-null if LValue has (non-unknown)
    // target bounds. If LValueAbstractSet is non-null, it is used as the key
    // in State.ObservedBounds to record the observed bounds of LValue.
    //
    // If LValue is a member expression, the observed bounds context will
    // include the updated bounds for each member expression whose target
    // bounds use the value of LValue.
    //
    // OriginalValue is the value of LValue before the assignment.
    // If OriginalValue is non-null, bounds in the observed bounds context
    // have all occurrences of LValue replaced with OriginalValue.
    // If OriginalValue is null, bounds in the observed bounds context
    // that use the value of LValue are set to bounds(unknown).
    BoundsExpr *UpdateBoundsAfterAssignment(Expr *LValue,
                                            const AbstractSet *LValueAbstractSet,
                                            Expr *E, Expr *Src,
                                            BoundsExpr *SrcBounds,
                                            Expr *OriginalValue,
                                            CheckedScopeSpecifier CSS,
                                            CheckingState &State) {
      // If LValue is associated with a member expression (i.e. the assignment
      // to LValue means that a member expression is being used to write to
      // memory), get the set of AbstractSets whose target bounds depend on
      // LValue. The observed bounds of each of these AbstractSets are recorded
      // in ObservedBounds. If they are not already present in ObservedBounds,
      // their observed bounds are initialized to their target bounds.
      MemberExpr *M = GetAssignmentTargetMemberExpr(LValue);
      if (M) {
        AbstractSetSetTy AbstractSets;
        SynthesizeMembers(M, LValue, CSS, AbstractSets);

        if (DumpSynthesizedMembers)
          DumpSynthesizedMemberAbstractSets(llvm::outs(), AbstractSets);

        for (const AbstractSet *A : AbstractSets) {
          // We only set the observed bounds of A to the target bounds of A
          // if A is not already present in ObservedBounds. If A is already
          // present in ObservedBounds, then there was an assignment in the
          // current top-level statement (or bundled block) that updated the
          // observed bounds of A. These observed bounds may or may not depend
          // on LValue. If A currently has observed bounds that do not use the
          // value of LValue, then the current assignment to LValue should
          // have no effect on the observed bounds of A.
          if (!State.ObservedBounds.count(A)) {
            State.ObservedBounds[A] =
              S.GetLValueDeclaredBounds(A->GetRepresentative(), CSS);
          }
        }
      }

      // If LValue belongs to an AbstractSet, the initial observed bounds of
      // LValue are SrcBounds. These bounds will be updated to account for
      // any uses of LValue below.
      BoundsExpr *PrevLValueBounds = nullptr;
      if (LValueAbstractSet) {
        PrevLValueBounds = State.ObservedBounds[LValueAbstractSet];
        State.ObservedBounds[LValueAbstractSet] = SrcBounds;

        // In an unchecked scope, if an expression with checked pointer type
        // is assigned to an unchecked pointer LValue (whose bounds are
        // declared via a bounds-safe interface), bounds validation should
        // validate the bounds of LValue after the current top-level statement.
        // For unchecked pointer LValues that were not assigned a checked
        // pointer expression during the current top-level statement, bounds
        // validation should skip validating the bounds of LValue.
        if (CSS == CheckedScopeSpecifier::CSS_Unchecked) {
          if (!LValue->getType()->isCheckedPointerType() &&
              !LValue->getType()->isCheckedArrayType()) {
            if (IsBoundsSafeInterfaceAssignment(LValue->getType(), Src))
              State.LValuesAssignedChecked.insert(LValueAbstractSet);
          }
        }
      }

      // Adjust ObservedBounds to account for any uses of LValue in the bounds.
      for (auto const &Pair : State.ObservedBounds) {
        const AbstractSet *A = Pair.first;
        BoundsExpr *Bounds = Pair.second;
        BoundsExpr *AdjustedBounds =
          BoundsUtil::ReplaceLValueInBounds(S, Bounds, LValue,
                                            OriginalValue, CSS);
        State.ObservedBounds[A] = AdjustedBounds;

        // If the assignment to LValue caused the observed bounds of A
        // to be bounds(unknown), add the pair to LostLValues.
        if (!Bounds->isUnknown() && AdjustedBounds->isUnknown())
          State.LostLValues[A] = std::make_pair(Bounds, LValue);

        // If E modifies the bounds of A, add the pair to BlameAssignments.
        // We can check this cheaply by comparing the pointer values of
        // AdjustedBounds and Bounds because ReplaceLValueInBounds returns
        // Bounds as AdjustedBounds if Bounds is not adjusted.
        if (AdjustedBounds != Bounds)
          State.BlameAssignments[A] = E;
      }

      // Adjust SrcBounds to account for any uses of LValue.
      BoundsExpr *AdjustedSrcBounds = nullptr;
      // If LValue belongs to an AbstractSet, then the observed bounds of
      // LValue are already SrcBounds adjusted to account for any uses of
      // LValue.
      if (LValueAbstractSet)
        AdjustedSrcBounds = State.ObservedBounds[LValueAbstractSet];
      else
        AdjustedSrcBounds =
          BoundsUtil::ReplaceLValueInBounds(S, SrcBounds, LValue,
                                            OriginalValue, CSS);

      // If the updated observed bounds of LValue are different than the
      // previous observed bounds of LValue, record that E updates the
      // observed bounds of LValue.
      // We can check this cheaply because ReplaceLValueInBounds returns
      // PrevLValueBounds as AdjustedSrcBounds if the previous observed
      // bounds of LValue were not adjusted.
      if (LValueAbstractSet && PrevLValueBounds != AdjustedSrcBounds) {
        State.BlameAssignments[LValueAbstractSet] = E;

        // If the original bounds of Src (before replacing LValue) were
        // unknown, record that the expression Src with unknown bounds was
        // assigned to LValue.
        if (SrcBounds->isUnknown())
          State.UnknownSrcBounds[LValueAbstractSet] = Src;
      }

      // If the initial source bounds were not unknown, but they are unknown
      // after replacing uses of LValue, then the assignment to LValue caused
      // the source bounds (which are the observed bounds for LValue) to be
      // unknown.
      if (LValueAbstractSet) {
        if (!SrcBounds->isUnknown() && AdjustedSrcBounds->isUnknown())
          State.LostLValues[LValueAbstractSet] =
            std::make_pair(SrcBounds, LValue);
      }

      return AdjustedSrcBounds;
    }

    // UpdateEquivExprsAfterAssignment updates the set EquivExprs of set of
    // equivalent expressions after the expression LValue is modified via
    // an assignment.
    //
    // OriginalValue is the value of LValue before the assignment.
    // If OriginalValue is non-null, each expression in EquivExprs has all
    // occurrences of LValue replaced with OriginalValue.
    // If OriginalValue is null, each expression in EquivExprs that uses the
    // value of LValue is removed from EquivExprs.
    void UpdateEquivExprsAfterAssignment(Expr *LValue, Expr *OriginalValue,
                                         CheckedScopeSpecifier CSS,
                                         CheckingState &State) {
      const EquivExprSets PrevEquivExprs = State.EquivExprs;
      State.EquivExprs.clear();
      for (auto I = PrevEquivExprs.begin(); I != PrevEquivExprs.end(); ++I) {
        ExprSetTy ExprList;
        for (auto InnerList = (*I).begin(); InnerList != (*I).end(); ++InnerList) {
          Expr *E = *InnerList;
          Expr *AdjustedE = BoundsUtil::ReplaceLValue(S, E, LValue,
                                                      OriginalValue, CSS);
          // Don't add duplicate expressions to any set in EquivExprs.
          if (AdjustedE && !EqualExprsContainsExpr(ExprList, AdjustedE))
            ExprList.push_back(AdjustedE);
        }
        if (ExprList.size() > 1)
          State.EquivExprs.push_back(ExprList);
      }
    }

    // UpdateSameValueAfterAssignment updates the set of expressions that
    // produce the same value as the source of an assignment, after an
    // assignment that modifies the expression LValue.
    //
    // OriginalValue is the value of LValue before the assignment.
    // OriginalValueUsesLValue is true if OriginalValue uses the value of
    // LValue. If this is true, then any expressions in SameValue that use
    // the value of LValue are removed from SameValue.
    //
    // UpdateSameValueAfterAssignment returns true if no expressions were
    // removed from SameValue, i.e. if no expressions in SameValue used the
    // value of LValue.
    bool UpdateSameValueAfterAssignment(Expr *LValue, Expr *OriginalValue,
                                        bool OriginalValueUsesLValue,
                                        CheckedScopeSpecifier CSS,
                                        CheckingState &State) {
      bool RemovedAnyExprs = false;
      const ExprSetTy PrevSameValue = State.SameValue;
      State.SameValue.clear();

      // Determine the expression (if any) to be used as the replacement for
      // LValue in expressions in SameValue.
      // If the original value uses the value of LValue, then any expressions
      // that use the value of LValue should be removed from SameValue.
      // For example, in the assignment i = i + 2, where the original value
      // of i is i - 2, the expression i + 2 in SameValue should be removed
      // rather than replaced with (i - 2) + 2.
      // Otherwise, RecordEqualityWithTarget would record equality between
      // (i - 2) + 2 and i, which is a tautology.
      Expr *LValueReplacement = OriginalValueUsesLValue ? nullptr : OriginalValue;

      for (auto I = PrevSameValue.begin(); I != PrevSameValue.end(); ++I) {
        Expr *E = *I;
        Expr *AdjustedE = BoundsUtil::ReplaceLValue(S, E, LValue,
                                                    LValueReplacement, CSS);
        if (!AdjustedE)
          RemovedAnyExprs = true;
        // Don't add duplicate expressions to SameValue.
        if (AdjustedE && !EqualExprsContainsExpr(State.SameValue, AdjustedE))
          State.SameValue.push_back(AdjustedE);
      }

      return !RemovedAnyExprs;
    }

    // RecordEqualityWithTarget updates the checking state to record equality
    // implied by an assignment or initializer of the form LValue = Src,
    // where Target is an rvalue expression that is the value of LValue.
    //
    // AllowTempEquality is true if it is permissible to record temporary
    // equality between Target and Src in State.TargetSrcEquality if necessary.
    // The purpose of TargetSrcEquality is to record equality between target
    // and source expressions only for checking bounds after the current
    // statement (unlike State.EquivExprs, this equality does not persist
    // across statements). However, not all target/source pairs should be added
    // to TargetSrcEquality. For example, in an assignment x = x + 1, SameValue
    // will be empty since x + 1 uses the value of x. However, x => x + 1
    // should not be added to TargetSrcEquality.
    //
    // State.SameValue is assumed to contain expressions that produce the same
    // value as Src.
    void RecordEqualityWithTarget(Expr *LValue, Expr *Target, Expr *Src,
                                  bool AllowTempEquality,
                                  CheckingState &State) {
      if (!LValue)
        return;
      LValue = LValue->IgnoreParens();

      // Certain kinds of expressions (e.g. member expressions,
      // pointer dereferences, array subscripts, etc.) are not allowed to be
      // included in EquivExprs. For these expressions, we record temporary
      // equality (if permitted by AllowTempEquality) between Target and Src
      // in TargetSrcEquality instead of in EquivExprs. If Src is allowed in
      // EquivExprs, SameValue will contain at least one expression that
      // produces the same value as Src.
      bool TargetAllowedInEquivExprs = isa<DeclRefExpr>(LValue);
      bool SrcAllowedInEquivExprs = State.SameValue.size() > 0;

      // For expressions that are allowed in EquivExprs, try to add Target
      // to an existing set F in EquivExprs that contains expressions that
      // produce the same value as Src. This prevents EquivExprs from growing
      // too large and containing redundant equality information.
      // For example, for the assignments x = 1; y = x; where the target is y,
      // SameValue = { x }, and EquivExprs contains F = { 1, x }, EquivExprs
      // should contain { 1, x, y } rather than { 1, x } and { 1, x, y }.
      if (TargetAllowedInEquivExprs && SrcAllowedInEquivExprs) {
        for (auto F = State.EquivExprs.begin(); F != State.EquivExprs.end(); ++F) {
          if (DoExprSetsIntersect(*F, State.SameValue)) {
            // Add all expressions in SameValue to F that are not already in F.
            // Any expressions in SameValue that are not already in F must be
            // at the end of SameValue. For example, F may be { 0, x, y } and
            // SameValue may be { 0, x, y, i ? x : y }.
            for (auto i = F->size(), SameValueSize = State.SameValue.size(); i < SameValueSize; ++i)
              F->push_back(State.SameValue[i]);

            // Add the target to F if necessary.
            if (!EqualExprsContainsExpr(*F, Target))
              F->push_back(Target);

            // Add the target to SameValue if necessary.
            if (!EqualExprsContainsExpr(State.SameValue, Target))
              State.SameValue.push_back(Target);
            return;
          }
        }
      }

      // If the target or the source are not allowed in EquivExprs, record
      // the equality between the target and source in TargetSrcEquality.
      // This temporary equality information will be used to validate the
      // bounds context after checking the current top-level CFG statement,
      // but does not persist across checking CFG statements.
      if (AllowTempEquality && Src &&
          (!TargetAllowedInEquivExprs || !SrcAllowedInEquivExprs)) {
        CHKCBindTemporaryExpr *Temp = GetTempBinding(Src);
        if (Temp)
          State.TargetSrcEquality[Target] = CreateTemporaryUse(Temp);
        else if (ExprUtil::CheckIsNonModifying(S, Src))
          State.TargetSrcEquality[Target] = Src;
      }

      if (!TargetAllowedInEquivExprs)
        return;

      // Avoid adding sets with duplicate expressions such as { e, e }
      // and singleton sets such as { e } to EquivExprs.
      if (!EqualExprsContainsExpr(State.SameValue, Target))
        State.SameValue.push_back(Target);
      if (State.SameValue.size() > 1)
        State.EquivExprs.push_back(State.SameValue);
    }

    // UpdateSameValue updates the set SameValue of expressions that produce
    // the same value as the value produced by the expression e.
    // e is an expression with exactly one subexpression.
    //
    // SubExprSameValue is the set of expressions that produce the same
    // value as the only subexpression of e.
    //
    // Val is an optional expression that may be contained in the updated
    // SameValue set. If Val is not provided, e is used instead.  If Val
    // and e are null, SameValue is not updated.
    void UpdateSameValue(Expr *E, const ExprSetTy SubExprSameValue,
                         ExprSetTy &SameValue, Expr *Val = nullptr) {
      Expr *SubExpr = dyn_cast<Expr>(*(E->child_begin()));
      assert(SubExpr);
      ExprEqualMapTy SubExprSameValueSets;
      SubExprSameValueSets[SubExpr] = SubExprSameValue;
      UpdateSameValue(E, SubExprSameValueSets, SameValue, Val);
    }

    // UpdateSameValue updates the set SameValue of expressions that produce
    // the same value as the value produced by the expression e.
    // e is an expression with n subexpressions, where n >= 0.
    //
    // Some kinds of expressions (e.g. assignments) have their own rules
    // for how to update the set SameValue.  UpdateSameValue is used to
    // update the set SameValue for expressions that do not have their own
    // own defined rules for updating SameValue.
    //
    // SubExprSameValueSets stores, for each subexpression S_i of e, a set
    // SameValue_i of expressions that produce the same value as S_i.
    //
    // Val is an optional expression that may be contained in the updated
    // SameValue set. If Val is not provided, e is used instead.  If Val
    // and e are null, SameValue is not updated.
    void UpdateSameValue(Expr *E, ExprEqualMapTy SubExprSameValueSets,
                         ExprSetTy &SameValue, Expr *Val = nullptr) {
      SameValue.clear();

      if (!Val) Val = E;
      if (!Val)
        return;

      // StmtExprs should not be included in SameValue.  When StmtExprs are
      // lexicographically compared, there is an assertion failure since
      // the children of StmtExprs are Stmts and not Exprs, so StmtExprs
      // should not be included in any sets that involve comparisons,
      // such as CheckingState.SameValue or CheckingState.EquivExprs.
      if (isa<StmtExpr>(Val))
        return;

      // Expressions that create new objects should not be included
      // in SameValue.
      if (CreatesNewObject(Val))
        return;

      // If Val is a call expression, SameValue does not contain Val.
      if (isa<CallExpr>(Val)) {
      }

      // If Val is a non-modifying expression, SameValue contains Val.
      else if (ExprUtil::CheckIsNonModifying(S, Val))
        SameValue.push_back(Val);

      // If Val is a modifying expression, use the SameValue_i sets of
      // expressions that produce the same value as the subexpressions of e
      // to try to construct a non-modifying expression ValPrime that produces
      // the same value as Val.
      else {
        Expr *ValPrime = nullptr;
        for (llvm::detail::DenseMapPair<Expr *, ExprSetTy> Pair : SubExprSameValueSets) {
          Expr *SubExpr_i = Pair.first;
          // For any modifying subexpression SubExpr_i of e, try to set
          // ValPrime to a nonmodifying expression from the set SameValue_i
          // of expressions that produce the same value as SubExpr_i.
          if (!ExprUtil::CheckIsNonModifying(S, SubExpr_i)) {
            ExprSetTy SameValue_i = Pair.second;
            for (auto I = SameValue_i.begin(); I != SameValue_i.end(); ++I) {
              Expr *E_i = *I;
              if (ExprUtil::CheckIsNonModifying(S, E_i)) {
                ValPrime = E_i;
                break;
              }
            }
          }
        }

        if (ValPrime)
          SameValue.push_back(ValPrime);
      }

      // If Val introduces a temporary to hold the value produced by e,
      // add the value of the temporary to SameValue.
      // TODO: checkedc-clang issue #832: adding uses of temporaries here
      // can result in false equivalence being recorded between expressions.
      if (CHKCBindTemporaryExpr *Temp = GetTempBinding(Val))
        SameValue.push_back(CreateTemporaryUse(Temp));
    }

    // Methods to get the original value of an expression.

    // GetOriginalValue returns the original value (if it exists) of the
    // expression Src with respect to the expression LValue in an assignment
    // LValue = Src.
    //
    // Target is the target expression of the assignment (that accounts for
    // any necessary casts of LValue).
    //
    // The out parameter OriginalValueUsesLValue will be set to true if the
    // original value uses the value of LValue.
    // This prevents callers from having to compute the occurrence count of
    // LValue in the original value, since GetOriginalValue computes this
    // count while trying to construct the inverse expression of the source
    // with respect to LValue.
    Expr *GetOriginalValue(Expr *LValue, Expr *Target, Expr *Src,
                           const EquivExprSets EQ,
                           bool &OriginalValueUsesLValue) {
      // Check if Src has an inverse expression with respect to LValue.
      Expr *IV = nullptr;
      if (InverseUtil::IsInvertible(S, LValue, Src))
        IV = InverseUtil::Inverse(S, LValue, Target, Src);
      if (IV) {
        // If Src has an inverse with respect to LValue, then the original
        // value (the inverse) must use the value of LValue.
        OriginalValueUsesLValue = true;
        return IV;
      }

      // If Src does not have an inverse with respect to LValue, then we
      // search the set EQ for a variable equivalent to LValue. In this
      // case, LValue must be a variable V.
      DeclRefExpr *V = VariableUtil::GetLValueVariable(S, LValue);
      if (!V) {
        OriginalValueUsesLValue = false;
        return nullptr;
      }

      // If Src does not have an inverse with respect to v, then the original
      // value is either some variable w != v in EQ, or it is null. In either
      // case, the original value cannot use the value of v.
      OriginalValueUsesLValue = false;
      
      // Check EQ for a variable w != v that produces the same value as v.
      Expr *ValuePreservingV = nullptr;
      ExprSetTy F = GetEqualExprSetContainingExpr(Target, EQ, ValuePreservingV);
      for (auto I = F.begin(); I != F.end(); ++I) {
        // Account for value-preserving operations on w when searching for
        // a variable w in F. For example, if F contains (T)LValueToRValue(w),
        // where w is a variable != v and (T) is a value-preserving cast, the
        // original value should be (T)LValueToRValue(w).
        Lexicographic Lex(S.Context, nullptr);
        Expr *E = Lex.IgnoreValuePreservingOperations(S.Context, *I);
        DeclRefExpr *W = VariableUtil::GetRValueVariable(S, E);
        if (W != nullptr && !ExprUtil::EqualValue(S.Context, V, W, nullptr)) {
          // Expression equality in EquivExprs does not account for types, so
          // expressions in the same set in EquivExprs may not have the same
          // type. The original value of Src with respect to v must have a type
          // compatible with the type of v (accounting for value-preserving
          // operations on v). For example, if F contains (T1)LValueToRValue(v)
          // and LValueToRValue(w), where v and w have type T2, (T1) is a value-
          // preserving cast, and T1 and T2 are not compatible types, the
          // original value should be LValueToRValue(w).
          if (S.Context.typesAreCompatible(ValuePreservingV->getType(),
                                            (*I)->getType()))
            return *I;
        }
      }

      return nullptr;
    }

    // GetIncomingBlockState returns the checking state that is true at the
    // beginning of the block by taking the intersection of the observed
    // bounds contexts and sets of equivalent expressions that were true
    // after each of the block's predecessors.
    //
    // Taking the intersection of the observed bounds contexts of the block's
    // predecessors ensures that, before checking a statement S in the block,
    // the block's observed bounds context contains only variables with bounds
    // that are in scope at S.  At the beginning of the block, each variable in
    // scope is mapped to its normalized declared bounds.
    CheckingState GetIncomingBlockState(const CFGBlock *Block,
                                        llvm::DenseMap<unsigned int, CheckingState> BlockStates) {
      CheckingState BlockState;
      bool IntersectionEmpty = true;
      for (const CFGBlock *PredBlock : Block->preds()) {
        // Prevent null or non-traversed (e.g. unreachable) blocks from causing
        // the incoming bounds context and EquivExprs set for a block to be empty.
        if (!PredBlock)
          continue;
        if (BlockStates.find(PredBlock->getBlockID()) == BlockStates.end())
          continue;
        CheckingState PredState = BlockStates[PredBlock->getBlockID()];
        if (IntersectionEmpty) {
          BlockState.ObservedBounds = PredState.ObservedBounds;
          BlockState.EquivExprs = PredState.EquivExprs;
          IntersectionEmpty = false;
        }
        else {
          BlockState.ObservedBounds =
            IntersectBoundsContexts(PredState.ObservedBounds,
                                    BlockState.ObservedBounds);
          BlockState.EquivExprs = IntersectEquivExprs(PredState.EquivExprs,
                                                      BlockState.EquivExprs);
        }
      }
      return BlockState;
    }

    // ContextDifference returns a bounds context containing all AbstractSets
    // A in Context1 where Context1[A] != Context2[A].
    BoundsContextTy ContextDifference(BoundsContextTy Context1,
                                      BoundsContextTy Context2) {
      BoundsContextTy Difference;
      for (const auto &Pair : Context1) {
        const AbstractSet *A = Pair.first;
        BoundsExpr *B = Pair.second;
        auto It = Context2.find(A);
        if (It == Context2.end() || !ExprUtil::EqualValue(Context, B, It->second, nullptr)) {
          Difference[A] = B;
        }
      }
      return Difference;
    }

    // EqualContexts returns true if Context1 and Context2 contain the same
    // sets of AbstractSets as keys, and for each key AbstractSet A,
    // Context1[A] == Context2[A].
    bool EqualContexts(BoundsContextTy Context1, BoundsContextTy Context2) {
      if (Context1.size() != Context2.size())
        return false;

      for (const auto &Pair : Context1) {
        auto It = Context2.find(Pair.first);
        if (It == Context2.end())
          return false;
        if (!ExprUtil::EqualValue(Context, Pair.second, It->second, nullptr))
          return false;
      }

      return true;
    }

    // IntersectBoundsContexts returns a bounds context resulting from taking
    // the intersection of the contexts Context1 and Context2.
    //
    // For each AbstractSet A that is in both Context1 and Context2, the
    // intersected context maps A to its normalized declared bounds.
    // Context1 or Context2 may map A to widened bounds, but those bounds
    // should not persist across CFG blocks.  The observed bounds for each
    // in-scope AbstractSet should be reset to its normalized declared bounds
    // at the beginning of a block, before widening the bounds in the block.
    BoundsContextTy IntersectBoundsContexts(BoundsContextTy Context1,
                                            BoundsContextTy Context2) {
      BoundsContextTy IntersectedContext;
      for (auto const &Pair : Context1) {
        const AbstractSet *A = Pair.first;
        if (!Pair.second || !Context2.count(A))
          continue;
        if (BoundsExpr *B = S.GetLValueDeclaredBounds(A->GetRepresentative()))
          IntersectedContext[A] = B;
      }
      return IntersectedContext;
    }

    // IntersectEquivExprs returns the intersection of two sets of sets of
    // equivalent expressions, where each set in EQ1 is intersected with
    // each set in EQ2 to produce an element of the result.
    EquivExprSets IntersectEquivExprs(const EquivExprSets EQ1,
                                      const EquivExprSets EQ2) {
      EquivExprSets IntersectedEQ;
      for (auto I1 = EQ1.begin(); I1 != EQ1.end(); ++I1) {
        ExprSetTy Set1 = *I1;
        for (auto I2 = EQ2.begin(); I2 != EQ2.end(); ++I2) {
          ExprSetTy Set2 = *I2;
          ExprSetTy IntersectedExprSet = IntersectExprSets(Set1, Set2);
          if (IntersectedExprSet.size() > 1)
            IntersectedEQ.push_back(IntersectedExprSet);
        }
      }
      return IntersectedEQ;
    }

    // IntersectExprSets returns the intersection of two sets of expressions.
    ExprSetTy IntersectExprSets(const ExprSetTy Set1, const ExprSetTy Set2) {
      ExprSetTy IntersectedSet;
      for (auto I = Set1.begin(); I != Set1.end(); ++I) {
        Expr *E1 = *I;
        if (EqualExprsContainsExpr(Set2, E1))
          IntersectedSet.push_back(E1);
      }
      return IntersectedSet;
    }

    // IntersectDeclSets returns the intersection of Set1 and Set2.
    DeclSetTy IntersectDeclSets(DeclSetTy Set1, DeclSetTy Set2) {
      DeclSetTy Intersection;
      for (auto I = Set1.begin(), E = Set1.end(); I != E; ++I) {
        const VarDecl *V = *I;
        if (Set2.find(V) != Set2.end()) {
          Intersection.insert(V);
        }
      }
      return Intersection;
    }

    // GetEqualExprSetContainingExpr returns the set F in EQ that contains e
    // if such a set F exists, or an empty set otherwise.
    //
    // If there is a set F in EQ that contains an expression e1 such that
    // e1 is canonically equivalent to e, ValuePreservingE is set to e1.
    // e1 may include value-preserving operations.  For example, if a set F
    // in EQ contains (T)e, where (T) is a value-preserving cast,
    // ValuePreservingE will be set to (T)e.
    ExprSetTy GetEqualExprSetContainingExpr(Expr *E, EquivExprSets EQ,
                                            Expr *&ValuePreservingE) {
      ValuePreservingE = nullptr;
      for (auto OuterList = EQ.begin(); OuterList != EQ.end(); ++OuterList) {
        ExprSetTy F = *OuterList;
        for (auto InnerList = F.begin(); InnerList != F.end(); ++InnerList) {
          Expr *E1 = *InnerList;
          if (ExprUtil::EqualValue(S.Context, E, E1, nullptr)) {
            ValuePreservingE = E1;
            return F;
          }
        }
      }
      return { };
    }

    // If e appears in a set F in EQ, GetEqualExprSetContainingExpr
    // returns F.  Otherwise, it returns an empty set.
    ExprSetTy GetEqualExprSetContainingExpr(Expr *E, EquivExprSets EQ) {
      for (auto OuterList = EQ.begin(); OuterList != EQ.end(); ++OuterList) {
        ExprSetTy F = *OuterList;
        if (::EqualExprsContainsExpr(S, F, E, nullptr))
          return F;
      }
      return { };
    }

    // IsEqualExprsSubset returns true if Exprs1 is a subset of Exprs2.
    bool IsEqualExprsSubset(const ExprSetTy Exprs1, const ExprSetTy Exprs2) {
      for (auto I = Exprs1.begin(); I != Exprs1.end(); ++I) {
        Expr *E = *I;
        if (!EqualExprsContainsExpr(Exprs2, E))
          return false;
      }
      return true;
    }

    // DoExprSetsIntersect returns true if the intersection of Exprs1 and
    // Exprs2 is nonempty.
    bool DoExprSetsIntersect(const ExprSetTy Exprs1, const ExprSetTy Exprs2) {
      for (auto I = Exprs1.begin(); I != Exprs1.end(); ++I) {
        Expr *E = *I;
        if (EqualExprsContainsExpr(Exprs2, E))
          return true;
      }
      return false;
    }

    // EqualExprsContainsExpr returns true if the set Exprs contains E.
    bool EqualExprsContainsExpr(const ExprSetTy Exprs, Expr *E) {
      return ::EqualExprsContainsExpr(S, Exprs, E, nullptr);
    }

    // CreatesNewObject returns true if the expression e creates a new object.
    // Expressions that create new objects should not be added to the
    // EquivExprs or SameValue sets of equivalent expressions in the checking
    // state.
    bool CreatesNewObject(Expr *E) {
      switch (E->getStmtClass()) {
        case Expr::InitListExprClass:
        case Expr::ImplicitValueInitExprClass:
        case Expr::CompoundLiteralExprClass:
        case Expr::ExtVectorElementExprClass:
        case Expr::ExprWithCleanupsClass:
        case Expr::BlockExprClass:
        case Expr::SourceLocExprClass:
        case Expr::PackExprClass:
        case Expr::FixedPointLiteralClass:
        case Expr::StringLiteralClass:
          return true;
        default: {
          for (auto I = E->child_begin(); I != E->child_end(); ++I) {
            if (Expr *SubExpr = dyn_cast<Expr>(*I)) {
              if (CreatesNewObject(SubExpr))
                return true;
            }
          }
          return false;
        }
      }
    }

    // GetAssignmentTargetMemberExpr returns the member expression, if any,
    // that is associated with the expression e, where e is the target of
    // an assignment. e is associated with a member expression m if:
    // 1. e is of the form m, or:
    // 2. e is of the form (T)e1 and e1 is associated with m (note that this
    //    includes implicit casts such as LValueToRValue(e1)), or:
    // 3. e is of the form *e1 and e1 is associated with m, or:
    // 4. e is of the form base[index] or index[base] and base is associated
    //    with m, or:
    // 5. e is of the form p OP e1 or e1 OP p and p is associated with m,
    //    where OP is any binary operator and p has pointer type.
    MemberExpr *GetAssignmentTargetMemberExpr(Expr *E) {
      if (!E)
        return nullptr;
      Lexicographic Lex(S.Context, nullptr);
      E = Lex.IgnoreValuePreservingOperations(S.Context, E);
      if (MemberExpr *M = dyn_cast<MemberExpr>(E))
        return M;
      else if (CastExpr *CE = dyn_cast<CastExpr>(E))
        return GetAssignmentTargetMemberExpr(CE->getSubExpr());
      else if (UnaryOperator *UO = dyn_cast<UnaryOperator>(E)) {
        if (UO->getOpcode() == UnaryOperatorKind::UO_Deref)
          return GetAssignmentTargetMemberExpr(UO->getSubExpr());
      } else if (ArraySubscriptExpr *AS = dyn_cast<ArraySubscriptExpr>(E))
        return GetAssignmentTargetMemberExpr(AS->getBase());
      else if (BinaryOperator *BO = dyn_cast<BinaryOperator>(E)) {
        Expr *LHS = BO->getLHS();
        Expr *RHS = BO->getRHS();
        if (LHS->getType()->isPointerType())
          return GetAssignmentTargetMemberExpr(LHS);
        else if (RHS->getType()->isPointerType())
          return GetAssignmentTargetMemberExpr(RHS);
      }
      return nullptr;
    }

    // SynthesizeMembers modifies the set AbstractSets to include AbstractSets
    // for member expressions whose target bounds use the value of the lvalue
    // expression LValue that is being modified via an assignment.
    void SynthesizeMembers(Expr *E, Expr *LValue, CheckedScopeSpecifier CSS,
                           AbstractSetSetTy &AbstractSets) {
      if (!E)
        return;
      Lexicographic Lex(S.Context, nullptr);
      E = Lex.IgnoreValuePreservingOperations(S.Context, E->IgnoreParens());

      // Recursively synthesize AbstractSets for certain subexpressions of E.
      switch (E->getStmtClass()) {
        case Expr::MemberExprClass:
          break;
        case Expr::ImplicitCastExprClass:
        case Expr::CStyleCastExprClass: {
          CastExpr *CE = cast<CastExpr>(E);
          SynthesizeMembers(CE->getSubExpr(), LValue, CSS, AbstractSets);
          return;
        }
        case Expr::UnaryOperatorClass: {
          UnaryOperator *UO = cast<UnaryOperator>(E);
          SynthesizeMembers(UO->getSubExpr(), LValue, CSS, AbstractSets);
          return;
        }
        case Expr::ArraySubscriptExprClass: {
          ArraySubscriptExpr *AE = cast<ArraySubscriptExpr>(E);
          SynthesizeMembers(AE->getBase(), LValue, CSS, AbstractSets);
          return;
        }
        case Expr::BinaryOperatorClass: {
          BinaryOperator *BO = cast<BinaryOperator>(E);
          Expr *LHS = BO->getLHS();
          Expr *RHS = BO->getRHS();
          if (LHS->getType()->isPointerType())
            SynthesizeMembers(LHS, LValue, CSS, AbstractSets);
          if (RHS->getType()->isPointerType())
            SynthesizeMembers(RHS, LValue, CSS, AbstractSets);
          return;
        }
        default:
          return;
      }

      MemberExpr *ME = dyn_cast<MemberExpr>(E);
      if (!ME)
        return;

      // Synthesize AbstractSets for a member expression `Base->Field`
      // or `Base.Field`.
      Expr *Base = ME->getBase();
      FieldDecl *Field = dyn_cast<FieldDecl>(ME->getMemberDecl());
      if (!Field)
        return;

      // For each sibling field F of Field in whose declared bounds Field
      // appears, check whether the concrete, expanded target bounds of
      // `Base->F` or `Base.F` use the value of M.
      auto I = BoundsSiblingFields.find(Field);
      if (I != BoundsSiblingFields.end()) {
        auto SiblingFields = I->second;
        for (const FieldDecl *F : SiblingFields) {
          if (!F->hasBoundsExpr())
            continue;
          MemberExpr *BaseF =
            ExprCreatorUtil::CreateMemberExpr(S, Base, F, ME->isArrow());
          ++S.CheckedCStats.NumSynthesizedMemberExprs;
          BoundsExpr *Bounds = MemberExprTargetBounds(BaseF, CSS);
          if (ExprUtil::FindLValue(S, LValue, Bounds)) {
            const AbstractSet *A = AbstractSetMgr.GetOrCreateAbstractSet(BaseF);
            AbstractSets.insert(A);
            ++S.CheckedCStats.NumSynthesizedMemberAbstractSets;
          }
        }
      }

      SynthesizeMembers(Base, LValue, CSS, AbstractSets);
    }

    // This describes an empty range. We use this where semantically the value
    // can never point to any range of memory, and statically understanding this
    // is useful.
    // We use this for example for function pointers or float-typed expressions.
    //
    // This is better than represenging the empty range as bounds(e, e), or even
    // bounds(e1, e2), because in these cases we need to do further analysis to
    // understand that the upper and lower bounds of the range are equal.
    BoundsExpr *CreateBoundsEmpty() {
      return BoundsUtil::CreateBoundsUnknown(S);
    }

    // This describes the bounds of null, which is compatible with every
    // other bounds annotation.
    BoundsExpr *CreateBoundsAny() {
      return new (Context) NullaryBoundsExpr(BoundsExpr::Kind::Any,
                                             SourceLocation(),
                                             SourceLocation());
    }

    // Currently our inference algorithm has some limitations,
    // where we cannot express bounds for things that will have bounds
    //
    // This is for the case where we want to allow these today,
    // but we need to re-visit these places and disallow some instances
    // when we can accurately calculate these bounds.
    BoundsExpr *CreateBoundsAllowedButNotComputed() {
      return CreateBoundsAny();
    }
    // This is for the opposite case, where we want to return bounds(unknown)
    // at the moment, but we want to re-visit these parts of inference
    // and in some cases compute bounds.
    BoundsExpr *CreateBoundsNotAllowedYet() {
      return BoundsUtil::CreateBoundsUnknown(S);
    }

    BoundsExpr *CreateBoundsForArrayType(QualType T) {
      return BoundsUtil::CreateBoundsForArrayType(S, T, IncludeNullTerminator);
    }

    BoundsExpr *ArrayExprBounds(Expr *E) {
      return BoundsUtil::ArrayExprBounds(S, E, IncludeNullTerminator);
    }

    BoundsExpr *CreateSingleElementBounds(Expr *LowerBounds) {
      assert(LowerBounds->isRValue());
      return BoundsUtil::ExpandToRange(S, LowerBounds,
                                       Context.getPrebuiltCountOne());
    }

    Expr *CreateTemporaryUse(CHKCBindTemporaryExpr *Binding) {
      return new (Context) BoundsValueExpr(SourceLocation(), Binding);
    }

    Expr *CreateAddressOfOperator(Expr *E) {
      QualType Ty = Context.getPointerType(E->getType(), CheckedPointerKind::Array);
      return UnaryOperator::Create(Context, E, UnaryOperatorKind::UO_AddrOf, Ty,
                                   ExprValueKind::VK_RValue,
                                   ExprObjectKind::OK_Ordinary,
                                   SourceLocation(), false,
                                   FPOptionsOverride());
    }

    // Infer bounds for string literals.
    BoundsExpr *InferBoundsForStringLiteral(Expr *E, StringLiteral *SL,
                                            CHKCBindTemporaryExpr *Binding) {
      // Use the number of characters in the string (excluding the null
      // terminator) to calcaulte size.  Don't use the array type of the
      // literal.  In unchecked scopes, the array type is unchecked and its
      // size includes the null terminator.  It converts to an ArrayPtr that
      // could be used to overwrite the null terminator.  We need to prevent
      // this because literal strings may be shared and writeable, depending on
      // the C implementation.
      auto *Size = ExprCreatorUtil::CreateIntegerLiteral(Context,
                     llvm::APInt(64, SL->getLength()));
      auto *CBE =
        new (Context) CountBoundsExpr(BoundsExpr::Kind::ElementCount,
                                      Size, SourceLocation(),
                                      SourceLocation());

      auto PtrType = Context.getDecayedType(E->getType());

      // For a string literal expression, we always bind the result of the
      // expression to a temporary. We then use this temporary in the bounds
      // expression for the string literal expression. Otherwise, a runtime
      // bounds check based on accessing the predefined expression could be
      // incorrect: the base value could be different for the lower and upper
      // bounds.
      auto *ArrLValue = CreateTemporaryUse(Binding);
      auto *Base = ExprCreatorUtil::CreateImplicitCast(S, ArrLValue,
                                      CastKind::CK_ArrayToPointerDecay,
                                      PtrType);
      return BoundsUtil::ExpandToRange(S, Base, CBE);
    }

    // Infer the bounds for a member expression.
    // A member expression is an lvalue.
    //
    // MemberExprBounds should only be called on an
    // expression that has not had any side effects performed
    // on it, since PruneTemporaryBindings expects no bounds
    // expressions to have been set.
    BoundsExpr *MemberExprBounds(MemberExpr *ME, CheckedScopeSpecifier CSS) {
      FieldDecl *FD = dyn_cast<FieldDecl>(ME->getMemberDecl());
      if (!FD)
        return BoundsUtil::CreateBoundsInferenceError(S);

      if (ME->getType()->isArrayType()) {
        // Declared bounds override the bounds based on the array type.
        BoundsExpr *B = FD->getBoundsExpr();
        if (B) {
          B = S.MakeMemberBoundsConcrete(ME->getBase(), ME->isArrow(), B);
          if (!B) {
            assert(ME->getBase()->isRValue());
            // This can happen if the base expression is an rvalue expression.
            // It could be a function call that returns a struct, for example.
            return CreateBoundsNotAllowedYet();
          }
          if (B->isElementCount() || B->isByteCount()) {
            Expr *Base = ExprCreatorUtil::CreateImplicitCast(S, ME,
                                            CastKind::CK_ArrayToPointerDecay,
                                            Context.getDecayedType(ME->getType()));
            return cast<BoundsExpr>(PruneTemporaryBindings(S,
                                      BoundsUtil::ExpandToRange(S, Base, B),
                                      CSS));
          } else
            return cast<BoundsExpr>(PruneTemporaryBindings(S, B, CSS));
        }

        // If B is an interop type annotation, the type must be identical
        // to the declared type, modulo checkedness.  So it is OK to
        // compute the array bounds based on the original type.
        return cast<BoundsExpr>(PruneTemporaryBindings(S, ArrayExprBounds(ME), CSS));
      }

      // It is an error for a member to have function type.
      if (ME->getType()->isFunctionType())
        return BoundsUtil::CreateBoundsInferenceError(S);

      // If E is an L-value, the ME must be an L-value too.
      if (ME->isRValue()) {
        llvm_unreachable("unexpected MemberExpr r-value");
        return BoundsUtil::CreateBoundsInferenceError(S);
      }

      Expr *AddrOf = CreateAddressOfOperator(ME);
      BoundsExpr* Bounds = CreateSingleElementBounds(AddrOf);
      return cast<BoundsExpr>(PruneTemporaryBindings(S, Bounds, CSS));
    }

    // Infer bounds for the target of a variable.
    // A variable is an lvalue.
    BoundsExpr *DeclRefExprTargetBounds(DeclRefExpr *DRE,
                                        CheckedScopeSpecifier CSS) {
      VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl());
      BoundsExpr *B = nullptr;
      InteropTypeExpr *IT = nullptr;
      if (VD) {
        B = VD->getBoundsExpr();
        IT = VD->getInteropTypeExpr();
      }

      // Variables with array type do not have target bounds.
      if (DRE->getType()->isArrayType())
        return BoundsUtil::CreateBoundsAlwaysUnknown(S);

      // Infer target bounds for variables without array type.

      bool IsParam = isa<ParmVarDecl>(DRE->getDecl());
      if (DRE->getType()->isCheckedPointerPtrType())
        return CreateTypeBasedBounds(DRE, DRE->getType(), IsParam, false);

      if (!VD)
        return BoundsUtil::CreateBoundsInferenceError(S);

      if (!B && IT)
        return CreateTypeBasedBounds(DRE, IT->getType(), IsParam, true);

      if (!B || B->isUnknown())
        return BoundsUtil::CreateBoundsAlwaysUnknown(S);

      Expr *Base = ExprCreatorUtil::CreateImplicitCast(S, DRE,
                                      CastKind::CK_LValueToRValue,
                                      DRE->getType());
      return BoundsUtil::ExpandToRange(S, Base, B);
    }

    // Infer bounds for the target of a unary operator.
    // A unary operator may be an lvalue.
    BoundsExpr *UnaryOperatorTargetBounds(UnaryOperator *UO,
                                          CheckedScopeSpecifier CSS) {
      if (UO->getOpcode() == UnaryOperatorKind::UO_Deref) {
        // Currently, we don't know the target bounds of a pointer stored in a
        // pointer dereference, unless it is a _Ptr type or an _Nt_array_ptr.
        if (UO->getType()->isCheckedPointerPtrType() ||
            UO->getType()->isCheckedPointerNtArrayType())
          return CreateTypeBasedBounds(UO, UO->getType(),
                                                  false, false);
        else
          return BoundsUtil::CreateBoundsUnknown(S);
      }

      // Unary operators other than pointer dereferences do not have lvalue
      // target bounds.
      return BoundsUtil::CreateBoundsInferenceError(S);
    }

    // Infer target bounds for an array subscript expression.
    // An array subscript is an lvalue.
    BoundsExpr *ArraySubscriptExprTargetBounds(ArraySubscriptExpr *ASE,
                                               CheckedScopeSpecifier CSS) {
      // Currently, we don't know the target bounds of a pointer returned by a
      // subscripting operation, unless it is a _Ptr type or an _Nt_array_ptr.
      if (ASE->getType()->isCheckedPointerPtrType() ||
          ASE->getType()->isCheckedPointerNtArrayType())
        return CreateTypeBasedBounds(ASE, ASE->getType(), false, false);
      else
        return BoundsUtil::CreateBoundsAlwaysUnknown(S);
    }

    // Infer the bounds for the target of a member expression.
    // A member expression is an lvalue.
    //
    // MemberExprTargetBounds should only be called on an
    // expression that has not had any side effects performed
    // on it, since PruneTemporaryBindings expects no bounds
    // expressions to have been set.
    BoundsExpr *MemberExprTargetBounds(MemberExpr *ME,
                                       CheckedScopeSpecifier CSS) {
      FieldDecl *F = dyn_cast<FieldDecl>(ME->getMemberDecl());
      if (!F)
        return BoundsUtil::CreateBoundsInferenceError(S);

      BoundsExpr *B = F->getBoundsExpr();
      InteropTypeExpr *IT = F->getInteropTypeExpr();
      if (B && B->isUnknown())
        return BoundsUtil::CreateBoundsAlwaysUnknown(S);

      Expr *MemberBaseExpr = ME->getBase();
      if (!B && IT) {
        B = CreateTypeBasedBounds(ME, IT->getType(),
                                      /*IsParam=*/false,
                                      /*IsInteropTypeAnnotation=*/true);
        return cast<BoundsExpr>(PruneTemporaryBindings(S, B, CSS));
      }
            
      if (!B)
        return BoundsUtil::CreateBoundsAlwaysUnknown(S);

      B = S.MakeMemberBoundsConcrete(MemberBaseExpr, ME->isArrow(), B);
      if (!B) {
        // This can happen when MemberBaseExpr is an rvalue expression.  An example
        // of this a function call that returns a struct.  MakeMemberBoundsConcrete
        // can't handle this yet.
        return CreateBoundsNotAllowedYet();
      }

      if (B->isElementCount() || B->isByteCount()) {
          Expr *MemberRValue;
        if (ME->isLValue())
          MemberRValue = ExprCreatorUtil::CreateImplicitCast(S, ME,
                                            CastKind::CK_LValueToRValue,
                                            ME->getType());
        else
          MemberRValue = ME;
        B = BoundsUtil::ExpandToRange(S, MemberRValue, B);
      }

      return cast<BoundsExpr>(PruneTemporaryBindings(S, B, CSS));
    }

    // Infer bounds for the target of a cast expression.
    // A cast expression may be an lvalue.
    BoundsExpr *LValueCastTargetBounds(CastExpr *CE, CheckedScopeSpecifier CSS) {
      // An LValueBitCast adjusts the type of the lvalue.  The bounds are not
      // changed, except that their relative alignment may change (the bounds 
      // may only cover a partial object).  TODO: When we add relative
      // alignment support to the compiler, adjust the relative alignment.
      if (CE->getCastKind() == CastKind::CK_LValueBitCast)
        return GetLValueTargetBounds(CE->getSubExpr(), CSS);

      // Cast kinds other than LValueBitCast do not have lvalue target bounds.
      return BoundsUtil::CreateBoundsAlwaysUnknown(S);
    }

    // Infer bounds for the target of a temporary binding expression.
    // A temporary binding may be an lvalue.
    BoundsExpr *LValueTempBindingTargetBounds(CHKCBindTemporaryExpr *Temp,
                                              CheckedScopeSpecifier CSS) {
      return BoundsUtil::CreateBoundsAlwaysUnknown(S);
    }

    // Given a Ptr type or a bounds-safe interface type, create the bounds
    // implied by the type.  If E is non-null, place the bounds in standard form
    // (do not use count or byte_count because their meaning changes
    //  when propagated to parent expressions).
    BoundsExpr *CreateTypeBasedBounds(Expr *E, QualType Ty, bool IsParam,
                                      bool IsBoundsSafeInterface) {
      BoundsExpr *BE = nullptr;
      // If the target value v is a Ptr type, it has bounds(v, v + 1), unless
      // it is a function pointer type, in which case it has no required
      // bounds.

      if (Ty->isCheckedPointerPtrType()) {
        if (Ty->isFunctionPointerType())
          BE = CreateBoundsEmpty();
        else if (Ty->isVoidPointerType())
          BE = Context.getPrebuiltByteCountOne();
        else
          BE = Context.getPrebuiltCountOne();
      } else if (Ty->isCheckedArrayType()) {
        assert(IsParam && IsBoundsSafeInterface && "unexpected checked array type");
        BE = CreateBoundsForArrayType(Ty);
      } else if (Ty->isCheckedPointerNtArrayType()) {
        BE = Context.getPrebuiltCountZero();
      }
   
      if (!BE)
        return CreateBoundsEmpty();

      if (!E)
        return BE;

      Expr *Base = E;
      if (Base->isLValue())
        Base = ExprCreatorUtil::CreateImplicitCast(S, Base,
                                  CastKind::CK_LValueToRValue,
                                  E->getType());

      // If type is a bounds-safe interface type, adjust the type of base to the
      // bounds-safe interface type.
      if (IsBoundsSafeInterface) {
        // Compute the target type.  We could receive an array type for a parameter
        // with a bounds-safe interface.
        QualType TargetTy = Ty;
        if (TargetTy->isArrayType()) {
          assert(IsParam);
          TargetTy = Context.getArrayDecayedType(Ty);
        };

        if (TargetTy != E->getType())
          Base = ExprCreatorUtil::CreateExplicitCast(S, TargetTy, CK_BitCast,
                                                     Base, true);
      } else
        assert(Ty == E->getType());

      return BoundsUtil::ExpandToRange(S, Base, BE);
    }

    // Compute the bounds of a cast operation that produces an rvalue.
    BoundsExpr *RValueCastBounds(CastExpr *E,
                                 BoundsExpr *TargetBounds,
                                 BoundsExpr *LValueBounds,
                                 BoundsExpr *RValueBounds,
                                 CheckingState State) {
      switch (E->getCastKind()) {
        case CastKind::CK_BitCast:
        case CastKind::CK_NoOp:
        case CastKind::CK_NullToPointer:
        // Truncation or widening of a value does not affect its bounds.
        case CastKind::CK_IntegralToPointer:
        case CastKind::CK_PointerToIntegral:
        case CastKind::CK_IntegralCast:
        case CastKind::CK_IntegralToBoolean:
        case CastKind::CK_BooleanToSignedIntegral:
          return RValueBounds;
        case CastKind::CK_LValueToRValue: {
          // For an LValueToRValue cast of an lvalue expression LValue, if
          // LValue has observed bounds, the rvalue bounds of the value of
          // LValue should be the observed bounds. This also accounts for
          // variables that have widened bounds, since widened bounds for
          // variables are recorded in State.ObservedBounds.
          if (BoundsExpr *ObservedBounds = GetLValueObservedBounds(E->getSubExpr(), State))
            return ObservedBounds;
          // If LValue has no recorded observed bounds, the rvalue bounds of
          // LValueToRValue(LValue) default to the target bounds of LValue.
          return TargetBounds;
        }
        case CastKind::CK_ArrayToPointerDecay: {
          // For an array to pointer cast of an lvalue expression LValue, if
          // LValue has observed bounds, the rvalue bounds of the value of
          // LValue should be the observed bounds. This also accounts for
          // variables that have widened bounds, since widened bounds for
          // variables are recorded in State.ObservedBounds.
          if (BoundsExpr *ObservedBounds = GetLValueObservedBounds(E->getSubExpr(), State))
            return ObservedBounds;
          // If LValue has no recorded observed bounds, the rvalue bounds of
          // ArrayToPointerDecay(LValue) default to the lvalue bounds of
          // LValue.
          return LValueBounds;
        }
        case CastKind::CK_DynamicPtrBounds:
        case CastKind::CK_AssumePtrBounds:
          llvm_unreachable("unexpected rvalue bounds cast");
        default:
          return BoundsUtil::CreateBoundsAlwaysUnknown(S);
      }
    }

    // If LValue belongs to an AbstractSet that has observed bounds recorded
    // in State.ObservedBounds, GetLValueObservedBounds returns the observed
    // bounds for the rvalue expression produced by LValue as recorded in
    // State.ObservedBounds. Otherwise, GetLValueObservedBounds returns null.
    BoundsExpr *GetLValueObservedBounds(Expr *LValue, CheckingState State) {
      Lexicographic Lex(S.Context, nullptr);
      LValue = Lex.IgnoreValuePreservingOperations(S.Context, LValue);

      // Only variables, member expressions, pointer dereferences, and
      // array subscripts have bounds recorded in State.ObservedBounds.
      if (!isa<DeclRefExpr>(LValue) && !isa<MemberExpr>(LValue) &&
          !ExprUtil::IsDereferenceOrSubscript(LValue))
        return nullptr;

      const AbstractSet *A = AbstractSetMgr.GetOrCreateAbstractSet(LValue);
      auto It = State.ObservedBounds.find(A);
      if (It != State.ObservedBounds.end())
        return It->second;

      return nullptr;
    }

    // Compute the bounds of a call expression.  Call expressions always
    // produce rvalues.
    //
    // If ResultName is non-null, it is a temporary variable where the result
    // of the call expression is stored immediately upon return from the call.
    BoundsExpr *CallExprBounds(const CallExpr *CE,
                               CHKCBindTemporaryExpr *ResultName,
                               CheckedScopeSpecifier CSS) {
      BoundsExpr *ReturnBounds = nullptr;
      if (CE->getType()->isCheckedPointerPtrType()) {
        if (CE->getType()->isVoidPointerType())
          ReturnBounds = Context.getPrebuiltByteCountOne();
        else
          ReturnBounds = Context.getPrebuiltCountOne();
      }
      else {
        // Get the function prototype, where the abstract function return
        // bounds are kept.  The callee (if it exists) 
        // is always a function pointer.
        const PointerType *PtrTy =
          CE->getCallee()->getType()->getAs<PointerType>();
        if (PtrTy == nullptr)
          return BoundsUtil::CreateBoundsInferenceError(S);
        const FunctionProtoType *CalleeTy =
          PtrTy->getPointeeType()->getAs<FunctionProtoType>();
        if (!CalleeTy)
          // K&R functions have no prototype, and we cannot perform
          // inference on them, so we return bounds(unknown) for their results.
          return BoundsUtil::CreateBoundsAlwaysUnknown(S);

        BoundsAnnotations FunReturnAnnots = CalleeTy->getReturnAnnots();
        BoundsExpr *FunBounds = FunReturnAnnots.getBoundsExpr();
        InteropTypeExpr *IType =FunReturnAnnots.getInteropTypeExpr();
        // If there is no return bounds and there is an interop type
        // annotation, use the bounds implied by the interop type
        // annotation.
        if (!FunBounds && IType)
          FunBounds = CreateTypeBasedBounds(nullptr, IType->getType(),
                                            false, true);

        if (!FunBounds)
          // This function has no return bounds
          return BoundsUtil::CreateBoundsAlwaysUnknown(S);

        ArrayRef<Expr *> ArgExprs =
          llvm::makeArrayRef(const_cast<Expr**>(CE->getArgs()),
                              CE->getNumArgs());

        // Concretize Call Bounds with argument expressions.
        // We can only do this if the argument expressions are non-modifying.
        // For argument expressions that are modifying, we issue an error
        // message only in checked scope because the argument expressions may
        // be re-evaluated during bounds validation.
        // TODO: The long-term solution is to introduce temporaries for
        // modifying argument expressions whose corresponding formals are used
        // in return or parameter bounds expressions.
        // Equality between the value computed by an argument expression and its
        // associated temporary would also need to be recorded.
        Sema::NonModifyingMessage Message =
          (CSS == CheckedScopeSpecifier::CSS_Unchecked) ?
          Sema::NonModifyingMessage::NMM_None :
          Sema::NonModifyingMessage::NMM_Error;
        ReturnBounds =
          S.ConcretizeFromFunctionTypeWithArgs(FunBounds, ArgExprs,
                       Sema::NonModifyingContext::NMC_Function_Return, Message);
        // If concretization failed, this means we tried to substitute with
        // a non-modifying expression, which is not allowed by the
        // specification.
        if (!ReturnBounds)
          return BoundsUtil::CreateBoundsInferenceError(S);
      }

      if (ReturnBounds->isElementCount() ||
          ReturnBounds->isByteCount()) {
        if (!ResultName)
          return BoundsUtil::CreateBoundsInferenceError(S);
        ReturnBounds = BoundsUtil::ExpandToRange(S, CreateTemporaryUse(ResultName), ReturnBounds);
      }
      return ReturnBounds;
    }

    // Check that casts to checked function pointer types produce a valid
    // function pointer.  This implements the checks in Section 3.8 of v0.7
    // of the Checked C specification.
    //
    // The cast expression E has type ToType, a ptr<> to a function p type.  To
    // produce the function pointer,  the program is performing a sequence of
    // casts, both implicit and explicit. This sequence may include uses of
    // addr-of- (&) or deref(*), which act like casts for function pointer
    // types.
    //
    // Start by checking whether E must produce a valid function pointer:
    // - An lvalue-to-rvalue cast,
    // - A bounds-safe interface cast.
    //
    // If E is not guaranteed produce a valid function pointer, check that E
    // is a value-preserving case. Iterate through the chain of subexpressions
    // of E, as long as we see value-preserving casts or a cast-like operator.
    // If a cast is not value-preserving, it is an error because the resulting
    // value may not be valid function pointer.
    //
    // Let Needle be the subexpression the iteration ends at. Check whether
    // Needle is guaranteed to be a valid checked function pointer of type Ty:
    // - It s a null pointer.
    // - It is decl ref to a named function and the pointee type of TyType
    //   matches the function type.
    // - It is a checked function pointer Ty.
    // If is none of those, emit diagnostic about an incompatible type.
    void CheckDisallowedFunctionPtrCasts(CastExpr *E) {
      // The type of the outer value
      QualType ToType = E->getType();

      // We're only looking for casts to checked function ptr<>s.
      if (!ToType->isCheckedPointerPtrType() ||
        !ToType->isFunctionPointerType())
        return;

      if (S.getLangOpts()._3C)
        return;

      // Skip lvalue-to-rvalue casts because they preserve types (except that
      // qualifers are removed).  The lvalue type should be a checked pointer
      // type too.
      if (const ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(E))
        if (ICE->getCastKind() == CK_LValueToRValue) {
          assert(ICE->getSubExpr()->getType()->isCheckedPointerType());
          return;
        }

      // Skip bounds-safe interface casts.  They are trusted casts inserted
      // according to bounds-safe interface rules.  The only difference in
      // types is checkedness, which means that this is a trusted cast
      // to the checked function type pointer.
      if (E->isBoundsSafeInterface())
        return;

      if (!CheckValuePreservingCast(E, ToType)) {
        // The top-level cast is not value-preserving
        return;
      }

      // Iterate through chain of subexpressions that are value-preserving
      // casts or cast-like operations.
      const Expr *Needle = E->getSubExpr();
      while (true) {
        Needle = Needle->IgnoreParens();

        // Stop at any cast or cast-like operators that have a checked pointer
        // type.  If they are potential problematic casts, they'll be checked
        // by another call to CheckedDisallowedFunctionPtrCasts.
        if (Needle->getType()->isCheckedPointerType())
          break;

        // If we've found a cast expression...
        if (const CastExpr *NeedleCast = dyn_cast<CastExpr>(Needle)) {
          if (const ImplicitCastExpr *ICE = 
                dyn_cast<ImplicitCastExpr>(NeedleCast))
            // Stop at lvalue-to-ravlue casts.
            if (ICE->getCastKind() == CK_LValueToRValue)
              break;

          if (NeedleCast->isBoundsSafeInterface())
            break;

          if (!CheckValuePreservingCast(NeedleCast, ToType)) {
            // The cast is not value-preserving,
            return;
          }

          Needle = NeedleCast->getSubExpr();
          continue;
        }

        // If we've found a unary operator (such as * or &)...
        if (const UnaryOperator *NeedleOp = dyn_cast<UnaryOperator>(Needle)) {
          // Check if the operator is value-preserving.
          // Only addr-of (&) and deref (*) are with function pointers
          if (!CheckValuePreservingCastLikeOp(NeedleOp, ToType)) {
            return;
          }

          // Keep iterating.
          Needle = NeedleOp->getSubExpr();
          continue;
        }

        // Otherwise we have found an expression that is neither
        // a cast nor a cast-like operator.  Stop iterating.
        break;
      }

      // See if we stopped at a subexpression that must produce a valid checked
      // function pointer.

      // A null pointer.
      if (Needle->isNullPointerConstant(S.Context, Expr::NPC_NeverValueDependent))
        return;

      // A DeclRef to a function declaration matching the desired function type.
      if (const DeclRefExpr *NeedleDeclRef = dyn_cast<DeclRefExpr>(Needle)) {
        if (isa<FunctionDecl>(NeedleDeclRef->getDecl())) {
          // Checked that the function type is compatible with the pointee type
          // of ToType.
          if (S.Context.typesAreCompatible(ToType->getPointeeType(),
                                           Needle->getType(),
                                           /*CompareUnqualified=*/false,
                                           /*IgnoreBounds=*/false))
            return;
        } else {
          S.Diag(Needle->getExprLoc(),
                 diag::err_cast_to_checked_fn_ptr_not_value_preserving)
            << ToType << E->getSourceRange();
          return;
        }
      }

      // An expression with a checked pointer type.
      QualType NeedleTy = Needle->getType();
      if (!S.Context.typesAreCompatible(ToType, NeedleTy,
                                      /* CompareUnqualified=*/false,
                                      /*IgnoreBounds=*/false)) {
        // See if the only difference is that the source is an unchecked pointer type.
        if (NeedleTy->isPointerType()) {
          const PointerType *NeedlePtrType = NeedleTy->getAs<PointerType>();
          const PointerType *ToPtrType = ToType->getAs<PointerType>();
          if (S.Context.typesAreCompatible(NeedlePtrType->getPointeeType(),
                                           ToPtrType->getPointeeType(),
                                           /*CompareUnqualifed=*/false,
                                           /*IgnoreBounds=*/false)) {
            // An _Assume_bounds_cast can be used to cast an unchecked function
            // pointer to a checked function pointer, if the only difference
            // is that the source is an unchecked pointer type.
            if (E->getCastKind() == CastKind::CK_AssumePtrBounds)
              return;
            S.Diag(Needle->getExprLoc(), 
                   diag::err_cast_to_checked_fn_ptr_from_unchecked_fn_ptr) <<
              ToType << E->getSourceRange();
            return;
          }
        }

        S.Diag(Needle->getExprLoc(), 
               diag::err_cast_to_checked_fn_ptr_from_incompatible_type)
          << ToType << NeedleTy << NeedleTy->isCheckedPointerPtrType()
          << E->getSourceRange();
      }

      return;
    }

    // See if a cast is value-preserving for a function-pointer casts.   Other
    // operations might also be, but this algorithm is currently conservative.
    //
    // This will add the required error messages.
    bool CheckValuePreservingCast(const CastExpr *E, const QualType ToType) {
      switch (E->getCastKind())
      {
      case CK_NoOp:
      case CK_NullToPointer:
      case CK_FunctionToPointerDecay:
      case CK_BitCast:
      case CK_LValueBitCast:
      // An _Assume_bounds_cast can be used to cast an unchecked function
      // pointer to a checked pointer, and should therefore be considered
      // value-preserving for a function-pointer cast.
      case CK_AssumePtrBounds:
        return true;
      default:
        S.Diag(E->getExprLoc(), diag::err_cast_to_checked_fn_ptr_not_value_preserving)
          << ToType << E->getSourceRange();

        return false;
      }
    }

    // See if an operationg is a value-preserving deref (*) or/ addr-of (&)
    // operator on a function pointer type.  Other operations might also be,
    // but this algorithm is currently conservative.
    //
    // This will add the required error messages
    bool CheckValuePreservingCastLikeOp(const UnaryOperator *E, const QualType ToType) {
      QualType ETy = E->getType();
      QualType SETy = E->getSubExpr()->getType();

      switch (E->getOpcode()) {
      case UO_Deref: {
        // This may be more conservative than necessary.
        bool between_functions = ETy->isFunctionType() && SETy->isFunctionPointerType();

        if (!between_functions) {
          // Add Error Message
          S.Diag(E->getExprLoc(), diag::err_cast_to_checked_fn_ptr_can_only_ref_deref_functions)
            << ToType << 0 << E->getSourceRange();
        }

        return between_functions;
      }
      case UO_AddrOf: {
        // This may be more conservative than necessary.
        bool between_functions = ETy->isFunctionPointerType() && SETy->isFunctionType();
        if (!between_functions) {
          // Add Error Message
          S.Diag(E->getExprLoc(), diag::err_cast_to_checked_fn_ptr_can_only_ref_deref_functions)
            << ToType << 1 << E->getSourceRange();
        }

        return between_functions;
      }
      default:
        S.Diag(E->getExprLoc(), diag::err_cast_to_checked_fn_ptr_not_value_preserving)
          << ToType << E->getSourceRange();

        return false;
      }
    }
  };
}

Expr *Sema::GetArrayPtrDereference(Expr *E, QualType &Result) {
  assert(E->isLValue());
  E = E->IgnoreParens();
  switch (E->getStmtClass()) {
    case Expr::DeclRefExprClass:
    case Expr::MemberExprClass:
    case Expr::CompoundLiteralExprClass:
    case Expr::ExtVectorElementExprClass:
    case Expr::RecoveryExprClass:
      return nullptr;
    case Expr::UnaryOperatorClass: {
      UnaryOperator *UO = cast<UnaryOperator>(E);
      if (UO->getOpcode() == UnaryOperatorKind::UO_Deref &&
          UO->getSubExpr()->getType()->isCheckedPointerArrayType()) {
        Result = UO->getSubExpr()->getType();
        return E;
      }

      return nullptr;
    }

    case Expr::ArraySubscriptExprClass: {
      // e1[e2] is a synonym for *(e1 + e2).
      ArraySubscriptExpr *AS = cast<ArraySubscriptExpr>(E);
      // An important invariant for array types in Checked C is that all
      // dimensions of a multi-dimensional array are either checked or
      // unchecked.  This ensures that the intermediate values for
      // multi-dimensional array accesses have checked type and preserve
      //  the "checkedness" of the outermost array.

      // getBase returns the pointer-typed expression.
      if (getLangOpts().UncheckedPointersDynamicCheck ||
          AS->getBase()->getType()->isCheckedPointerArrayType()) {
        Result = AS->getBase()->getType();
        return E;
      }

      return nullptr;
    }
    case Expr::ImplicitCastExprClass: {
      ImplicitCastExpr *IC = cast<ImplicitCastExpr>(E);
      if (IC->getCastKind() == CK_LValueBitCast)
        return GetArrayPtrDereference(IC->getSubExpr(), Result);
      return nullptr;
    }
    case Expr::CHKCBindTemporaryExprClass: {
      CHKCBindTemporaryExpr *Temp = cast<CHKCBindTemporaryExpr>(E);
      return GetArrayPtrDereference(Temp->getSubExpr(), Result);
    }
    case Expr::MatrixSubscriptExprClass: {
      MatrixSubscriptExpr *MS = cast<MatrixSubscriptExpr>(E);
      if (MS->getBase()->getType()->isCheckedPointerArrayType()) {
        Result = MS->getBase()->getType();
        return E;
      }

      return nullptr;
    }
    default: {
      llvm_unreachable("unexpected lvalue expression");
      return nullptr;
    }
  }
}

BoundsExpr *Sema::CheckNonModifyingBounds(BoundsExpr *B, Expr *E) {
  if (!CheckIsNonModifying(B, Sema::NonModifyingContext::NMC_Unknown,
                              Sema::NonModifyingMessage::NMM_None)) {
    Diag(E->getBeginLoc(), diag::err_inferred_modifying_bounds) <<
        B << E->getSourceRange();
    CheckIsNonModifying(B, Sema::NonModifyingContext::NMC_Unknown,
                          Sema::NonModifyingMessage::NMM_Note);
    return CreateInvalidBoundsExpr();
  } else
    return B;
}

Expr *Sema::MakeAssignmentImplicitCastExplicit(Expr *E) {
  if (!E->isRValue())
    return E;

  ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(E);
  if (!ICE)
    return E;

  bool isUsualUnaryConversion = false;
  CastKind CK = ICE->getCastKind();
  Expr *SE = ICE->getSubExpr();
  QualType TargetTy = ICE->getType();
  if (CK == CK_FunctionToPointerDecay || CK == CK_ArrayToPointerDecay ||
      CK == CK_LValueToRValue)
    isUsualUnaryConversion = true;
  else if (CK == CK_IntegralCast) {
    QualType Ty = SE->getType();
    // Half FP have to be promoted to float unless it is natively supported
    if (CK == CK_FloatingCast && TargetTy == Context.FloatTy &&
        Ty->isHalfType() && !getLangOpts().NativeHalfType)
      isUsualUnaryConversion = true;
    else if (CK == CK_IntegralCast &&
             Ty->isIntegralOrUnscopedEnumerationType()) {
      QualType PTy = Context.isPromotableBitField(SE);
      if (!PTy.isNull() && TargetTy == PTy)
        isUsualUnaryConversion = true;
      else if (Ty->isPromotableIntegerType() &&
              TargetTy == Context.getPromotedIntegerType(Ty))
        isUsualUnaryConversion = true;
    }
  }

  if (isUsualUnaryConversion)
    return E;

  return ExprCreatorUtil::CreateExplicitCast(*this, TargetTy, CK, SE,
                                             ICE->isBoundsSafeInterface());
}

void Sema::CheckFunctionBodyBoundsDecls(FunctionDecl *FD, Stmt *Body) {
  if (Body == nullptr)
    return;
#if TRACE_CFG
  llvm::outs() << "Checking " << FD->getName() << "\n";
#endif
  ModifiedBoundsDependencies Tracker;
  // Compute a mapping from expressions that modify lvalues to in-scope bounds
  // declarations that depend upon those expressions.  We plan to change
  // CheckBoundsDeclaration to traverse a function body in an order determined
  // by control flow.   The modification information depends on lexically-scoped
  // information that can't be computed easily when doing a control-flow
  // based traversal.
  ComputeBoundsDependencies(Tracker, FD, Body);

  // Run a prepass traversal over the function before running bounds checking.
  // This traversal gathers information that is used during bounds checking,
  // as well as in other Checked C analyses.
  PrepassInfo Info;
  CheckedCAnalysesPrepass(Info, FD, Body);

  std::pair<ComparisonSet, ComparisonSet> EmptyFacts;
  CFG::BuildOptions BO;
  BO.AddLifetime = true;
  BO.AddNullStmt = true;
  std::unique_ptr<CFG> Cfg = CFG::buildCFG(nullptr, Body, &getASTContext(), BO);
  CheckBoundsDeclarations Checker(*this, Info, Body, Cfg.get(), FD, EmptyFacts);
  if (Cfg != nullptr) {
    AvailableFactsAnalysis Collector(*this, Cfg.get());
    Collector.Analyze();
    if (getLangOpts().DumpExtractedComparisonFacts)
      Collector.DumpComparisonFacts(llvm::outs(), FD->getNameInfo().getName().getAsString());
    Checker.TraverseCFG(Collector, FD);
  }
  else {
    // A CFG couldn't be constructed.  CFG construction doesn't support
    // __finally or may encounter a malformed AST.  Fall back on to non-flow 
    // based analysis.  The CSS parameter is ignored because the checked
    // scope information is obtained from Body, which is a compound statement.
    Checker.Check(Body, CheckedScopeSpecifier::CSS_Unchecked);
  }

#if TRACE_CFG
  llvm::outs() << "Done " << FD->getName() << "\n";
#endif
}

void Sema::CheckTopLevelBoundsDecls(VarDecl *D) {
  if (!D->isLocalVarDeclOrParm()) {
    PrepassInfo Info;
    std::pair<ComparisonSet, ComparisonSet> EmptyFacts;
    CheckBoundsDeclarations Checker(*this, Info, nullptr, nullptr, nullptr, EmptyFacts);
    Checker.TraverseTopLevelVarDecl(D, GetCheckedScopeInfo());
  }
}

namespace {
  class NonModifiyingExprSema : public RecursiveASTVisitor<NonModifiyingExprSema> {

  private:
    // Represents which kind of modifying expression we have found
    enum ModifyingExprKind {
      MEK_Assign,
      MEK_Increment,
      MEK_Decrement,
      MEK_Call,
      MEK_Volatile
    };

  public:
    NonModifiyingExprSema(Sema &S, Sema::NonModifyingContext From,
                          Sema::NonModifyingMessage Message) :
      S(S), FoundModifyingExpr(false), ReqFrom(From),
      Message(Message) {}

    bool isNonModifyingExpr() { return !FoundModifyingExpr; }

    // Assignments are of course modifying
    bool VisitBinaryOperator(BinaryOperator *E) {
      if (E->isAssignmentOp()) {
        addError(E, MEK_Assign);
        FoundModifyingExpr = true;
      }
      
      return true;
    }

    // Pre-increment/decrement, Post-increment/decrement
    bool VisitUnaryOperator(UnaryOperator *E) {
      if (E->isIncrementDecrementOp()) {
        addError(E,
          E->isIncrementOp() ? MEK_Increment : MEK_Decrement);
        FoundModifyingExpr = true;
      }

      return true;
    }

    // Dereferences of volatile variables are modifying.
    bool VisitCastExpr(CastExpr *E) {
      CastKind CK = E->getCastKind();
      if (CK == CK_LValueToRValue)
        FindVolatileVariable(E->getSubExpr());

      return true;
    }

    void FindVolatileVariable(Expr *E) {
      E = E->IgnoreParens();
      switch (E->getStmtClass()) {
        case Expr::DeclRefExprClass: {
          QualType RefType = E->getType();
          if (RefType.isVolatileQualified()) {
            addError(E, MEK_Volatile);
            FoundModifyingExpr = true;
          }
          break;
        }
        case Expr::ImplicitCastExprClass: {
          ImplicitCastExpr *ICE = cast<ImplicitCastExpr>(E);
          if (ICE->getCastKind() == CastKind::CK_LValueBitCast)
            return FindVolatileVariable(ICE->getSubExpr());
          break;
        }
        default:
          break;
      }
    }

    // Function Calls are defined as modifying
    bool VisitCallExpr(CallExpr *E) {
      addError(E, MEK_Call);
      FoundModifyingExpr = true;

      return true;
    }

    // Do not traverse the children of a BoundsValueExpr. Any expressions
    // that are wrapped in a BoundsValueExpr should not be considered
    // modifying expressions. For example, BoundsValue(TempBinding(f()))
    // should not be considered modifying.
    bool TraverseBoundsValueExpr(BoundsValueExpr *E) {
      return true;
    }


  private:
    Sema &S;
    bool FoundModifyingExpr;
    Sema::NonModifyingContext ReqFrom;
    Sema::NonModifyingMessage Message;
    // Track modifying expressions so that we can suppress duplicate diagnostic
    // messages for the same modifying expression.
    SmallVector<Expr *, 4> ModifyingExprs;

    void addError(Expr *E, ModifyingExprKind Kind) {
      if (Message != Sema::NonModifyingMessage::NMM_None) {
        for (auto Iter = ModifyingExprs.begin(); Iter != ModifyingExprs.end(); Iter++) {
          if (*Iter == E)
            return;
        }
        ModifyingExprs.push_back(E);
        unsigned DiagId = (Message == Sema::NonModifyingMessage::NMM_Error) ?
          diag::err_not_non_modifying_expr : diag::note_modifying_expression;
        S.Diag(E->getBeginLoc(), DiagId)
          << Kind << ReqFrom << E->getSourceRange();
      }
    }
  };
}

bool Sema::CheckIsNonModifying(Expr *E, NonModifyingContext Req,
                               NonModifyingMessage Message) {
  NonModifiyingExprSema Checker(*this, Req, Message);
  Checker.TraverseStmt(E);

  return Checker.isNonModifyingExpr();
}

/* Will uncomment this in a future pull request.
bool Sema::CheckIsNonModifying(BoundsExpr *E, bool ReportError) {
  NonModifyingContext req = NMC_Unknown;
  if (isa<RangeBoundsExpr>(E))
    req = NMC_Range;
  else if (const CountBoundsExpr *CountBounds = dyn_cast<CountBoundsExpr>(E))
    req = CountBounds->isByteCount() ? NMC_Byte_Count : NMC_Count;

  NonModifiyingExprSema Checker(*this, Req, ReportError);
  Checker.TraverseStmt(E);

  return Checker.isNonModifyingExpr();
}
*/

void Sema::WarnDynamicCheckAlwaysFails(const Expr *Condition) {
  bool ConditionConstant;
  if (Condition->EvaluateAsBooleanCondition(ConditionConstant, Context)) {
    if (!ConditionConstant) {
      // Dynamic Check always fails, emit warning
      Diag(Condition->getBeginLoc(), diag::warn_dynamic_check_condition_fail)
        << Condition->getSourceRange();
    }
  }
}

// If the VarDecl D has a byte_count or count bounds expression,
// NormalizeBounds expands it to a range bounds expression.  The expanded
// range bounds are attached to the VarDecl D to avoid recomputing the
// normalized bounds for D.
BoundsExpr *Sema::NormalizeBounds(const VarDecl *D) {
  // Do not attempt to normalize bounds for invalid declarations.
  if (D->isInvalidDecl())
    return nullptr;

  // If D already has a normalized bounds expression, do not recompute it.
  if (BoundsExpr *NormalizedBounds = D->getNormalizedBounds())
    return NormalizedBounds;

  // Expand the bounds expression of D to a RangeBoundsExpr if possible.
  const BoundsExpr *B = D->getBoundsExpr();
  BoundsExpr *ExpandedBounds = BoundsUtil::ExpandBoundsToRange(*this,
                                             const_cast<VarDecl *>(D),
                                             const_cast<BoundsExpr *>(B));
  if (!ExpandedBounds)
    return nullptr;

  // Attach the normalized bounds to D to avoid recomputing them.
  D->setNormalizedBounds(ExpandedBounds);
  return ExpandedBounds;
}

// If the BoundsDeclFact F has a byte_count or count bounds expression,
// NormalizeBounds expands it to a range bounds expression.  The expanded
// range bounds are attached to the BoundsDeclFact F to avoid recomputing
// the normalized bounds for F.
BoundsExpr *Sema::NormalizeBounds(const BoundsDeclFact *F) {
  // Do not attempt to normalize bounds for bounds facts that are
  // associated with invalid declarations.
  const VarDecl *D = F->getVarDecl();
  if (D->isInvalidDecl())
    return nullptr;

  // If F already has a normalized bounds expression, do not recompute it.
  if (BoundsExpr *NormalizedBounds = F->getNormalizedBounds())
    return NormalizedBounds;

  // Expand the bounds expression of F to a RangeBoundsExpr if possible.
  const BoundsExpr *B = F->getBoundsExpr();
  BoundsExpr *ExpandedBounds = BoundsUtil::ExpandBoundsToRange(*this,
                                             const_cast<VarDecl *>(D),
                                             const_cast<BoundsExpr *>(B));
  if (!ExpandedBounds)
    return nullptr;

  // Attach the normalized bounds to F to avoid recomputing them.
  F->setNormalizedBounds(ExpandedBounds);
  return ExpandedBounds;
}

// Returns the declared bounds for the lvalue expression E. Assignments
// to E must satisfy these bounds. After checking a top-level statement,
// the inferred bounds of E must imply these declared bounds.
BoundsExpr *Sema::GetLValueDeclaredBounds(Expr *E, CheckedScopeSpecifier CSS) {
  if (DeclRefExpr *DRE = VariableUtil::GetLValueVariable(*this, E)) {
    if (const VarDecl *V = dyn_cast_or_null<VarDecl>(DRE->getDecl()))
      return NormalizeBounds(V);
  }

  PrepassInfo Info;
  std::pair<ComparisonSet, ComparisonSet> EmptyFacts;
  CheckBoundsDeclarations CBD(*this, Info, EmptyFacts);
  return CBD.GetLValueTargetBounds(E, CSS);
}

// Print Checked C profiling information.
void Sema::PrintCheckedCStats() {
  if (!getLangOpts().CheckedC)
    return;

  llvm::errs() << "\n*** Checked C Stats:\n";
  llvm::errs() << "  " << CheckedCStats.NumSynthesizedMemberExprs
               << " MemberExprs created while synthesizing members.\n";
  llvm::errs() << "  " << CheckedCStats.NumSynthesizedMemberAbstractSets
               << " AbstractSets created while synthesizing members.\n";
}
