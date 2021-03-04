# Bounds Checking: Generalizing from Variables to LValues

## Problem Summary

The current bounds checking algorithm checks that the inferred bounds of each
in-scope variable imply the target bounds of the variable after each top-level
CFG statement. This supports bounds checking in multiple assignments.
For example:

```
void f(array_ptr<int> small : count(1),
       array_ptr<int> medium : count(2),
       array_ptr<int> large : count(3)) {
	// At the end of medium = small: medium's inferred bounds are count(1).
	// These inferred bounds do not imply the target bounds of count(2).
	// At the end of medium = large: medium's inferred bounds are count(3).
	// These inferred bounds imply the target bounds of count(2).
	// The bounds are not checked until the end of the statement, when
	// the inferred bounds of count(3) imply the target bounds of count(2).
	medium = small, medium = large;
}
```

For other kinds of lvalue expressions (`*p`, `a.f`, etc.), bounds checking
occurs immediately after checking an assignment to the expression. The goal
is to generalize the work that was previously done for variables so that the
bounds checking behavior is the same for all lvalue expressions.

## Definitions

This document uses the following terms for two lvalue expressions `e1` and `e2`.

`e1` and `e2` are **canonically equivalent** if and only if `e1` and `e2`
have the same canonical form; that is, if the normalized PreorderASTs for
`e1` and `e2` are equivalent. For example:

- `a->f` and `*a.f` are canonically equivalent.
- `*p` and `p[0]` are canonically equivalent.
- `a->f` and `b->f` are not canonically equivalent.
- `*p` and `*q` are not canonically equivalent.

`e1` and `e2` are **identical** if and only if `e1` and `e2` both point to
the same contiguous memory location, i.e. if `e1` and `e2` both point to
the same location and range of memory.

`e1` and `e2` **must alias** if it is guaranteed that `e1` and `e2` are
identical.

`e1` and `e2` **may alias** if `e1` points to a range `[L1, U1]` in memory,
`e2` points to a range `[L2, U2]` in memory, and it is possible that:

- `L1 <= L2 <= U1`, or:
- `L2 <= L1 <= U2`.

`e1` and `e2` **do not alias** if is not the case that `e1` and `e2` may alias.

An lvalue expression `e` **belongs** to an AbstractSet `A` if and only if the
canonical form (PreorderAST) `P` for `e` is equivalent to `A.CanonicalForm`.
If `e` belongs to `A`, then `A` **contains** `e`.

The bounds for expressions belonging to an AbstractSet `A` are **killed**
if `ObservedBounds[A]` is set to `bounds(unknown)`.

## Bounds Checking for Variables

The compiler tracks the inferred bounds for each in-scope variable while
traversing expressions. The following data structures and methods are relevant
for tracking, updating, and using the inferred bounds:

- **Tracking:** the `ObservedBounds` member of the `CheckingState` class.
- **Updating:** the `TraverseCFG`, `GetIncomingBlockState`,
  `UpdateCtxWithWidenedBounds`, `GetDeclaredBounds`, `ResetKilledBounds`,
  and `UpdateAfterAssignment` methods.
- **Using:** the `ValidateBoundsContext` and `RValueCastBounds` methods.

## Design: Data Structures and Methods

The proposed design for lvalue generalization involves the following data
structures and methods:

- AbstractSet class
  - This data structure represents an equivalence class of expressions,
    where lvalue expressions `e1` and `e2` belong to the same AbstractSet
    if and only if `e1` and `e2` are canonically equivalent. AbstractSets
    themselves do not consider aliasing concerns (aliasing is handled by
    the `AliasAnalysis` class).
- AbstractSetExprs map
  - This data structure maps an `Expr *` to the `AbstractSet *` to which
    it belongs. It is used for efficiency in GetOrCreateAbstractSet and to
    determine which expressions belong to a given AbstractSet.
- GetOrCreateAbstractSet method
  - This method returns the `AbstractSet *` for an `Expr * E` that is used
    as the key in the `ObservedBounds` map to read and update the observed
    bounds of `E`. It creates a new AbstractSet for `E` if necessary.
- GetExprsInAbstractSet method
  - This method returns the set of `Expr *` that belong to a given
    `AbstractSet * A`. It uses the `AbstractSetExprs` map to determine the set
    of expressions.
