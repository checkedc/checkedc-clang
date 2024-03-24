# Bounds Validation Plan

This doc represents the status of bounds validation as of 15th Sept 2021. The bounds checker may have had improvements/changes/bug fixes added to it since then.

This document lists some test cases to:

1) illustrate the current status of the bounds checker by providing examples that the bounds checker currently supports, and
2) enumerate some areas of future work by providing examples that the bounds checker does not currently support.

## Current Status

As of 15th Sept 2021, the bounds checker can use sets of equivalent expressions to detect equality between expressions used in bounds.

The following example illustrates some ways in which the bounds checker currently uses equivalent expressions.

### Example 1: Equivalent Expressions

```
void f(_Array_ptr<int> p : count(3), _Array_ptr<int> q : count(4)) {
  // Equality between p and q is implied by the assignment.
  // The observed bounds of bounds(q, q + 4) imply the declared bounds of
  // bounds(p, p + 3).
  p = q;
  
  // Equality between i and 3 is implied by the initialization.
  int i = 3;
  
  // Equality between r and p is implied by the initialization.
  // The observed bounds of bounds(p, p + 3) imply the declared bounds of
  // bounds(r, r + i) since we have equality between r and p and between
  // i and 3.
  _Array_ptr<int> r : count(i) = p;
}
```

As of 15th Sept 2021, the bounds checker includes some limited support for commutativity, associativity, and constant folding.

Suppose `E` is the upper expression of a bounds expression. If `E` can be expressed as `C1 +/- C2 +/- ... +/- Cj + E1 +/- Ck +/- ... +/- Cn`, where:

1. `E1` is a pointer-typed expression, and:
2. Each `Ci` is an optional integer constant, and:
3. `C1 +/- ... +/- Cn` does not overflow, then:

we can rewrite `E` as `E1 + C`, where `C` is `C1 +/- ... +/- Cn`.

In addition, the bounds checker can apply left-associativity to `E1` if applicable. For example, if `E1` can be expressed as `p + (i + j)`, where `p` has pointer type and `i` and `j` have integer type, then we can rewrite `E1` as `(p + i) + j`. If `j` is an integer constant, this allows `j` to be constant folded into the constant part `C = C1 +/- ... +/- j +/- Ck +/- ... +/- Cn`.

The following examples illustrate the normalizations that are currently supported in bounds validation.

### Example 2: Bounds Widening

```
void bounds_widening_increment(_Nt_array_ptr<char> p : count(len), unsigned int len) {
  if (*(p + len)) {
    // The (widened) observed bounds of p before ++len are bounds(p, p + (len + 1)).
    // The observed bounds of p after ++len are bounds(p, p + ((len - 1) + 1)).
    // We can rewrite p + ((len - 1) + 1) as (p + len) + (-1 + 1) using left-associativity
    // and apply constant folding to obtain p + len.
    // The observed bounds of bounds(p, p + ((len - 1) + 1)) imply the declared bounds of
    // bounds(p, p + len).
    ++len;
  }
}
```

### Example 3: Constant Folding with Addition and Subtraction

```
void constant_folding_addition(int i, _Array_ptr<int> p : count(i)) {
  // i + 3 - 2 - 1 == i + 0 == i
  _Array_ptr<int> q : bounds(p, p + i + 3 - 2 - 1) = 0;
  q = p;
}
```

### Example 4: Left-Associativity

```
void associativity(_Array_ptr<int> p : bounds(p, (p + i) + j), unsigned int i, unsigned int j) {
  // (p + i) + j == p + (i + j)
  _Array_ptr<int> q : bounds(p, p + (i + j)) = 0;
  q = p;
}
```

## Future Work

The following examples describe some of the current limitations of bounds validation and illustrate the ways in which bounds validation should work in the future.

### Equality Using Arithmetic

The bounds checker should be able to use mathematical properties and operations such as constant folding and commutativity to determine when two bounds expressions are equivalent.

#### Example 5: Constant Folding with Multiplication

```
void constant_folding_multiplication_one(int i, _Array_ptr<int> p : count(i)) {
  // (2 - 1) * i == 1 * i == i
  _Array_ptr<int> q : count((2 - 1) * i) = 0;
  p = q;
}
```

