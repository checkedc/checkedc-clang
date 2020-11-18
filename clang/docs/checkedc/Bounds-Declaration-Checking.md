# Bounds Declaration Checking

## Checking Overview
After each statement in the clang CFG, the inferred bounds for an expression must imply the expression's declared bounds. The algorithms for inferring and checking bounds for expressions are implemented in [SemaBounds.cpp](https://github.com/microsoft/checkedc-clang/blob/master/clang/lib/Sema/SemaBounds.cpp).

## Bounds Terminology
The bounds checking code uses the following definitions to reason about bounds expressions:
- **Declared bounds**: The bounds that the programmer has declared for a pointer expression. For example, in `array_ptr<int> p : bounds(p, p + 1) = 0`, the declared bounds of `p` are `bounds(p, p + 1)`.
- **Inferred bounds**: The bounds that the compiler determines for a pointer expression at a particular point in checking a statement. For example, after checking `array_ptr<int> p : bounds(p, p + 1) = 0`, the inferred bounds of `p` are `bounds(any)` (since `0` by definition has bounds of `bounds(any)`).

Examples:
```
// p has declared bounds of bounds(p, p + i).
// q has declared bounds of bounds(q, q + j).
void f(array_ptr<int> p : count(i), array_ptr<int> q : count(j)) {
  // Updating after assignment: The inferred bounds of the left-hand side
  // variable are the bounds of the right-hand side expression.
  // p has inferred bounds of bounds(q, q + j).
  p = q;

  // Updating after modifying a variable used in declared bounds:
  // The value of i is lost after this assignment, so the inferred
  // bounds of p are bounds(unknown).
  i = 0;

  // Updating after modifying a variable used in declared bounds:
  // The original value of i before this assignment was i - 1.
  // This original value is substituted for i in the inferred bounds
  // of p, so the inferred bounds of p are bounds(p, p + i - 1).
  i = i + 1;
}
```

## Checking State
The bounds checking methods use the [CheckingState](https://github.com/microsoft/checkedc-clang/blob/master/clang/lib/Sema/SemaBounds.cpp#L661) class to maintain state while recursively checking expressions. The members of CheckingState track information that is then used to prove or disprove that inferred bounds imply declared bounds. Some notable members of CheckingState include:
- **ObservedBounds**: A map of variables to the current inferred bounds as determined by the bounds checker.
  - Example: If ObservedBounds is `{ p => bounds(p, p + 1), q => bounds(unknown) }`, then the current inferred bounds of `p` are `bounds(p, p + 1)` and the current bounds of `q` are `bounds(unknown)`.
- **EquivExprs**: A set of sets of expressions that produce the same value. If expressions `e1` and `e2` are in the same set in EquivExprs, then `e1` and `e2` produce the same value.
  - Example: If EquivExprs is `{ { x, y, 1 }, { i + j, k } }`, then the variables `x` and `y` are equal to `1`, and the variable `k` is equal to `i + j`.
- **SameValue**: A set of expressions that produce the same value as the expression that is currently being checked.
  - Example: If SameValue is `{ x, y, 1 }` and the current expression being checked is the variable `y`, then `x`, `y`, and `1` all produce the same value as `y`.
  