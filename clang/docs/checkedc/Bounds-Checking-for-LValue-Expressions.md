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
for defining, updating, and using the inferred bounds:

### CheckingState

This class contains several members which maintain the state of the bounds
checker as it traverses expressions. Two members are especially revelant for
checking variable bounds:

- ObservedBounds: maps a variable declaration (`VarDecl *`) to the current
  bounds (`BoundsExpr *`) that the compiler has inferred for the variable.
- EquivExprs: set of sets of expressions. If two expressions `e1` and `e2`
  are in the same set in `EquivExprs`, then `e1` and `e2` produce the same
  value.

### TraverseCFG

This method traverses each block in the body of a function. Before traversing
the function body, each function parameter `p` with declared bounds is added
to ObservedBounds. For example:

```
void f(_Array_ptr<int> a : count(len), int len) {
	// ObservedBounds = { a => bounds(a, a + len) }.

	...Body of f
}
```

### GetIncomingBlockState

Before traversing a CFG block `B`, this method sets `ObservedBounds` and
`EquivExprs` to the intersection of the `ObservedBounds_i` and `EquivExprs_i`
from each predecessor block `B_i` of `B`. This ensures, that, before checking
a statement `S` within block `B`, `ObservedBounds` contains only variables that
are in scope at `S`. For example:

```
void f(int flag) {
	// Block B1.
	// At the end of this block, ObservedBounds contains bounds for a.
	_Array_ptr<int> a : count(1) = 0;

	if (flag) {
		// Block B2. Predecessors: B1.
		// At the beginning of this block: ObservedBounds contains a.
		// At the end of this block: ObservedBounds contains a and b.
		_Array_ptr<int> b : count(2) = 0;
	}

	// Block B3. Predecessors: B1, B2.
	// B1's ObservedBounds contains a. B2's observed bounds contains a and b.
	// At the beginning of this block: ObservedBounds contains a.
	// At the end of this block: ObservedBounds contains a and c.
	_Array_ptr<int> c : count(3) = 0;
}
```

### UpdateCtxWithWidenedBounds

Before checking a CFG block `B`, this method updates `ObservedBounds` to map
each variable that has widened bounds in `B` to its widened bounds. For
example:

```
void f(_Nt_array_ptr<char> p : count(1)) {
	if (*(p + 1)) {
		// CFG block B.
		// Within this block, the bounds of p are widened by 1.

		// p[1] is within the (widened) observed bounds of (p, p + 2).
		char c = p[1];
	}
}
```

`p` initially has observed bounds of `bounds(p, p + 1)` (the declared bounds
of `p`). In block `B`, `p` has widened bounds of `bounds(p, p + 2)`. Before
checking block `B`, `UpdateCtxWithWidenedBounds` updates `ObservedBounds` so
that `ObservedBounds[p] = bounds(p, p + 2)`.

### GetDeclaredBounds

Before checking a statement `S` within a CFG block `B`, this method updates
`ObservedBounds` so that, for each variable `x` declared in `S` that has
declared bounds `D`, `ObservedBounds[x] = D`. Before checking `S`, the observed
bounds for each variable `v` that is in scope at `S` will be either:

1. The widened bounds for `v`, if `v` has widened bounds within the current CFG
   block `B`, or:
2. The declared bounds for `v`, if `v` has declared bounds, or:
3. `ObservedBounds` will not contain an entry for `v` (e.g. `v` is an `int`).

### ResetKilledBounds

A statement `S` within a CFG block `B` may kill the widened bounds of a
variable `v` if `S` modified any variables that are used by the widened
bounds of `v`. After checking `S`, this method updates `ObservedBounds`
so that, for each variable `x` whose widened bounds are killed by `S`,
`ObservedBounds[x]` is the declared bounds of `x`.

### UpdateAfterAssignment

After an assignment to a variable `v`, this method updates each bounds
expression in `ObservedBounds` that uses `v`, and updates any sets in
`EquivExprs` that use `v`.

### ValidateBoundsContext

After checking a statement `S`, this method checks that, for each variable
declaration `v` in `ObservedBounds`, the inferred bounds `ObservedBounds[v]`
`ObservedBounds[v]` imply the declared bounds of `v` (`v->getBoundsExpr()`).
The bounds validation uses the expression equality recorded in `EquivExprs`.

### RValueCastBounds

For an `LValueToRValue` or `ArrayToPointerDecay` cast of a variable `v`,
the inferred rvalue bounds of the cast are the inferred bounds
`ObservedBounds[v]` of `v`.

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
are guranteed to be **identical lvalue expressions**. That is, if:

1. `e1` and `e2` point to the same location in memory, and:
2. `e1` and `e2` have the same range in memory.

This definition does not account for aliasing concerns. It is possible for
lvalue expressions to partially or completely overlap (for example, struct
variables `a` and `b` may share some or all of their fields). For the initial
planned work for lvalue generalization, we will only consider lvalue identity
as defined above. We may consider aliasing issues in future work.

### ObservedBounds Keys

Currently, the `ObservedBounds` map uses `VarDecl *` as its keys. This ensures
that updating the inferred bounds for a `DeclExpr *` `x` updates the inferred
bounds for all other `DeclRefExpr *` `y` where `x` and `y` have the same
`VarDecl *`. However, `VarDecl *` will not work as a key for general lvalue
expressions.

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
