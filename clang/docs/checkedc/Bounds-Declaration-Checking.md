# Bounds Declaration Checking

## Checking Overview

After each full expression within a function body, the inferred bounds for
the value produced by an lvalue expression must imply the target bounds for
the lvalue.
A **full expression** is an expression not nested within another expression
(according to the C11 standard, 2011).
The algorithms for inferring and checking bounds for expressions are
implemented in [SemaBounds.cpp](https://github.com/microsoft/checkedc-clang/blob/master/clang/lib/Sema/SemaBounds.cpp).

## Bounds Terminology

The bounds checking code uses the following definitions to reason about
bounds expressions:

- **Declared bounds**: The bounds that the programmer has declared for a
  pointer-typed or array-typed expression.
  - Example: In `_Array_ptr<int> p : bounds(p, p + 1) = 0;`, the declared bounds
    of `p` are `bounds(p, p + 1)`.
- **Inferred bounds**: The bounds that the compiler determines for a pointer
  expression at a particular point in checking an expression. In
  SemaBounds.cpp, these bounds may also be referred to as "observed bounds".
  - Example: After checking `_Array_ptr<int> p : bounds(p, p + 1) = 0`, the
    inferred bounds of `p` are `bounds(any)` (since `0` by definition has
    bounds of `bounds(any)`).
- **RValue bounds**: The bounds of the value produced by an rvalue expression.
  - Example: If `p` is a variable of type `_Array_ptr<int>` with declared bounds
    of `bounds(p, p + 1)`, then the rvalue expression `p + 3` has type
    `_Array_ptr<int>` and rvalue bounds of `bounds(p, p + 1)`.
  - Example: If `p` is a variable of type `_Array_ptr<int>` with declared bounds
    of `bounds(p, p + 5)`, then the rvalue expression from reading the value of
    `*(p + 2)` has rvalue bounds of `bounds(unknown)`. `*(p + 2)` has type `int`,
    and integers do not have bounds.
- **LValue bounds**: The bounds of an lvalue expression `e`. These bounds
  determine whether it is valid to access memory using the lvalue produced by
  `e`, and should be the range (or a subrange) of an object in memory. LValue
  bounds are used to check that a memory access is within bounds.
  - Example: If `p` is a non-array-typed variable, then the lvalue bounds
    of `p` are `bounds(&p, &p + 1)`.
  - Example: If `p` is a variable with declared bounds of `bounds(p, p + 3)`,
    then `*(p + 1)` and `p[2]` have lvalue bounds of `bounds(p, p + 3)`. The
    compiler will check that `p + 1` and `p + 2` are within the lvalue bounds of
    `bounds(p, p + 3)`.
- **Target bounds**: The target bounds of an lvalue expression `e`. Values
  assigned to `e` must satisfy these bounds. Values read through `e` will
  meet these bounds. If an lvalue expression `e` has target bounds, then `e`
  must either have pointer type or array type.
  - Example: If `p` is a variable with declared bounds of `bounds(p, p + 1)`,
    then the target bounds of `p` are `bounds(p, p + 1)`.
  - Example: If `p` is a pointer to a null-terminated array pointer, then
    the target bounds of `*p` are `bounds(*p, *p + 0)`.
  - Example: If `p` is a pointer to a singleton `_Ptr<T>`, then the target
    bounds of `*p` are `bounds((_Array_ptr<T>)*p, (_Array_ptr<T>)*p + 1)`.
  - Example: If S is a struct with members
    `{ _Array_ptr<int> f : count(len); int len; }` and `s` is a variable of
    type `struct S`, then the target bounds of `s.f` are
    `bounds(s.f, s.f + s.len)`. In an assignment `s.f = e;`, the rvalue
    bounds of `e` must imply `bounds(s.f, s.f + s.len)`.

Examples of declared and inferred bounds:

```
// p has declared bounds of bounds(p, p + i).
// q has declared bounds of bounds(q, q + j).
void f(_Array_ptr<int> p : count(i), _Array_ptr<int> q : count(j),
       unsigned int i, unsigned int j) {
  // Updating after assignment: The inferred bounds of the left-hand side
  // variable are the bounds of the right-hand side expression.
  // p has inferred bounds of bounds(q, q + j).
  p = q;

  // Updating after an invertible modification to a variable used in declared
  // bounds: The original value of i before this assignment is i - 1.
  // This original value is substituted for i in the inferred bounds
  // of p, so the inferred bounds of p are bounds(p, p + i - 1).
  i = i + 1;

  // Updating after some uninvertible modifications to a variable used in
  // declared bounds: The compiler cannot determine an original value for i
  // after these assignments, so the inferred bounds of p are bounds(unknown).
  i = 0;
  i = p[1] / 3;
  i = *q;
  i = 2 * i;
}
```

## Bounds Checking Implementation Overview

The bounds checker traverses each full expression in the body of a function.
During this traversal, the bounds checker determines the inferred bounds for
the lvalue expressions that are currently in scope. At the end of traversing
the full expression, the bounds checker determines the target bounds of each
lvalue expression, and attempts to prove or disprove that the inferred bounds
for the value produced by the lvalue expression imply the target bounds.

## Bounds Validity

For a given inferred bounds expression `S` and a target bounds expression `D`,
the bounds checker attempts to prove or disprove that `S` implies `D`. There
are three possible results for this proof:

1. `True`: the bounds checker proved that `S` implies `D`.
2. `False`: the bounds checker proved that `S` does not imply `D`.
3. `Maybe`: the bounds checker could neither prove nor disprove that `S`
   implies `D`. Some features of the bounds checker are not fully implemented,
   so it will not be able to prove or disprove bounds validity for all inferred
   and target bounds expressions.

The bounds checker uses the following rules to determine whether `S` implies
`D`:

1. If `S` is `bounds(any)`, then `S` implies `D`.
2. If `D` is `bounds(unknown)`, then `S` implies `D`.
3. If `S` is `bounds(unknown)` and `D` is not `bounds(unknown)`, then `S`
   does not imply `D`.

If `S` and `D` are neither `bounds(any)` nor `bounds(unknown)`, they are
converted to ranges. A range consists of:

1. A base expression.
2. A lower offset. This may be either an integer constant or an expression.
3. An upper offset. This may be either an integer constant or an expression.

For example, the bounds expression `bounds(p - 2, p + j)` will be converted to
a range with base `p`, lower offset `2`, and upper offset `j`.

In order for an inferred bounds range with base `S`, lower offset `Sl`, and
upper offset `Su` to imply a target bounds range with base `D`, lower offset
`Dl`, and upper offset `Du`, the following must be true:

1. `S == D`, and:
2. `Sl <= Dl`, and:
3. `Du <= Su`

In other words, the target bounds range must be contained within the inferred
bounds range.

If the compiler can prove that inferred bounds imply target bounds, no
compile-time errors or warnings are emitted. If the compiler can prove that
inferred bounds do not imply target bounds, a compile-time error is emitted.
If the compiler can neither prove nor disprove that inferred bounds imply
target bounds, a compile-time warning is emitted.

Examples of inferred bounds provably implying target bounds:

```
void f(_Array_ptr<int> small : count(2), _Array_ptr<int> large : count(5)) {
  // Target LHS bounds: bounds(small, small + 2)
  // Inferred RHS bounds: bounds(large + 5)
  small = large;

  // Target LHS bounds: bounds(unknown)
  // Inferred RHS bounds: bounds(unknown)
  large[3] = *small;

  // Target LHS bounds: bounds(small, small + 2)
  // Inferred RHS bounds: bounds(any)
  small = 0;

  // Target LHS bounds: bounds(large, large + 5)
  // Inferred RHS bounds: bounds(small, small + 10)
  large = _Dynamic_bounds_cast<_Array_ptr<int>>(small, bounds(small, small + 10));
}
```

Examples of inferred bounds provably not implying target bounds:

```
void f(_Array_ptr<int> small : count(2), _Array_ptr<int> large : count(5)) {
  // Target LHS bounds: bounds(large, large + 5)
  // Inferred RHS bounds: bounds(small, small + 2)
  large = small;

  // Target LHS bounds: bounds(large, large + 5)
  // Inferred RHS bounds: bounds(small, small + 3)
  large = _Dynamic_bounds_cast<_Array_ptr<int>>(small, bounds(small, small + 3));
}
```

Examples where the bounds checker cannot prove that the inferred bounds
imply the target bounds:

```
void f(_Array_ptr<int> p : count(2), _Array_ptr<int> q : count(3)) {
  // Target LHS bounds: bounds(p, p + 2)
  // Inferred RHS bounds: bounds(q, q + 3)
  // Target LHS range: p with lower offset 0 and upper offset 2
  // Inferred RHS range: q with lower offset 0 and upper offset 3
  // The bases of these ranges (p and q) are not equivalent. After the
  // assignment p = q + 1, p and q + 1 are equivalent, but p and q are
  // not equivalent. Since the bases are not equivalent, the bounds checker
  // cannot prove the validity of these bounds.
  p = q + 1;
}
```

## Bounds Checking Across Multiple Assignments

During bounds checking, the checker maintains a state that tracks the observed
bounds for certain lvalue expressions (see below for more details). The
observed bounds in this state are validated when:

1. The checker is done traversing a full expression that is **not** contained
   within a bundled statement.
2. The checker is done traversing the last full expression in a bundled
   statement.

During these validations, the observed bounds of all lvalue expressions as
recorded in the current checking state must imply the target bounds for the
lvalue expression.

**1. Comma-separated assignments**

Note that, in an expression that contains multiple assignments, bounds checking
is only performed at the end of the full expression, not after each assignment.
The observed bounds of variables, member expressions, pointer dereferences,
and array subscripts are updated after each assignment, but validating the
updated observed bounds is not done until the end of the entire full
expression.

For example:

```
void f(_Array_ptr<int> small : count(1),
       _Array_ptr<int> medium : count(2),
       _Array_ptr<int> large : count(3)) {
  medium = small, medium = large;
}
```

After checking the assignment `medium = small`, the observed bounds of `medium`
are `bounds(small, small + 1)`, which are too narrow to imply the target bounds
of `bounds(medium, medium + 2)`. However, after the assignment `medium = large`,
the observed bounds of `medium` are `bounds(large, large + 3)`. At the end of
checking the statement `medium = small, medium = large;`, the bounds checker
validates that the observed bounds of `bounds(large, large + 3)` imply the
target bounds of `bounds(medium, medium + 2)`. This proof result returns
`True`, so no compiler errors are emitted for this statement.

The observed bounds for lvalue expressions are tracked throughout a full
expression and then reset after validating the bounds. For example:

```
void f(_Array_ptr<int> p : count(2), _Array_ptr<int> q : count(1)) {
  p = q, q++;
  p--;
}
```

After checking the assignment `p = q`, the observed bounds of `p` are
`bounds(q, q + 1)`. After the increment `q++`, the observed bounds of `p`
are `bounds(q - 1, (q - 1) + 1)`. These are the bounds for `p` that the
checker uses during bounds validation at the end of checking the statement
`p = q, q++;`. After validating the bounds, the observed bounds of `p` are
reset to its declared bounds of `bounds(p, p + 2)`.

After the decrement `p--`, the observed bounds of `p` are
`bounds(p + 1, (p + 1) + 2)`. These are the bounds for `p` that the checker
uses during bounds validation at the end of checking the statement `p--;`.

**2. Bundled statements**

Bounds checking for bundled statements behaves in a similar manner as
bounds checking for expressions that contain comma-separated assignments.
The observed bounds of lvalues are tracked across all expressions in a
bundled statement, and bounds validation is only performed after traversing
the last full expression in the bundled statement. For example:

```
void f(_Array_ptr<int> small : count(1),
       _Array_ptr<int> medium : count(2),
       _Array_ptr<int> large : count(3)) {
  _Bundled {
    medium = small;
    medium = large;
  }
}
```

After traversing the assignment `medium = small;`, the observed bounds for
`medium` are `bounds(small, small + 1)`, which are too narrow to imply the
target bounds for `medium`. However, there is no bounds validation performed
here since `medium = small;` is not the last statement in the bundled
statement.

After traversing the assignment `medium = large;`, the observed bounds for
`medium` are `bounds(large, large + 3)`. Since `medium = large;` is the last
statement in the bundled statement, these are the observed bounds that the
checker uses during bounds validation. These observed bounds do imply the
target bounds of `bounds(medium, medium + 2)`, so there is no compiler error
emitted for this bundled statement.

## Checking LValue Expressions

While traversing each full expression in a function body, the bounds checker
keeps track of the inferred bounds for the following kinds of lvalue
expressions:

1. Variables (e.g. `v`)
2. Member expressions (e.g. `s.f`, `s->f`)
3. Pointer dereference expressions (e.g. `*p`)
4. Array subscript expressions (e.g. `p[i]`)

It is possible for a programmer to express the same member expression, pointer
deference, or array subscript in multiple ways. This means that different clang
expression nodes can refer to the same lvalue. For two lvalue expressions `e1`
and `e2`, we say that `e1` and `e2` are **identical** if and only if `e1` and
`e2` both point to the same contiguous memory location, i.e. if `e1` and `e2`
both point to the same location and range of memory.

Any two identical lvalue expressions should always have the same inferred
bounds. For example:

```
void f(_Array_ptr<_Nt_array_ptr<char>> p : count(10)) {
  *p = "abc";
  p[0] = "xyz";
}
```

The lvalue expressions `*p` and `p[0]` are identical. Therefore, the assignment
`*p = "abc"` should not only update the inferred bounds of `*p`, but also of
`p[0]` (as well as all other lvalue expressions such as `*(p + 0)`, `p[1 - 1]`,
etc. that are identical to `*p`). The assignment `p[0] = "xyz"` should behave
in a similar manner.

In order to perform these updates, we consider equivalence classes of lvalue
expressions where `e1` and `e2` belong to the same equivalence class if and
only if `e1` and `e2` are identical. The
[AbstractSet](https://github.com/microsoft/checkedc-clang/blob/master/clang/include/clang/AST/AbstractSet.h)
class is a representation of an equivalence class of lvalue expressions.
The bounds checker uses `AbstractSets` to track the inferred bounds for all
lvalue expressions that belong to a given `AbstractSet`.

### Canonical Forms

In the current implementation, we consider two lvalue expressions to belong
to the same `AbstractSet` if and only if they have the same canonical form.
The canonical form of an lvalue expression `e` is defined as:

1. If `e` is a variable, then the canonical form of `e` is `e` together with
   its declaration. (In other words, two variables `e1` and `e2` are canonically
   equivalent if and only if `e1` and `e2` share the same declaration. It is not
   sufficient for `e1` and `e2` to share the same variable name).
2. If `e` is a member expression of the form `a.f`, then the canonical form
   of `e` is `a.f`.
3. If `e` is a member expression of the form `a->f` `*a.f`, `(*(a + 0)).f`,
   `(*(0 + a)).f`, `a[0].f`, or `0[a].f`, then the canonical form of `e` is
   `(a + 0)->f`.
4. If `e` is a member expression of the form `(a + i)->f`, `(a + i)->f`,
   `(*(a + i)).f`, `(*(i + a)).f`, `a[i].f`, or `i[a].f`, where `i` is an integer,
   then the canonical form of `e` is `(a + i)->f`.
5. If `e` is a pointer dereference of the form `*e1`, `*(e1 + 0)`, `*(0 + e1)`,
   `e1[0]`, or `0[e1]`, then the canonical form of `e` is `*(e1 + 0)`.

The [PreorderAST](https://github.com/microsoft/checkedc-clang/blob/master/clang/include/clang/AST/PreorderAST.h)
class determines the canonical form of an expression, and determines whether
two canonical forms are equivalent. Therefore, two lvalue expressions `e1` and
`e2` belong to the same `AbstractSet` if and only the `PreorderAST` for `e1`
and the `PreorderAST` for `e2` are equivalent. If an lvalue expression belongs
to an `AbstractSet` `A`, we say that `A` **contains** `e`.

## Checking State

The bounds checking methods use the `CheckingState` class (defined in
[SemaBounds.cpp](https://github.com/microsoft/checkedc-clang/blob/master/clang/lib/Sema/SemaBounds.cpp))
to maintain state while recursively checking expressions. The members of
CheckingState track information that is then used to prove or disprove that
inferred bounds imply target bounds. Some notable members of CheckingState
include:

### ObservedBounds

A map of an `AbstractSet` to the current inferred bounds of all lvalue
expression in the `AbstractSet` as determined by the bounds checker while
traversing a full expression.

Example: suppose `P` is an `AbstractSet` that contains the variable `p`, and
`A0` is an `AbstractSet` that contains the expressions `*a`, `*(a + 0)`, and
`a[0]`. If `ObservedBounds` is `{ P => bounds(p, p + 1), A0 => bounds(unknown) }`,
then the current inferred bounds of `p` are `bounds(p, p + 1)`, and the current
inferred bounds of `*a`, `*(a + 0)`, and `a[0]` are `bounds(unknown)`.

`ObservedBounds` is updated after each assignment to a variable, member
expression, pointer dereference, and array subscript in the
`UpdateBoundsAfterAssignment` method. It is used to check the validity of
bounds in the `ValidateBoundsContext` method.

### EquivExprs

A set of sets of expressions that produce the same value. If expressions `e1`
and `e2` are in the same set in EquivExprs, then `e1` and `e2` produce the
same value.

`EquivExprs` contains only rvalue expressions. Certain kinds of rvalue
expressions are not allowed in `EquivExprs`, such as expressions that:

1. Modify any expressions. For example, `p++` is not allowed in `EquivExprs`.
2. Read memory via a pointer. For example, `*p` is not allowed in `EquivExprs`.
3. Are a member expression. For example, `a.f` is not allowed in `EquivExprs`.
4. Create an object literal. For example, `"abc"` is not allowed in
   `EquivExprs`.

Example: If `EquivExprs` is `{ { x, y, 1 }, { i + j, k } }`, then the variables
`x` and `y` produce the same value as `1`, and the variable `k` produces the
same value as `i + j`. Note that since `EquivExprs` contains only rvalue
expressions, `x`, `y`, `i`, `j`, and `k` should be read as `LValueToRValue(x)`,
etc. where `LValueToRValue` is a clang implicit cast that produces the value
of an lvalue expression.

### SameValue

A set of expressions that produce the same value as the expression that is
currently being checked.

Like `EquivExprs`, `SameValue` contains only rvalue expressions. Any
expressions that are not allowed in `EquivExprs` are not allowed in
`SameValue`.

Example: If `SameValue` is `{ x, y, 1 }` and the current expression being
checked is the variable `y`, then `x`, `y`, and `1` all produce the same
value as `y`.

In addition, the `CheckingState` class also has several members that keep
track of information that is used to emit diagnostic messages during bounds
validation (if the bounds checker is not able to prove that a given inferred
bounds expression implies a given target bounds expression). These members
include:

1. `LostLValues`
2. `UnknownSrcBounds`
3. `BlameAssignments`

## Updating the Checking State

The `ObservedBounds` and `EquivExprs` members of the `CheckingState` instance
are updated before, during, and after checking each statement `S` in a CFG
block `B`.

**Before** checking the block `B`:

The incoming `CheckingState` for a block `B` is determined by taking the
intersection of the `CheckingStates` for each predecessor block of `B`.
This intersection is computed in the method `GetIncomingBlockState`. To
get the intersection of any two `CheckingState` instances `State1` and
`State2`:

1. The resulting `ObservedBounds` map contains a mapping for each `AbstractSet`
   `A` that is a key in both `State1.ObservedBounds` and `State2.ObservedBounds`.
   `ObservedBounds[A]` is the target bounds for all lvalue expressions in `A`.
2. Each set in the resulting `EquivExprs` is the intersection of a set `F`
   in `State1.EquivExprs` and a set `G` in `State2.EquivExprs`, if the
   intersection of `F` and `G` contains more than one expression.

**Before** checking `S`:

For each variable `v` that:

1. Is in scope at `S` or is declared in `S`, and:
2. Has declared bounds,

`ObservedBounds` will contain an `AbstractSet` `V` that contains `v`.

`ObservedBounds[V]` (where `V` contains a variable `v`) will be either:

1. The widened bounds of `v`, if `v` has widened bounds at the statement `S`
   (as determined by the [BoundsWideningAnalysis](https://github.com/microsoft/checkedc-clang/blob/master/clang/include/clang/Sema/BoundsWideningAnalysis.h)), or:
2. The declared bounds of `v`.

**While** checking `S`:

`ObservedBounds` and `EquivExprs` may be updated by individual checking
methods. For example, `UpdateAfterAssignment` updates the checking state
after assignments, increments, and decrements.

**After** checking `S`:

The updated checking state is used to validate that the observed bounds of
each variable imply the declared bounds of the variable.

After validating the bounds, `ObservedBounds` is reset to its value from before
checking `S`. Any observed bounds that were updated while checking `S` are only
used to validate the bounds. They do not persist across multiple statements in
a CFG block. Finally, `ObservedBounds` is updated so that, if a variable `v` is
no longer in scope after `S`, the `AbstractSet` `V` containing `v` is removed
from `ObservedBounds`. The value of `v` is also removed fro `EquivExprs`.

The following methods are responsible for updating the `ObservedBounds` and
`EquivExprs` members of the checking state.

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
  // At the end of this block: ObservedBounds contains bounds for a.
  // EquivExprs = { { 0, a } }.
  _Array_ptr<int> a : count(1) = 0;

  if (flag) {
    // Block B2. Predecessors: B1.
    // At the beginning of this block: ObservedBounds contains a.
    // At the end of this block: ObservedBounds contains a and b.
    // EquivExprs = { { 0, a, b } }.
    _Array_ptr<int> b : count(2) = 0;
  }

  // Block B3. Predecessors: B1, B2.
  // B1's ObservedBounds contains a. B2's observed bounds contains a and b.
  // At the beginning of this block: ObservedBounds contains a.
  // At the end of this block: ObservedBounds contains a and c.
  // EquivExprs = { { 0, a, c } }.
  _Array_ptr<int> c : count(3) = 0;
}
```

### GetDeclaredBounds

Before checking a statement `S` within a CFG block `B`, this method updates
`ObservedBounds` so that, for each variable `x` declared in `S` that has
declared bounds `D`, `ObservedBounds[x] = D`. For example:

```
void f(_Array_ptr<int> a : count(1)) {
  // Before calling GetDeclaredBounds:
  // ObservedBounds = { a => bounds(a, a + 1) }
  // After calling GetDeclaredBounds:
  // ObservedBounds = { a => bounds(a, a + 1), b => bounds(b, b + 2) }
  _Array_ptr<int> b : count(2) = 0;
}
```

### UpdateWidenedBounds

Before checking a CFG block `B`, this method updates `ObservedBounds` to map
each variable that has widened bounds in `B` to its widened bounds. For
example:

```
void f(_Nt_array_ptr<char> p : count(2)) {
  // Since p is dereferenced at its upper bound of 2, the bounds of p are
  // widened by 1.
  if (*(p + 2)) {
    // CFG block B.

    // Before checking this statement, the initial observed bounds of p
    // are its widened bounds of bounds(p, p + 2).
    // p[2] is within the (widened) observed bounds of bounds(p, p + 2).
    // This will not result in a compile-time error.
    char c = p[2];

    // This statement kills the widened bounds of p.
    p = 0;

    // p no longer has widened bounds at this statement. The initial observed
    // bounds of p are its declared bounds of bounds(p, p + 1).
    // p[2] is outside of its bounds of bounds(p, p + 1). This will result
    // in a compile-time error.
    char d = p[2];
  }
}
```

`p` initially has observed bounds of `bounds(p, p + 1)` (the declared bounds
of `p`). At the statement `char c = p[2]`, `p` has widened bounds of
`bounds(p, p + 2)`. Before checking `char c = p[2]`, `UpdateWidenedBounds`
updates `ObservedBounds` so that `ObservedBounds[P] = bounds(p, p + 2)` (where
`P` is the `AbstractSet` that contains `p`).

### UpdateStateForVariableOutOfScope

After traversing a statement `S`, a given variable `v` may no longer be in
scope. If `v` belongs to an `AbstractSet` `V` that is a key in `ObservedBounds`,
`UpdateStateForVariableOutOfScope` removes `V` from `ObservedBounds` so that
subsequent statements do not perform bounds checking for out-of-scope variables.
If the value of `v` belongs to a set `F` in `EquivExprs`, then the value of `v`
is removed from `F`, and if `F` then contains fewer than two expressions, `F`
is removed from `EquivExprs`.

### UpdateAfterAssignment

After an assignment to a lvalue expression `e` of the form `e = src`, this
method makes several updates to the `CheckingState` instance:

1. Updates the inferred bounds of each `AbstractSet` `A` in `ObservedBounds`
   if the current inferred bounds of `A` use the value of `e`.
2. Update the inferred bounds of the RHS rvalue expression `src` if they use
   the value of `e`.
3. If `e` has target bounds, set the inferred bounds of `E` to the updated
   inferred bounds for `src` (where `E` is the `AbstractSet` containing `E`).
4. Update each rvalue expression in `EquivExprs` and `SameValue` that uses
   the value of `e`. Note that `SameValue` will contain expressions that
   produce the same value as `src`.
5. Update `EquivExprs` and `SameValue` to record equality between the value
   of `e` and `src`.

After certain kinds of assignments to `e`, `e` may have an original value
from before the assignment. For example, in the assignment `e = e + 1`, where
`e` is an unsigned integer or a pointer, the original value of `e` is `e - 1`.
If `e` has an original value, this original value is used to update each
inferred bounds expression in `ObservedBounds` and each rvalue expression in
`EquivExprs` and `SameValue`.

For each `AbstractSet` `A` in `ObservedBounds`:

1. Let `B` = `ObservedBounds[A]`.
2. If `B` uses the value of `e`, and the original value of `e` is a non-null
   expression `OV`, replace all uses of `e` in `B` with `OV`.
3. If `B` uses the value of `e`, and the original value of `e` is null, set
   `ObservedBounds[A]` to `bounds(unknown)`.

The inferred bounds for `src` and the expressions in `EquivExprs` are updated
in a similar manner. For expression in `SameValue`, if the original value `OV`
of `e` uses the value of `e` (e.g. if the original value is `e - 1`), then any
expressions in `SameValue` that use the value of `e` are removed from
`SameValue`. This prevents `SameValue` from recording equality between
expressions such as `e + 1` and `(e - 1) + 1`.

For example:

```
void f(_Array_ptr<int> a : count(i), unsigned i) {
  // Before checking each statement: a's observed bounds are bounds(a, a + i).

  // The original value of i is null. After checking this statement,
  // the observed bounds of a are bounds(unknown).
  i = 0;

  // The original value of i is i + 1. After checking this statement,
  // the observed bounds of a are bounds(a, a + (i + 1)).
  i--;

  // The original value of a is a - 2. After checking this statement,
  // the observed bounds of a are bounds(a - 2, (a - 2) + 1).
  a += 2;
}
```

### ValidateBoundsContext

After checking a statement `S`, this method checks that, for each `AbstractSet`
`A` in `ObservedBounds`, the inferred bounds `ObservedBounds[A]` for all lvalue
expressions in `A` imply the target bounds for all lvalue expressions in `A`.
The bounds validation uses the expression equality recorded in `EquivExprs`.

## Checking Methods

The entry point for bounds checking is the method [TraverseCFG](https://github.com/microsoft/checkedc-clang/blob/master/clang/lib/Sema/SemaBounds.cpp#L2317). For each statement `S` in the
clang CFG, TraverseStmt does the following:

1. Recursively traverse `S` and its subexpression by calling the `Check`
   method. Checking `S` updates the checking state and infers bounds for expressions that `S` may have modified.
2. For each `AbstractSet` `A` in the `ObservedBounds` map in the checking
   state, validate that the inferred bounds of the lvalue expressions in `A`
   as recorded in `ObservedBounds` imply the target bounds of the lvalue
   expressions in `A`.

The recursive traversal for bounds checking is done in the two methods
`Check` and `CheckLValue`. These two methods check rvalue and lvalue methods,
respectively. Check returns the bounds of the value produced by an rvalue expression. CheckLValue returns the lvalue bounds of an lvalue expression.

`Check` and `CheckLValue` call different methods for different kinds of clang
expressions (`CheckBinaryOperator`, `CheckUnaryOperator`, `CheckDeclRefExpr`,
`CheckArraySubscriptExpr`, etc.). Each of these methods performs the following
actions for an expression `e`:

1. Infer the subexpression rvalue and/or lvalue bounds as needed by recursively
   calling `Check` and/or `CheckLValue` on the subexpressions of `e`.
2. If needed, infer the target bounds for an lvalue expression (e.g. the LHS
   of an assignment) by calling `GetLValueTargetBounds`.
3. If needed, use the inferred lvalue bounds to add a bounds check to an
   lvalue expression `e1` (`e1` could be `e` or one of its subexpressions).
   This check performs the following actions:

- Checks that the memory access that `e1` is being used to perform
  meets the lvalue bounds.
- Sets the lvalue bounds of `e1`. During code generation, these lvalue
  bounds will be used to insert a dynamic check. At runtime, the dynamic
  check will verify that, for any access `*(e1 + i)`, `i` is within the
  lvalue bounds of `e1`.

4. Update the members of the `CheckingState` instance that maintains internal
   checking state. For example, after an assignment to a variable `i` that is
   used in the declared bounds of a variable `p`, update the `ObservedBounds`
   member of the `CheckingState` instance to reflect the updated inferred
   bounds of the `AbstractSet` that contains `p`.
5. Use the inferred rvalue, lvalue, and/or target bounds of `e`'s
   subexpressions to return the inferred rvalue or lvalue bounds of `e`.
