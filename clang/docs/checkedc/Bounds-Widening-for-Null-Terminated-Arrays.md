# Bounds Widening for Null-terminated Arrays

## Null-terminated Arrays
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

In the next sections we describe a dataflow analysis to widen bounds for
null-terminated arrays.

## Dataflow Analysis Properties
The key properties of the dataflow analysis are described below:

1. **Forward:** The basic blocks of the function are traversed in reverse
post-order. In other words, a basic block is visited before its successors.

2. **Path-sensitive:** Path-sensitivity means that the dataflow analysis
generates different facts on the `then` and `else` branches.

3. **Flow-sensitive:** Flow-sensitivity means that the sequence of instructions
in a basic block is taken into consideration when performing the analysis.

4. **Intra-procedural:** The analysis is done on one function at a time.

## Dataflow Analysis Details
For every basic block we compute the following sets: `In` and `Kill`. The `In`
set for basic block `B` is denoted as `In[B]` and the `Kill` set is denoted as
`Kill[B]`.

For every edge we compute the following sets: `Out` and `Gen`. The `Out` set on
edge `Bi->Bj` is denoted as `Out[Bi][Bj]` and the Gen set is denoted as
`Gen[Bi][Bj]`.

### In[B]
`In[B]` stores the mapping between an `_Nt_array_ptr` and its widened bounds
inside block `B`. For example, given `_Nt_array_ptr V` with declared bounds
`(low, high)`, `In[B]` would store the mapping `{V:i}`, where `i` is an unsigned
integer and the bounds of `V` should be widened to `(low, high + i)`.

Dataflow equation:
`In[B] = ∩ Out[B*][B], where B* ∈ pred(B)`.

### Kill[B]
In block `B`, the bounds for an `_Nt_array_ptr V` are said to be killed by a
statement `S` if:
1. `V` is assigned to in `S`, or
2. a variable used in the declared bounds of `V` is assigned to in `S`.
Thus, `Kill[B]` stores the mapping between a statement `S` and `_Nt_array_ptr's`
whose bounds are killed in `S`.

### Gen[Bi][Bj]
Given `_Nt_array_ptr V` with declared bounds `(low, high)`, the bounds of `V`
can be widened by 1 if `V` is dereferenced at the upper bound. This means that
if there is an edge `Bi->Bj` whose edge condition is of the form `if (*(V +
high + i))`, where `i` is an unsigned integer offset, the widened bounds
`{V:i+1}` can be added to `Gen[Bi][Bj]`, provided we have already tested for
pointer access of the form `if (*(V + high + i - 1))`.

For example:
```
_Nt_array_ptr<T> V : bounds (low, high);
if (*V) { // Ptr dereference is NOT at the current upper bound. No bounds widening.
  if (*(V + high)) { // Ptr dereference is at the current upper bound. Widen bounds by 1. New bounds for V are (low, high + 1).
    if (*(V + high + 1)) { // Ptr dereference is at the current upper bound. Widen bounds by 1. New bounds for V are (low, high + 2).
      if (*(V + high + 3)) { // Ptr dereference is *not* at the current upper bound. No bounds widening. Flag an error!.
```

### Out[Bi][Bj]
`Out[Bi][Bj]` denotes the bounds widened by block `Bi` on edge `Bi->Bj`.

Dataflow equation:
`Out[Bi][Bj] = (In[Bi] - Kill[Bi]) ∪ Gen[Bi][Bj]`

### Initial values of In and Out sets

To compute `In[B]`, we compute the intersection of `Out[B*][B]`, where `B*` are
all preds of block `B`. When there is a back edge from block `B'` to `B` (for
example in the case of loops), the Out set for block `B'` will be empty. As a
result, the intersection operation would always result in an empty set `In[B]`.

So to handle this, we initialize the In and Out sets for all blocks to `Top`.
`Top` represents the union of the Gen sets of all edges. We have chosen the
offsets of ptr variables in `Top` to be the max unsigned int. The reason behind
this is that in order to compute the actual In sets for blocks we are going to
intersect the Out sets on all the incoming edges of the block. And in that case
we would always pick the ptr with the smaller offset. Choosing max unsigned int
also makes handling `Top` much easier as we do not need to explicitly store edge
info.

Thus, we have the following two equations for `Top`:
```
Top = ∪ Gen[Bi][Bj]
Top[V] = ∞
```

And the following initial values for all blocks:
```
In[B] = Top
Out[Bi][Bj] = Top, where Bj ∈ succ(Bi)
```

Now, we also need to handle the case where there is an unconditional jump into a
block (for example, as a result of a `goto`). In this case, we cannot widen the
bounds because we would not have tested the ptr dereference on the
unconditional edge. So in this case we want the intersection (and hence the In
set) to result in an empty set.

So we initialize the In and Out sets of all blocks to `Top`, except the Entry
block.

Thus, we have the following initial value for the Entry block:
```
In[Entry] = ∅
Out[Entry][B*] = ∅, where B* ∈ succ(Entry)
```

## Implementation Details
The main class that implements the analysis is
[`BoundsAnalysis`](https://github.com/microsoft/checkedc-clang/blob/master/clang/lib/Sema/BoundsAnalysis.cpp)
and the main function is `BoundsAnalysis::WidenBounds()`.

`WidenBounds` will perform the bounds widening for the entire function. We can
then we can call `BoundsAnalysis::GetWidenedBounds` to retrieve the
widened bounds for the current basic block.

The approach used for implementing the analysis is the iterative worklist
algorithm in which we keep adding blocks to a worklist as long as we do not
reach a fixed point i.e.: as long as the Out sets for the blocks keep changing.

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
