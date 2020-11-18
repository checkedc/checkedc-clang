# Bounds Declaration Checking

## Checking Overview
After each statement in the clang CFG, the inferred bounds for an expression
must imply the expression's declared bounds. The algorithms for inferring and
checking bounds for expressions are implemented in [SemaBounds.cpp](https://github.com/microsoft/checkedc-clang/blob/master/clang/lib/Sema/SemaBounds.cpp).

## Bounds Terminology
The bounds checking code uses the following definitions to reason about
bounds expressions:
- **Declared bounds**: The bounds that the programmer has declared for a
pointer expression.
  -Example: In `array_ptr<int> p : bounds(p, p + 1) = 0`, the declared bounds
  of `p` are `bounds(p, p + 1)`.
- **Inferred bounds**: The bounds that the compiler determines for a pointer
expression at a particular point in checking a statement.
  -Example: After checking `array_ptr<int> p : bounds(p, p + 1) = 0`, the
  inferred bounds of `p` are `bounds(any)` (since `0` by definition has
  bounds of `bounds(any)`).
- **RValue bounds**: The bounds of the value produced by an rvalue expression.
  - Example: If `p` is a variable with declared bounds of `bounds(p, p + 1)`,
  then the rvalue expression obtained by reading the value of the variable `p`
  has bounds of `bounds(p, p + 1)`.
- **LValue bounds**: The bounds of an lvalue expression `e`. These bounds
determine whether it is valid to access memory using `e`, and should be the
range (or a subrange) of an object in memory.
  - Example: If `p` is a non-array-typed variable, then the lvalue bounds
  of `p` are `bounds(&p, &p + 1)`.
- **Target bounds**: The target bounds of an lvalue expression `e`. Values
assigned through `e` must satisfy these bounds. Values read through `e` will
meet these bounds.
  - Example: If `p` is a variable with declared bounds of `bounds(p, p + 1)`,
  then the target bounds of `p` are `bounds(p, p + 1)`. If `e` is an expression
  that is assigned through `p`, then the bounds of `e` must imply
  `bounds(p, p + 1)`.

Examples:
```
// p has declared bounds of bounds(p, p + i).
// q has declared bounds of bounds(q, q + j).
void f(array_ptr<int> p : count(i), array_ptr<int> q : count(j)) {
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

## Checking State
The bounds checking methods use the [CheckingState](https://github.com/microsoft/checkedc-clang/blob/master/clang/lib/Sema/SemaBounds.cpp#L661)
class to maintain state while recursively checking expressions. The members
of CheckingState track information that is then used to prove or disprove
that inferred bounds imply declared bounds. Some notable members of
CheckingState include:
- **ObservedBounds**: A map of variables to the current inferred bounds as
determined by the bounds checker.
  - Example: If ObservedBounds is `{ p => bounds(p, p + 1), q => bounds(unknown) }`,
  then the current inferred bounds of `p` are `bounds(p, p + 1)` and the
  current bounds of `q` are `bounds(unknown)`.
- **EquivExprs**: A set of sets of expressions that produce the same value. If
expressions `e1` and `e2` are in the same set in EquivExprs, then `e1` and `e2`
produce the same value.
  - Example: If EquivExprs is `{ { x, y, 1 }, { i + j, k } }`, then the
  variables `x` and `y` are equal to `1`, and the variable `k` is equal
  to `i + j`.
- **SameValue**: A set of expressions that produce the same value as the
expression that is currently being checked.
  - Example: If SameValue is `{ x, y, 1 }` and the current expression being
  checked is the variable `y`, then `x`, `y`, and `1` all produce the same
  value as `y`.

## Checking Methods
The entry point for bounds checking is the method [TraverseCFG](https://github.com/microsoft/checkedc-clang/blob/master/clang/lib/Sema/SemaBounds.cpp#L2317). For each statement `S` in the
clang CFG, TraverseStmt does the following:

1. Recursively traverse `S` and its subexpression by calling the Check method.
Checking `S` updates the checking state and infers bounds for expressions that
`S` may have modified.
2. For each variable `v` in the ObservedBounds map in the checking state,
validate that the inferred bounds of `v` as recorded in ObservedBounds imply
the declared bounds of `v`.

The recursive traversal for bounds checking is done in the two methods
[Check](https://github.com/microsoft/checkedc-clang/blob/master/clang/lib/Sema/SemaBounds.cpp#L2501) and [CheckLValue](https://github.com/microsoft/checkedc-clang/blob/master/clang/lib/Sema/SemaBounds.cpp#L2501).
These two methods check rvalue and lvalue methods, respectively. Check returns
the bounds of the value produced by an rvalue expression. CheckLValue returns
the lvalue bounds and target bounds of an lvalue expression.

Check and CheckLValue call different methods for different kinds of clang AST
expressions (CheckBinaryOperator, CheckUnaryOperator, CheckDeclRefExpr,
CheckArraySubscriptExpr, etc.). Each of these methods performs the following
actions for an expression `e`:
1. Infer the subexpression rvalue, lvalue, and target bounds as needed by
recursively calling Check and/or CheckLValue on the subexpressions of `e`.
2. If needed, use the inferred lvalue bounds to add a bounds check to an
lvalue expression `e1` (`e1` could be `e` or one of its subexpressions).
This check performs the following actions:
  * Checks that the memory access that the `e1` is being used to perform
  meets the lvalue bounds.
  * Sets the lvalue bounds of `e1`. During code generation, these lvalue
  bounds will be used to insert a dynamic check. At runtime, the dynamic
  check will verify that, for any access `*(e1 + i)`, `i` is within the
  lvalue bounds of `e1`.
3. Update the members of the CheckingState instance that maintains internal
checking state. For example, after an assignment to a variable `i` that is
used in the declared bounds of a variable `p`, update the ObservedBounds member
of the CheckingState to reflect the updated inferred bounds of `p`.
4. Use the inferred rvalue, lvalue, and target bounds of `e`'s subexpressions
to return the inferred rvalue, lvalue and/or target bounds of `e`.
  