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
elements of the sequence are implicitly read (for example via call to `strlen`
function). In the example below the bounds of the null-terminated-array `p` are
widened to `bounds(p, p + x)`.
```
  void f(_Nt_array_ptr<char> p) {
    int x = strlen(p) _Where p : bounds(p, p + x);
  }
```

In the next section we describe a dataflow analysis to widen bounds for
null-terminated arrays. The dataflow analysis is **forward**,
**path-sensitive**, **flow-sensitive** and **intra-procedural**.

## Dataflow Analysis for Widening the Bounds of Null-terminated Arrays

We use `V` to denote a null-terminated array variable, `bounds(Lower, Upper)`
to denote the bounds expression for `V`, `S` to denote a statement, and `B` and
`B'` to denote basic blocks.

Note: Two variables having the same name but different declarations are treated
distinctly by the analysis.

The dataflow analysis tracks all null-terminated array variables in a function
along with their bounds expressions. The dataflow facts that flow through the
analysis are sets of pairs `V:bounds(Lower, Upper)`.

For every basic block `B`, we compute the sets `In[B]` and `Out[B]`.

For every statement `S`, we compute the sets `Gen[S]`, `Kill[S]`, `StmtIn[S]`
and `StmtOut[S]`.

The sets `In[B]`, `Out[B]`, `StmtIn[S]` and `StmtOut[S]` are part of the
fixed-point computation whereas the sets `Gen[S]` and `Kill[S]` are computed
**before** the fixed-point computation.

### Gen[S]
`Gen[S]` maps each null-terminated array variable `V` that occurs in statement
`S` to a bounds expression comprising a lower bound, and an upper bound to
which `V` may potentially be widened.

Dataflow equation:
```
Let the initial value of Gen[S] be ∅.

If S declares bounds(Lower, Upper) as bounds of V:
  Gen[S] = Gen[S] ∪ {V:bounds(Lower, Upper)}

Else if S is the terminating condition for block B:
  Let V have declared bounds as bounds(Lower, Upper).
  If S dereferences V at E:
    Gen[S] = Gen[S] ∪ {V:bounds(Lower, E + 1)}

Else if W is a where_clause ∧
        W annotates S ∧
        W declares bounds(Lower, Upper) as bounds of V:
  Gen[S] = Gen[S] ∪ {V:bounds(Lower, Upper)}
```
For each variable `Z` we maintain a set of all the null-terminated array
variables in whose bounds expressions `Z` occurs. This is used in the
computation of the `Kill` sets.
```
Let the initial value of BoundsVars[Z] be ∅.

∀ V:bounds(Lower, Upper) ∈ Gen[S],
  ∀ variables Z occurring in Lower or Upper,
    BoundsVars[Z] = BoundsVars[Z] ∪ {V}
```

### Kill[S]
`Kill[S]` denotes the set of null-terminated arrays whose bounds are killed by
the statement `S`.

Dataflow equation:
```
Let the initial value of Kill[S] be ∅.

If V:bounds(Lower, Upper) ∈ Gen[S]:
  Kill[S] = Kill[S] ∪ {V}

If S assigns to Z ∧ Z is a variable ∧ Z ∈ keys(BoundsVars):
  Kill[S] = Kill[S] ∪ BoundsVars[Z]
```

### StmtIn[S]
`StmtIn[S]` denotes the mapping between null-terminated array variables and
their widened bounds expressions at the start of statement `S`.

Dataflow equation:
```
If S is the first statement of B:
  StmtIn[S] = In[B]
Else:
  StmtIn[S] = StmtOut[S'], where S' ∈ pred(S)
```

### StmtOut[S]
`StmtOut[S]` denotes the mapping between null-terminated array variables and
their widened bounds expressions at the end of statement `S`.

Dataflow equation:
```
StmtOut[S] = (StmtIn[S] - Kill[S]) ∪ Gen[S]
```

### Out[B]
`Out[B]` denotes the mapping between null-terminated array variables and their
widened bounds expressions at the end of block `B`.

Dataflow equation:
```
If block B contains one or more statements:
  Let S be the last statement in block B.
  Out[B] = StmtOut[S]
Else:
  Out[B] = In[B]
```

### In[B]
`In[B]` denotes the mapping between null-terminated array variables and their
widened bounds expressions upon entry to block `B`. The `In` set for a block
`B` is computed as the intersection of the `Out` sets of all the predecessor
blocks of `B`.

