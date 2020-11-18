# Bounds Declaration Checking

## Checking Overview
After each statement in the clang CFG, the inferred bounds for an expression must imply the expression's declared bounds. The algorithms for inferring and checking bounds for expressions are implemented in [SemaBounds.cpp](https://github.com/microsoft/checkedc-clang/blob/master/clang/lib/Sema/SemaBounds.cpp).

## Bounds Terminology
The bounds checking code uses the following definitions to reason about bounds expressions:
- **Declared bounds**: The bounds that the programmer has declared for a pointer expression. For example, in `array_ptr<int> p : bounds(p, p + 1) = 0`, the declared bounds of `p` are `bounds(p, p + 1)`.
- **Inferred bounds**: The bounds that the compiler determines for a pointer expression at a particular point in checking a statement. For example, after checking `array_ptr<int> p : bounds(p, p + 1) = 0`, the inferred bounds of `p` are `bounds(any)` (since `0` by definition has bounds of `bounds(any)`).

## Checking State
The bounds checking methods use the [CheckingState](https://github.com/microsoft/checkedc-clang/blob/master/clang/lib/Sema/SemaBounds.cpp#L661) class to maintain state while recursively checking expressions. The members of CheckingState track information that is then used to prove or disprove that inferred bounds imply declared bounds. Some notable members of CheckingState include:
- **ObservedBounds**: A map of variables to the current inferred bounds as determined by the bounds checker.
- **EquivExprs**: A set of sets of expressions that produce the same value. If expressions `e1` and `e2` are in the same set in EquivExprs, then `e1` and `e2` produce the same value.
- **SameValue**: a set of expressions that produce the same value as the expression that is currently being checked.