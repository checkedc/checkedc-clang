# Bounds Widening for Null-terminated Arrays

## Null-terminated Arrays
A null-terminated array is a sequence of elements in memory that ends with a
null terminator. Checked C adds the type `_Nt_array_ptr<T>` to represent
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
null-terminated arrays. The dataflow analysis is forward, path-sensitive,
flow-sensitive and intra-procedural.

## Dataflow Analysis for Widening the Bounds of Null-terminated Arrays
We use `V` to denote a null-terminated array variable, `X` to denote a widened
bounds offset, `S` to denote a statement, and `B` and `B'` to denote basic
blocks.

The dataflow analysis tracks all null-terminated array variables in a function
along with their widened bounds. The dataflow facts that flow through the
analysis are a set of pairs `V:X` such that `V` is a null-terminated array
variable and `X` is the widened bounds offset for `V`.

For every basic block `B`, we compute the sets `In[B]` and `Out[B]`.

For every statement `S`, we compute the sets `Gen[S]`, `Kill[S]`, `StmtIn[S]`
and `StmtOut[S]`.

The sets `In[B]`, `Out[B]`, `StmtIn[S]` and `StmtOut[S]` are part of the
fixed-point computation whereas the sets `Gen[S]` and `Kill[S]` are computed
**before** the fixed-point computation.

The widened bounds offset `X` could either be an integer constant or a integer
variable. If the bounds of a null-terminated array `V` are `bounds(V + Low, V +
High)` then `X` represents the widened bounds offset such that the widened
bounds for `V` are `bounds(V + Low, V + High + X)`.

### Gen[S]
`Gen[S]` maps each null-terminated array variable `V` that occurs in statement
`S` to the bounds offset to which `V` might be possibly widened.

Dataflow equation:
```
If S is the terminating condition for block B:
  If S dereferences at (upper_bound(V) + X) ∧ X is an integer constant:
    Gen[S] = Gen[S] ∪ {V:X}

Else if W is a where_clause ∧ W annotates S ∧ W declares V : bounds(V, V + X):
  Gen[S] = Gen[S] ∪ {V:X}
```

### Kill[S]
`Kill[S]` denotes the set of null-terminated arrays whose bounds are killed by
the statement `S`.

Dataflow equation:
```
If S assigns to V ∨
   S assigns to Z ∧ Z is a variable ∧ Z is used in bounds(V) ∨
   W is a where_clause ∧ W annotates S ∧ W declares bounds(V):
  Kill[S] = Kill[S] ∪ {V}
```

### In[B]
`In[B]` denotes the mapping between null-terminated array variables and their
widened bounds offsets upon entry to block `B`. The `In` set for a block `B` is
computed as the intersection of the `Out` sets of all the predecessor blocks of
`B`.

We define the intersection operation on sets of dataflow facts as follows:
```
For two sets of dataflow facts D1 and D2,
∀ V:Xi ∈ D1 ∧ V:Xj ∈ D2,
  If both Xi and Xj are integer constants:
    V:min(Xi, Xj) ∈ D1 ∩ D2
  Else if Xi is <special_top_value>:
    V:Xj ∈ D1 ∩ D2
  Else if Xj is <special_top_value>:
    V:Xi ∈ D1 ∩ D2

  // Note: If both Xi and Xj are variables or if one is a variable and the
  //       other is an integer constant then neither V:Xi nor V:Xj belong
  //       to D1 ∩ D2.
```

Dataflow equation:
```
∀ B' ∈ pred(B),
  Let S be the terminating condition for block B'.

  If S dereferences V at current upper_bound(V):
    If element at upper_bound(V) is provably non-null:
      In[B] = In[B] ∩ Out[B']
    Else:
      In[B] = In[B] ∩ (Out[B'] - Gen[S])
  Else:
    In[B] = In[B] ∩ Out[B']
```

### Out[B]
`Out[B]` denotes the mapping between null-terminated array variables and their
widened bounds offsets at the end of block `B`.

Dataflow equation:
```
Let S be the last statement in block B.
Out[B] = StmtOut[S]
```

### StmtIn[S]
`StmtIn[S]` denotes the mapping between null-terminated array variables and
their widened bounds offsets at the start of statement `S`.

Dataflow equation:
```
If S is first_statement(B):
  StmtIn[S] = In[B]
Else:
  StmtIn[S] = StmtOut[S'], where S' ∈ pred(S)
```

### StmtOut[S]
`StmtOut[S]` denotes the mapping between null-terminated array variables and
their widened bounds offsets at the end of statement `S`.

Dataflow equation:
```
StmtOut[S] = (StmtIn[S] - Kill[S]) ∪ Gen[S]
```

### Initial values of `In[B]` and `Out[B]`
As we saw above, `In[B]` is computed as the intersection of all the predecessor
blocks of `B`. If the initial values of `In[B]` and `Out[B]` are `∅` then the
intersection would always produce an empty set and we would not be able to
propagate the widened bounds for any null-terminated array.

So we initialize the initial values of `In[B]` and `Out[B]` to `Top`.
```
∀ blocks B,
  In[B] = Out[B] = Top
```

We define `Top` as follows:
```
Top = {V:<special_top_value> | V is a null-terminated array variable in the function}
```

Now, we also need to handle the case where there is an unconditional jump into
a block. In this case, we cannot widen the bounds because we cannot provably
infer that the element at `upper_bound(V)`
is non-null on the unconditional edge. For example:
```
void f(_Nt_array_ptr<int> p, int x) {
  if (condition) {
    x = strlen(p) _Where p : bounds(p, p + x);
  g(x); // p is not widened on all paths into this block.
}
```

Thus the `Out` set of the `Entry` block is propagated to all blocks whose
predecessors do not widen the bounds of **any** null-terminated array. So in
this case we want the intersection of `Out` blocks to be an empty set. To
handle this case we initialize the `In` and `Out` sets of the `Entry` block to
`∅`.
```
In[Entry] = Out[Entry] = ∅
```

## Implementation Details
The main class that implements the analysis is
[`BoundsAnalysis`](https://github.com/microsoft/checkedc-clang/blob/master/clang/lib/Sema/BoundsAnalysis.cpp).

## Debugging the Analysis
In order to debug the bounds widening anlaysis, you can use the clang flag
`-fdump-widened-bounds`. This will dump the function name, the basic blocks
sorted by block ID, and for each the variable name and the widened bounds for
each null-terminated array in the block.