- GetLValueTargetBounds method
  - This method returns the target bounds for an lvalue `Expr * E`. It is used
    in ValidateBoundsContext to determine the target bounds for all lvalue
    expressions in an `AbstractSet * A`, in order to prove or disprove that
    `ObservedBounds[A]` imply the target bounds for `A`.
  - Note: the target bounds that GetLValueTargetBounds returns are normalized
    (if possible) to a `RangeBoundsExpr *`. For example, for the `DeclRefExpr *`
    `p` which has declared bounds of `count(2)` (a `CountBoundsExpr *`),
    GetLValueTargetBounds will return the `RangeBoundsExpr * bounds(p, p + 2)`.
- SynthesizeMemberExprs method
  - This method takes a `MemberExpr * e` and creates a set of MemberExprs
    whose target bounds use the value of `e`. For each `MemberExpr * m` in
    the resulting set of MemberExprs, `ObservedBounds[M]` are set to the
    target bounds of `m`, where `M` is the `AbstractSet *` that contains `m`.
- AliasAnalysis class
  - This class performs dataflow analysis to determine aliasing relationships
    between lvalue expressions. For an AbstractSet `E` which contains an lvalue
    expression `e`, the AliasAnalysis class supports queries for::
    - MustAlias: the set `S` of AbstractSets where, for each AbstractSet `A`
      in `S`, each expression belonging to `A` must alias with each expression
      that belongs to `E`.
    - MayAlias: The set `S` of AbstractSets where, for each AbstractSet `A` in
      `S`, each lvalue expression `a` that belongs to `A` obeys the following:
      - `a` may alias with `e`, and:
      - It is not the case that `a` must alias with `e`.
    - Note: the results of MustAlias and MayAlias are disjoint sets.

## AbstractSet: LValue Expression Equality

### Requirements for Bounds Checking

For certain lvalue expressions `e1` and `e2`, updating the inferred bounds
of `e1` should also update the inferred bounds of `e2`. For example:

```
struct S {
	array_ptr<int> f : count(2);
};

void f(struct S *a, array_ptr<int> p : count(3)) {
	(*a).f = 0, p = a->f;
}
```

At the end of `(*a).f = 0`, the inferred bounds of `(*a).f` are `bounds(any`).
At the end of `p = a->f`, the inferred bounds `p` are the inferred bounds of
`a->f`. The inferred bounds of `a->f` should be the same as the inferred bounds
of `(*a).f`, since `(*a).f` and `a->f` are identical lvalue expressions.

In general, for lvalue expressions `e1` and `e2`, updating the inferred bounds
of `e1` should also update the bounds of `e2` if and only if `e1` and `e2`
are guaranteed to be **identical lvalue expressions**. That is, if:

1. `e1` and `e2` point to the same location in memory, and:
2. `e1` and `e2` have the same range in memory.

For the initial planned work for lvalue generalization, we will only generalize
across lvalue expressions that we are able to determine as identical based on
the definition stated above. Further, while determining identical lvalue
expressions, we will initially ignore aliasing concerns and ignore lvalue
expressions that do not fully overlap in memory. We may consider aliasing
issues in future work.

### ObservedBounds Keys

Currently, the `ObservedBounds` map uses `VarDecl *` as its keys. This ensures
that updating the inferred bounds for a `DeclRefExpr *` `x` updates the
inferred bounds for all other `DeclRefExpr *` `y` where `x` and `y` have the
same `VarDecl *`. However, `VarDecl *` will not work as a key for general
lvalue expressions.

If the `ObservedBounds` map uses `Expr *` as its keys, then updating the
inferred bounds of `(*a).f` will not update the inferred bounds of `a->f`
since `(*a).f` and `a->f` are distinct expressions in the Clang AST.
Therefore, a different type is needed for the `ObservedBounds` keys.

To support the new keys for `ObservedBounds`, we introduce the AbstractSet
class. Given an lvalue expression `e`, an AbstractSet represents all lvalue
lvalue expressions that are identical to `e`. This representation will be
the key in `ObservedBounds` that maps to the inferred bounds of `e` (as well
as all other lvalue expressions that are identical to `e`).

The AbstractSet class is an abstraction of memory. If two lvalue expressions
`e1` and `e2` both belong to an AbstractSet `A`, then `e1` and `e2` are in
the set of lvalue expressions that point to the same contiguous memory
location. In addition, `e1` and `e2` must be canonically equivalent. The
AliasAnalysis class determines, for AbstractSets `A1` and `A2`, whether
the expressions in `A1` must or may alias with expressions in `A2`.

### AbstractSet Implementation

The AbstractSet class contains the following members:

