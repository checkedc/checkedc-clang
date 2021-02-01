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

## Bounds Checking for Variables

The compiler tracks the inferred bounds for each in-scope variable while
traversing expressions. The following data structures and methods are relevant
for tracking, updating, and using the inferred bounds:

- **Tracking:** the `ObservedBounds` member of the `CheckingState` class.
- **Updating:** the `TraverseCFG`, `GetIncomingBlockState`,
`UpdateCtxWithWidenedBounds`, `GetDeclaredBounds`, `ResetKilledBounds`,
and `UpdateAfterAssignment` methods.
- **Using:** the `ValidateBoundsContext` and `RValueCastBounds` methods.

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

To support the new keys for `ObservedBounds`, we introduce an AbstractSet API.
Given an lvalue expression `e`, the API should return a representation of the
set containing all lvalue expressions that are identical to `e`. This
representation will be the key in `ObservedBounds` that maps to the inferred
bounds of `e` (as well as all other lvalue expressions that are identical to
`e`).

The AbstractSet representation of an lvalue expression `e` may use a canonical
form of `e`. For example, the canonical form of `(*a).f` may be `a->f`. This
canonicalization may use the
[PreorderAST](https://github.com/microsoft/checkedc-clang/blob/master/clang/include/clang/AST/PreorderAST.h).

## Work Item Overview

1. Define the representation that the AbstractSet API returns. What will be
   the type of the representation? What are some examples of the representation
   for different kinds of expressions (`DeclRefExpr *`, `MemberExpr *`, etc.)?
   (As described above, the representation may use the `PreorderAST` for
   canonicalization. We may also consider using the comparison in
   [CanonBounds.cpp](https://github.com/microsoft/checkedc-clang/blob/master/clang/lib/AST/CanonBounds.cpp)).
2. Implement the AbstractSet API for `DeclRefExpr *`. This is necessary to
   maintain the current behavior for checking variable bounds.
3. Replace the current `VarDecl *` keys in `ObservedBounds` with the
   AbstractSet representation. This should result in no changes in compiler
   behavior (since only `DeclRefExpr *` have AbstractSet representations).
4. Implement AbstractSet representations for other kinds of lvalue
   expressions (see below for the suggested prioritized expression kinds).
   This should result in the compiler behavior for the implemented lvalue
   expressions to be identical to the current behavior for variables.
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
kind of lvalue expression. Priority 0 and Priority 1 expressions should be
implemented before the April release of the Checked C compiler.

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
