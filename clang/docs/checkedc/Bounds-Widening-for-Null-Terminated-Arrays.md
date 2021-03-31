# Bounds Widening for Null-terminated Arrays

## What are Null-terminated Arrays?
A null-terminated array is a sequence of elements in memory that ends with a
null terminator. Checked C adds the type `_Nt_array_ptr<T>` to represent
pointers to these kinds of arrays. These arrays can be divided into two parts:
a prefix with bounds and a sequence of additional elements that ends with a
null terminator.

An important property of a null-terminated array is that the initial elements
of the sequence can be read, provided that preceding elements are not the null
terminator. This gives rise to the following observation:
**The bounds of a null-terminated array can be widened based on the number of
elements read.**

The bounds of a null-terminated array can also be widened using a where clause.
In the example below the bounds of the null-terminated-array `p` are widened to
`bounds(p, p + x)`.
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

Only the sets `In[B]` and `Out[B]` are part of the fixed-point computation. The
sets `Gen[S]` and `Kill[S]` are computed **before** the fixed-point computation
while the sets `StmtIn[S]` and `StmtOut[S]` are computed **after** the
fixed-point computation.

The widened bounds offset `X` could either be an integer constant or a integer
variable. If the bounds of a null-terminated array `V` are `bounds(V + Low, V +
High)` then `X` represents the widened bounds offset such that the widened
bounds for `V` are `bounds(V + Low, V + High + X)`.

### Gen[S]
`Gen[S]` maps each null-terminated array variable `V` that occurs in statement
`S` to the bounds offset to which `V` might be possibly may be widened.

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

We define the intersection of the `Out` sets as follows:
```
For basic blocks Bi and Bj, let Out[Bi] ∩ Out[Bj] = ∅, then
∀ V:Xi ∈ Out[Bi] ∧ V:Xj ∈ Out[Bj],
  If both Xi and Xj are integer constants:
    Out[Bi] ∩ Out[Bj] = Out[Bi] ∩ Out[Bj] ∪ {V:min(Xi, Xj)}
  Else:
    Out[Bi] ∩ Out[Bj] = Out[Bi] ∩ Out[Bj]
```

Alternate definition of intersection:
```
For basic blocks Bi and Bj,
∀ V:Xi ∈ Out[Bi] ∧ V:Xj ∈ Out[Bj],
  If both Xi and Xj are integer constants:
    V:min(Xi, Xj) ∈ Out[Bi] ∩ Out[Bj]
```

Dataflow equation:
```
∀ B' ∈ pred(B),
  If S is the terminating condition for block B' ∧ S dereferences V at upper_bound(V):
    If element at upper_bound(V) is provably non-null:
      In[B] = In[B] ∩ Out[B']
    Else:
      In[B] = In[B] ∩ (Out[B'] - Gen[S])
```

### Out[B]
`Out[B]` denotes the mapping between null-terminated arrays and their widened
bounds offsets at the end of block `B`.

Dataflow equation:
```
∀ statements S ∈ B, i ∈ {1,...,n},
  let Kill[B] = ∪ Kill[Si]
  let Gen[B] = ∪ Gen[Si]

  Out[B] = (In[B] - Kill[B]) ∪ Gen[B]
```

### StmtIn[S]
`StmtIn[S]` denotes the mapping between null-terminated arrays and their widened
bounds offsets at the start of statement `S`.

Dataflow equation:
```
If S is first_statement(B):
  StmtIn[S] = In[B]
Else:
  StmtIn[S] = StmtOut[S'], where S' ∈ pred(S)
```

### StmtOut[S]
`StmtOut[S]` denotes the mapping between null-terminated arrays and their
widened bounds offsets at the end of statement `S`.

Dataflow equation:
```
StmtOut[S] = (StmtIn[S] - Kill[S]) ∪ Gen[S]
```

### Initial values of `In[B]` and `Out[B]`
From the dataflow equations defined above, we know that:
```
∀ B' ∈ pred(B),
  In[B] = In[B] ∩ Out[B']
```

But what are the initial values of the `In[B]` and `Out[B]`? If their initial
values are `∅` then the intersection would always produce an empty set and we
would not be able to propagate the widened bounds for any null-terminated
array.

So we initialize the initial values of `In[B]` and `Out[B]` to `Gen[B]` that
denotes the union of the `Gen` sets for all statements in a block.
```
∀ statements S ∈ B, i ∈ {1,...,n},
Gen[B] = Gen[B] ∪ Gen[Si]
```

We define the union operation used above as follows:
```
For statements Si and Sj, let Gen[Si] = {V:Xi, W:Y} and Gen[Sj] = {V:Xj, U:Z},
If Xi > Xj:
  Gen[Si] ∪ Gen[Sj] = {V:Xi, W:Y, U:Z}
Else:
  Gen[Si] ∪ Gen[Sj] = {V:Xj, W:Y, U:Z}
```

Thus, for all blocks we have the following initial values for `In[B]` and
`Out[B]`:
```
In[B] = Out[B] = Gen[B]
```

Now, we also need to handle the case where there is an unconditional jump into
a block (for example, as a result of a `goto`). In this case, we cannot widen
the bounds because we cannot provably infer that the element at upper_bound(V) is non-null on the unconditional edge. So in this case we want the intersection (and hence the `In` set) to be an empty set. To handle this case we initialize the `In` and `Out` sets of the `Entry` block to `∅`.

Thus, we have the following initial values for `In[Entry]` and `Out[Entry]`:
```
In[Entry] = Out[Entry] = ∅
```

## Implementation Details
The main class that implements the analysis is
[`BoundsAnalysis`](https://github.com/microsoft/checkedc-clang/blob/master/clang/lib/Sema/BoundsAnalysis.cpp)
and the main function is `BoundsAnalysis::WidenBounds()`.

`WidenBounds` will perform the bounds widening for the entire function. We can
then call `BoundsAnalysis::GetWidenedBounds` to retrieve the widened bounds for
the current basic block.

The approach used for implementing the analysis is the iterative worklist
algorithm in which we keep adding blocks to a worklist as long as we do not
reach a fixed point i.e.: as long as the `Out` sets for the blocks keep changing.

### Algorithm
```
1.  For the current function F:
2.    For each basic block B in the reverse post-order for F:
3.      Compute the Kill set for B
4.      For each predecessor B' of B:
5.        Compute the Gen set on edge B'->B
6.      Add B to a queue called WorkList

7.    For each basic block in WorkList:
8.      Compute the In set for B
9.      For each successor B' of B:
10.       Store the current Out set Out[B][B'] as OldOut
11.       Compute the new Out set on edge B->B'
12.       Add B' to WorkList if Out[B][B'] != OldOut
```

## Debugging the Analysis
In order to debug the bounds widening anlaysis, you can use the clang flag
`-fdump-widened-bounds`. This will dump the function name, the basic blocks
sorted by block ID, and for each `_Nt_array_ptr` in the block the variable name
and its widened bounds, if applicable.