```
void constant_folding_multiplication_zero(int i, _Array_ptr<int> p : count(0 * i)) {
  // 0 * i == 0
  // 1 > 0
  _Array_ptr<int> q : count(1) = 0;
  p = q;
}
```

#### Example 6: Commutativity

```
void commutativity(int i, _Array_ptr<int> p : count(i + 2)) {
  // 2 + i == i + 2
  _Array_ptr<int> q : count(2 + i) = 0;
  p = q;

  // i + r + 2 == r + i + 2
  // p == r, so r + i + 2 == p + i + 2
  _Array_ptr<int> r: bounds(r, i + r + 2) = 0;
  p = r;
}
```

### Axioms

The bounds checker should be able to use the following axioms to prove the validity of bounds expressions:

1. For any expression `e` and positive integer `i`, `e + i > e` (assuming `e + i` does not overflow).
2. For any expression `e` and positive integer `i`, `e - i < e` (assuming `e - i` does not overflow).
3. For any expressions `e1` and `e2`, `e1 - e2 == e1 + -e2` (assuming `e1 + -e2` does not overflow).
4. For any expression `e`, `e - e == 0` (assuming `e - e` does not overflow).

#### Example 7: Adding a Positive Integer

```
void add_positive_integer(unsigned int i, _Array_ptr<int> p : count(i)) {
  // i + 1 > i
  _Array_ptr<int> q : count(i + 1) = 0;
  p = q;
}
```

```
void compare_positive_integers(unsigned int i, _Array_ptr<int> p : count(i + 1)) {
  // i + 2 > i + 1
  _Array_ptr<int> q : count(i + 2) = 0;
  p = q;
}
```

#### Example 8: Subtracting a Positive Integer

```
void subtract_positive_integer(unsigned int i, _Array_ptr<int> p : count(i - 1)) {
  // i > i - 1
  _Array_ptr<int> q : count(i) = 0;
  p = q;
}
```

#### Example 9: Adding a Negative Integer

```
void add_negative_integer(unsigned int i, _Array_ptr<int> p : count(i - 2)) {
  // i + -2 == i - 2
  _Array_ptr<int> q : count(i + -2) = 0;
  p = q;
}
```

#### Example 10: Cancelling Two Expressions

```
void cancellation(unsigned int i, _Array_ptr<int> p : count(i + 3 - i)) {
  // i + 3 - i == 3
  _Array_ptr<int> q : count(3) = 0;
  p = q;
}
```

### More Sophisticated Uses of Equivalent Expressions

The bounds checker should be able to determine more information from the equivalent expressions by making substitutions.

#### Example 11: Replacing One Variable With a Constant

```
void single_var_const_substitution(_Array_ptr<int> p : bounds(p, p + 3)) {
  // i == 2 + 1, so i == 3
  int i = 2 + 1;
  _Array_ptr<int> q : count(i) = 0;
  p = q;

  // i == 4, so i > 3
  int j = 4;
  _Array_ptr<int> r : count(j) = 0;
  p = r;
}
```

#### Example 12: Replacing Multiple Variables With Constants

```
void multi_var_const_substitution(void) {
  // j == 3 and i == 2 and 3 > 2, so j > i
  int i = 2;
  int j = 3;
  _Array_ptr<int> p : count(i) = 0;
  _Array_ptr<int> q : count(j) = 0;
  p = q;
}
```

#### Example 13: Replacing Variables With Non-Constant Expressions

```
void var_substitution(_Array_ptr<int> p : count(i), int i) {
  // j == i + 1 and i + 1 > i, so j > i
  int j = i + 1;
  _Array_ptr<int> q : count(j) = 0;
  p = q;
}
```

#### Example 14: More Complicated Substitutions

```
void multiple_substitutions(_Array_ptr<int> p : count(i + j), int i, int j) {
  int k = 2;
  int x = i + k;
  int y = x + j;
  // y == x + j and x == i + k, so y == i + k + j
  // y == i + k + j == i + j + k
  // k == 2, so k > 0, so i + j + k > i + j
  _Array_ptr<int> q : count(y) = 0;
  p = q;
}
```

