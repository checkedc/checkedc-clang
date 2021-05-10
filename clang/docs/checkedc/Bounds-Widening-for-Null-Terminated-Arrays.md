# Bounds Widening for Null-terminated Arrays

## Null-terminated Arrays
A null-terminated array is a sequence of elements in memory that ends with a
null element. Checked C adds the type `_Nt_array_ptr<T>` to represent
pointers to these kinds of arrays. Each such array can be divided into two
parts: a prefix with bounds and a sequence of additional elements that ends
with a null terminator.

An important property of a null-terminated array is that the initial elements
of the sequence can be read, provided that preceding elements are not the null
terminator. This gives rise to the following observation:
**The bounds of a null-terminated array can be widened based on the number of
elements read.**

The bounds of a null-terminated array can also be widened when the initial
elements of the sequence are known to be read by a function call (for example
via call to `strlen` function). In the example below the bounds of the
null-terminated-array `p` are widened to `bounds(p, p + x)`.

```
  void f(_Nt_array_ptr<char> p) {
    int x = strlen(p) _Where p : bounds(p, p + x);
  }
```

In the next section we describe a dataflow analysis to widen bounds for
null-terminated arrays. The dataflow analysis is **forward**,
**path-sensitive**, **flow-sensitive** and **intra-procedural**.

Note: This analysis assumes that the bounds declarations in where clauses are
valid. The validity needs to be proven elsewhere.

## Dataflow Analysis for Widening the Bounds of Null-terminated Arrays
We use `V` to denote a variable that is a pointer to a null-terminated array,
`bounds(Lower, Upper)` to denote the bounds expression for `V`, `S` to denote a
statement, and `B` and `B'` to denote basic blocks.

Note: The analysis treats two variables having the same name but different
declarations as distinct from each other.

The dataflow analysis tracks all variables that are pointers to null-terminated
arrays local to a function along with their bounds expressions. The dataflow
facts that flow through the analysis are sets of pairs `V:bounds(Lower,
Upper)`.

For every basic block `B`, we compute the sets `Kill[B]`, `Gen[B]`, `In[B]` and
`Out[B]`.

For every statement `S`, we compute the sets `Kill[S]` and `Gen[S]`.

The sets `In[B]` and `Out[B]` are part of the fixed-point computation, whereas
the sets `Kill[B]`, `Gen[B]`, `Kill[S]` and `Gen[S]` are computed **before**
the fixed-point computation.

### Initial operations
In each function, we map a variable `Z` to the set of all variables that are
pointers to null-terminated arrays and in whose bounds expressions `Z` occurs.
We maintain this map only for variables that are mapped to non-empty sets.
```
∀ variables Z in function F, let the initial value of BoundsVars[Z] be ∅.

∀ statements S,
  If V:bounds(Lower, Upper) is either declared or specified as a where clause fact:
    ∀ variables Z occurring in Lower or Upper,
      BoundsVars[Z] = BoundsVars[Z] ∪ {V}
```

### Kill[B]
`Kill[B]` denotes the set of variables that are pointers to null-terminated
arrays and whose bounds are killed in block `B`.

Dataflow equation:
```
Kill[B] = Kill[S_1] ∪ Kill[S_2] ∪ ... ∪ Kill[S_n]
```
The `Kill` set for each statement `S` denotes the set of variables that are
pointers to null-terminated arrays and whose bounds are killed by `S`.
`Kill[S]` is computed as follows:
```
If S declares bounds of V or
   S is the terminating condition for block B and S dereferences V or
   W is a where_clause and W annotates S and W declares bounds of V:
  Kill[S] = Kill[S] ∪ {V}

Else if S modifies Z and Z is a variable and Z ∈ keys(BoundsVars):
  Kill[S] = Kill[S] ∪ BoundsVars[Z]
```
Note: We currently only track modifications to variables that occur in bounds
expressions local to a basic block.

### Gen[B]
`Gen[B]` denotes the mapping between variables that are pointers to
null-terminated arrays whose bounds may potentially be widened in block `B`,
and their widened bounds expressions.