- CanonicalForm
  - Type: `PreorderAST`
  - Description: canonical representation of expressions within the set.
    For example, if the expressions `a->f` and `*a.f` belong to the set,
    then CanonicalForm may be a PreorderAST representation of `a->f`.
    This will be used to determine which AbstractSet a new lvalue expression
    `e` belongs to, by comparing the canonical PreorderAST for `e` to
    CanonicalForm. This PreorderAST can be created from the first expression
    that is used to create the AbstractSet.
- We considered an alternative design for determining the target bounds for
  all lvalue expressions that belong to an AbstractSet. This involves adding
  a TargetBounds member to the AbstractSet class (of type `BoundsExpr *`) that
  stores the target bounds for all lvalue expressions in the set. This member
  would be set during `GetOrCreateAbstractSet` by calling `GetLValueTargetBounds`.

## Work Item Overview

1. Implement the AbstractSet class and GetOrCreateAbstractSet method as
   described above.
2. Implement lexicographic ordering for PreorderASTs. This is necessary
   to avoid an expensive linear search in GetOrCreateAbstractSet.
3. Replace the current `VarDecl *` keys in `ObservedBounds` with `AbstractSet *`.
   This should result in no changes in compiler behavior (since only
   `DeclRefExpr *` will have an AbstractSet representation).
4. Implement PreorderAST canonicalization for other kinds of lvalue expressions
   (see below for the suggested prioritized expression kinds). This should
   result in the compiler behavior for the implemented lvalue expressions
   to be identical to the current behavior for variables.
5. For each lvalue expression kind that has an AbstractSet representation,
   remove the current bounds checking behavior that deals with lvalue
   expressions that are not a `DeclRefExpr *`. For example, the
   `CheckBinaryOperator` method currently performs bounds checking for all
   assignments where the left-hand side is not a `DeclRefExpr *`.
6. Track equality for lvalue expressions. Certain types of lvalue expressions,
   e.g. expressions such as `*p` and `a->f` which read memory via a pointer,
   are currently not allowed in `EquivExprs`. However, bounds checking relies
   on equality information. For example, in an assignment `a->f = e`, where `e`
   is an expression with inferred bounds of `(e, e + i)`, the compiler needs
   to know that `a->f` and `e` are equal.

## LValue Expression Priorities

The lvalue generalization work will be done by incrementally adding AbstractSet
representations for each kind of lvalue expression. The bounds checker checks
the following kinds of lvalue expressions (in the `CheckLValue` method):

- `DeclRefExpr *`. Examples: `x`, `y`, `myvariable`.
- `UnaryOperator *`. Examples: `*p`, `*(p + 1)`.
- `ArraySubscriptExpr *`. Examples: `arr[0]`, `1[arr]`.
- `MemberExpr *`. Examples: `a.f`, `a->f`.
- `ImplicitCastExpr *`. Examples: `LValueBitCast(e)`.
- `CHKCBindTemporaryExpr *`. Examples: `TempBinding({ 0 })`.

The following priorities are proposed for AbstractSet implementations for each
kind of lvalue expression.

- Priority 0: `DeclRefExpr *`. This is necessary to maintain the current
  bounds checking behavior for variables. It will also serve to test the
  implementation of the AbstractSet API since implementing it for
  `DeclRefExpr *` should not result in any compiler behavior changes.
- Priority 1: `MemberExpr *`. This expression kind is more likely to be
  involved in bounds checking, since a `MemberExpr *` is more likely to have
  target bounds that are not `bounds(unknown)`.
- Priority 2: `UnaryOperator *` and `ArraySubscriptExpr *`. These are more
  likely than `MemberExpr *` to have unknown target bounds.
- Priority 3: `ImplicitCastExpr *` and `CHKCBindTemporaryExpr *`. These kinds
  are less common in example code.

## Lexicographic Ordering for PreorderASTs

Lexicographic ordering for PreorderASTs leverages the ordering implemented in
[CanonBounds.cpp](https://github.com/microsoft/checkedc-clang/blob/master/clang/lib/AST/CanonBounds.cpp).
particularly the CompareExpr method.

For two leaf nodes `N1` and `N2` of a PreorderAST, the result of
lexicographically comparing `N1` and `N2` is `CompareExpr(E1, E1)` for the
underlying expressions `E1` and `E2` of `N1` and `N2`.

As a possible future optimization, the PreorderAST class can maintain a set
of VarDecls that identify the set of variables that it uses. This can be used
to more quickly order two PreorderASTs `P1` and `P2` by comparing the set of
variables involved in `P1` and `P2`. This can help avoid expensive comparisons
for deeply nested PreorderASTs that differ only in the last expression.
For example, `P1` may be constructed from the expression `a->b->c->d` and
`P2` may be constructed from the expression `a->b->c->d->e`.