We define the intersection operation on sets of dataflow facts as follows:
```
For two sets of dataflow facts D1 and D2,
∀ V:bounds(Li, Ui) ∈ D1 ∧ V:bounds(Lj, Uj) ∈ D2,

  If bounds(Li, Ui) is Top:
    V:bounds(Lj, Uj) ∈ D1 ∩ D2
  Else if bounds(Lj, Uj) is Top:
    V:bounds(Li, Ui) ∈ D1 ∩ D2
  Else if range(Li, Ui) is a subrange of range(Lj, Uj):
    V:bounds(Li, Ui) ∈ D1 ∩ D2
  Else if range(Lj, Uj) is a subrange of range(Li, Ui):
    V:bounds(Lj, Uj) ∈ D1 ∩ D2
```

Note: `Top` is defined later in this document.

Dataflow equation:
```
∀ B' ∈ pred(B),
  Let S be the terminating condition for block B'.

  If S dereferences V:
    Let bounds(Lower, Upper) be the the bounds of V in StmtIn[S].
    
    If S dereferences V at Upper ∧ edge(B',B) is a true edge:
      // Only on a true edge we know that the element dereferenced at Upper is
      // non-null.
      In[B] = In[B] ∩ Out[B']
    Else:
      In[B] = In[B] ∩ ((Out[B'] - Gen[S]) ∪ StmtIn[S])
  Else:
    In[B] = In[B] ∩ Out[B']
```

### Initial values of `In[B]` and `Out[B]`
As we saw above, `In[B]` is computed as the intersection of all the predecessor
blocks of `B`. If the initial values of `In[B]` and `Out[B]` are `∅` then the
intersection would always produce an empty set and we would not be able to
propagate the widened bounds expression for any null-terminated array. For
example:
```
void f(_Nt_array_ptr<char> p) {
  // If the Out set for back edge of the loop is ∅, then the In set of the loop
  // will always be ∅.
  while (*p) {
    // do something
  }
}
```

We maintain a set of all null-terminated array variables in the function and
use it to initialize the `In` and `Out` sets for blocks.
```
Let the initial value of AllVars be ∅.

∀ V in function F,
  AllVars = AllVars ∪ {V}
```

Thus, we initialize `In[B]` and `Out[B]` to `{AllVars:Top}`.
```
∀ blocks B,
  In[B] = Out[B] = {AllVars:Top | Top is a special bounds expression}
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

Thus, the `Out` set of the `Entry` block is propagated to all blocks whose
predecessors do not widen the bounds of **any** null-terminated array. So in
this case we want the intersection of `Out` blocks to be an empty set. To
handle this case we initialize the `In` and `Out` sets of the `Entry` block to
`∅`.
```
In[Entry] = Out[Entry] = ∅
```

### Testing the analysis
For quick reference, we reproduce here the dataflow equation for the
computation of the `In` sets for basic blocks. We annotate (as comments) the
various conditions of the equation and refer to them in the test cases below.
```
∀ B' ∈ pred(B),
  Let S be the terminating condition for block B'.

  If S dereferences V:             // I
    Let bounds(Lower, Upper) be the the bounds of V in StmtIn[S].
    
    If S dereferences V at Upper ∧ // II
       edge(B',B) is a true edge:  // III
      // Only on a true edge we know that the element dereferenced at Upper is
      // non-null.
      In[B] = In[B] ∩ Out[B']
    Else:                          // IV
      In[B] = In[B] ∩ ((Out[B'] - Gen[S]) u StmtIn[S])
  Else:                            // V
    In[B] = In[B] ∩ Out[B']
```

#### Test Cases
```
_Nt_array_ptr<char> p : bounds(p, p + 1);
int x = strlen(p) _Where p : bounds(p, p + x);
if (1) {               // Tests V
} else if (*(p + 1)) { // Tests I, IV
} else if (*(p + x)) { // Tests I, II, III
} else {               // Tests I, IV
}

_Nt_array_ptr<char> p : bounds(p, p + 1);
int x = strlen(p) _Where p : bounds(p, p + x);
if (*(p + x)) {        // Tests I, II, III
  if (*(p + x + 1)) {  // Tests I, II, III
  }
}

_Nt_array_ptr<char> p : bounds(p, p + 1);
int x = strlen(p) _Where p : bounds(p, p + x);
if (*(p + x)) {        // Tests I, II, III
  x = 10;
  if (*(p + x + 1)) {  // Tests I, IV
  }
}

_Nt_array_ptr<char> p : bounds(p, p);
if (*p) {           // Tests I, II, III
  if (*(p + 1)) {   // Tests I, II, III
    p = 0;
    if (*(p + 2)) { // Tests I, IV
    }
  }
}

_Nt_array_ptr<char> p : bounds(p, p);
if (*p) { // Tests I, II, III
}

_Nt_array_ptr<char> p : bounds(p, p + 1);
if (*(p + 1)) { // Tests I, II, III
}

_Nt_array_ptr<char> p : bounds(p, p);
if (*(p + 1)) { // Tests I, IV
}

_Nt_array_ptr<char> p : bounds(p, p + 1);
if (*p) { // Tests I, IV
}
```