Dataflow equation:
```
Gen[B] = Gen[S_n] ∪ (Gen[S_n-1] - Kill[S_n]) ∪
        (Gen[S_n-2] - Kill[S_n-1] - Kill[S_n]) ∪
        ... ∪ (Gen[S_1] - Kill[S_2] - Kill[S_3] - ... - Kill[S_n])
```
The `Gen` set for each statement `S` maps each variable that is a pointer to a
null-terminated array `V` that occurs in `S` to a bounds expression comprising
a lower bound, and an upper bound to which `V` may potentially be widened.
`Gen[S]` is computed as follows:
```
If S declares bounds(Lower, Upper) as bounds of V:
  Gen[S] = Gen[S] ∪ {V:bounds(Lower, Upper)}

Else if S is the terminating condition for block B:
  Let V have declared bounds as bounds(Lower, Upper).
  If S dereferences V in an expression E:
    Gen[S] = Gen[S] ∪ {V:bounds(Lower, E + 1)}

Else if W is a where_clause and W annotates S and
        W declares bounds(Lower, Upper) as bounds of V:
  Gen[S] = Gen[S] ∪ {V:bounds(Lower, Upper)}
```
Note: Currently, we only consider where clauses that annotate calls to `strlen`
and `strnlen`.

### Out[B]
`Out[B]` denotes the mapping between variables that are pointers to
null-terminated arrays and their widened bounds expressions at the end of block
`B`.

Dataflow equation:
```
Out[B] = (In[B] - Kill[B]) ∪ Gen[B]
```

### In[B]
`In[B]` denotes the mapping between variables that are pointers to
null-terminated arrays and their widened bounds expressions upon entry to block
`B`. The `In` set for a block `B` is computed as the intersection of the `Out`
sets of all the predecessor blocks of `B`.

Note 1: We define the intersection operation on sets of dataflow facts as
follows:
```
For two sets of dataflow facts D1 and D2,
  If V:bounds(Li, Ui) ∈ D1 and V:bounds(Lj, Uj) ∈ D2:
    If bounds(Li, Ui) is Top:
      V:bounds(Lj, Uj) ∈ D1 ∩ D2
    Else if bounds(Lj, Uj) is Top:
      V:bounds(Li, Ui) ∈ D1 ∩ D2
    Else if range(Li, Ui) is a subrange of range(Lj, Uj):
      V:bounds(Li, Ui) ∈ D1 ∩ D2
    Else if range(Lj, Uj) is a subrange of range(Li, Ui):
      V:bounds(Lj, Uj) ∈ D1 ∩ D2
```
Note 2: `Top` is defined later in this document.

Note 3: `StmtIn[S]` is computed as follows:
```
Let S_i denote the i^th statement in block B.

StmtIn[S_1] = In[B]

StmtIn[S_i] = Gen[S_i-1] ∪ (Gen[S_i-2] - Kill[S_i-1]) ∪
             (Gen[S_i-3] - Kill[S_i-2] - Kill[S_i-1]) ∪
              ... ∪ (Gen[S_1] - Kill[S_2] - Kill[S_3] - ... - Kill[S_i-1])
```
Dataflow equation:
```
∀ B' ∈ pred(B),
  Let S be the terminating condition for block B'.

  If S dereferences V:                           // Case A
    Let bounds(Lower, Upper) be the the bounds of V in StmtIn[S].

    If S dereferences V at Upper and            // Case B
       edge(B', B) is a true edge:              // Case C
      // On a true edge, we can infer that the element dereferenced at Upper is
      // non-null.
      In[B] = In[B] ∩ Out[B']
    Else:                                       // Case D
      Let V:X ∈ Out[B'] and V:X' ∈ StmtIn[S]
      In[B] = In[B] ∩ ((Out[B'] - {V:X}) ∪ {V:X'}
  Else:                                         // Case E
    In[B] = In[B] ∩ Out[B']
```

### Widened bounds at each statement
Once the analysis reaches a fixpoint, we can get the widened bounds for each
statement in a final pass over the function:
```
Widened bounds at statement S = (StmtIn[S] - Kill[S]) ∪ Gen[S]
```

### Initial values of `In[B]` and `Out[B]`
As we saw above, `In[B]` is computed as the intersection of all the predecessor
blocks of `B`. If the initial values of `In[B]` and `Out[B]` are `∅` then the
intersection would always produce an empty set and we would not be able to
propagate the widened bounds for any null-terminated array. For example:
```
void f(_Nt_array_ptr<char> p) {
  // If the Out set for the back edge of the loop is ∅, then the In set of the
  // loop will always be ∅.
  while (*p) {
    // do something
  }
}
```

So we maintain the set of all variables that are pointers to null-terminated
arrays in the function and use it to initialize the `In` and `Out` sets for
blocks.
```
Let the initial value of AllVars be ∅.

∀ V in function F,
  AllVars = AllVars ∪ {V}
```

Thus, we initialize `In[B]` and `Out[B]` as follows:
```
∀ blocks B,
  In[B] = Out[B] = {V:Top | V ∈ AllVars and Top is a special bounds expression}
```

Now, we also need to handle the case where there is an unconditional jump into
a block. In this case, we cannot widen the bounds because we cannot provably
infer that the element at `upper_bound(V)`
is non-null on the unconditional edge. For example:
```
void f(_Nt_array_ptr<char> p, int x) {
  if (condition) {
    x = strlen(p) _Where p : bounds(p, p + x);
  g(x); // p is not widened on all paths into this block.
}
```

Thus, the `Out` set of the `Entry` block is propagated to all blocks that have
at least one predecessor that does not widen the bounds of **any**
null-terminated array. So in this case we want the intersection of `Out` blocks
to be an empty set. To handle this case we initialize the `In` and `Out` sets
of the `Entry` block to `∅`.
```
In[Entry] = Out[Entry] = ∅
```

We also initialize the `In` and `Out` sets of all unreachable basic blocks to
`∅`.

### Whose bounds may be widened by a dereference expression?
Below is an algorithm to determine the variables whose bounds may be
potentially widened on a dereference expression of the form `if (*e)`. This is
used in the computation of the `Gen[S]` and `Kill[S]` sets to determine `V` in
the condition `If S dereferences V at E`.
```
1. Traverse the dereference expression and gather the set of variables (say A) that occur in the expression.
2. For each variable V in set A:
  2.a. Gather the set of variables (say B) in whose upper bounds expressions V occurs.
3. Compute the intersection of all sets Bi. Let's call the resultant set as C.
4. For each variable W in set C:
  4.a. Find the set of variables (say D) that occur in the upper bounds expression of W.
  4.b. For each set Di:
    4.b.1. If set Di equals set A, the variable W may potentially be widened.
```
Example:
```
void f(int x) {
  Nt_array_ptr<T> p : bounds(p, p + x);
  Nt_array_ptr<T> q : bounds(p, p + x + y);
  
  if (*(p + x)) // Whose bounds may be potentially widened here?
}

Algorithm:
1. The set of variables that occur in the dereference expression "if (*(p + x))" is A = {p, x}.
2. Traverse each variable in set A = {p, x}:
   p occurs in the upper bounds expression of B1 = {p, q}.
   x occurs in the upper bounds expression of B2 = {p, q}.
3. The intersection of B1 and B2 is C = {p, q}.
4. Traverse each variable in set C = {p, q}:
   1. The set of variables that occur in the upper bounds expression of p is D1 = {p, x}
      Set D1 == Set A:
        The bounds of p may be potentially widened.
   2. The set of variables that occur in the upper bounds expression of q is D2 = {p, x, y}
      Set D2 != Set A:
        The bounds of q may not be potentially widened.
```

### Testing the analysis
In the test cases below, we test the conditions that are part of dataflow
equation of the `In` set.

#### Test Cases
```
_Nt_array_ptr<char> p : bounds(p, p + 1);
int x = strlen(p) _Where p : bounds(p, p + x);
if (1) {                                     // Tests cases E
} else if (*(p + 1)) {                       // Tests cases A, D
} else if (*(p + x)) {                       // Tests cases A, B, C
} else {                                     // Tests cases A, D
}

_Nt_array_ptr<char> p : bounds(p, p + 1);
int x = strlen(p) _Where p : bounds(p, p + x);
if (*(p + x)) {                              // Tests cases A, B, C
  if (*(p + x + 1)) {                        // Tests cases A, B, C
  }
}

_Nt_array_ptr<char> p : bounds(p, p + 1);
int x = strlen(p) _Where p : bounds(p, p + x);
if (*(p + x)) {                              // Tests cases A, B, C
  x = 10;
  if (*(p + x + 1)) {                        // Tests cases A, D
  }
}

_Nt_array_ptr<char> p : bounds(p, p);
if (*p) {                                    // Tests cases A, B, C
  if (*(p + 1)) {                            // Tests cases A, B, C
    p = 0;
    if (*(p + 2)) {                          // Tests cases A, D
    }
  }
}

_Nt_array_ptr<char> p : bounds(p, p);
if (*p) {                                    // Tests cases A, B, C
  if (*(p + 1)) {                            // Tests cases A, B, C
    p = 0;
    if (*(p + 2)) {                          // Tests cases A, D
    }
  }
}

_Nt_array_ptr<char> p : bounds(p, p);
if (*p) {                                    // Tests cases A, B, C
}

_Nt_array_ptr<char> p : bounds(p, p + 1);
if (*(p + 1)) {                              // Tests cases A, B, C
}

_Nt_array_ptr<char> p : bounds(p, p);
if (*(p + 1)) {                              // Tests cases A, D
}

_Nt_array_ptr<char> p : bounds(p, p + 1);
if (*p) {                                    // Tests cases A, D
}
```
