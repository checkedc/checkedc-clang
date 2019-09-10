# Dataflow Analysis for Collecting Facts

## Definitions
**Fact:** when we refer to a `fact`, we mean a comparison between two expressions. Examples include `*a < 2` or `final_size >= initial_size`. Every fact is represented by a `std::pair<Expr *, Expr *>`. The first element of the pair should be less than or equal to the second element.


## Properties
The key properties of the analysis are described below.
1. **Forward:** the basic blocks of the function are traversed in reverse post-order. In other words, a basic block is visited before its successors.
2. **Path-sensitive:** path-sensitivity in this context means we should be able to distinguish between `else` and `then` branches since they generate different facts.
3. **Flow-sensitive:** as any other dataflow analysis, the sequence of basic blocks are taken into consideration when performing the analysis. Note that a basic block might consist of multiple statements. For more information on CFG basic blocks in clang, please refer to the [documentation](https://clang.llvm.org/doxygen/classclang_1_1CFGBlock.html).
4. **Intra-procedural:** the analysis is done on one function at a time. The reason behind this decision is scalability.
5. **Conservative:** since we do not perform pointer analysis prior to collecting facts, we have to be careful about those involving pointer dereferences. Also, when collecting the facts in one function, we are not aware of the side effects of function calls. The details about conservativity will be clarified under implementation details.


## Implementation Details
The main class that implements the analysis is [`AvailableFactsAnalysis`](https://github.com/microsoft/checkedc-clang/blob/master/lib/Sema/AvailableFactsAnalysis.cpp), and the main function is `AvailableFactsAnalysis::Analyze()`.
Prior to checking bounds declarations, `Analyze()` is called. Afterwards, using `AvailableFactsAnalysis::GetFacts(std::pair&)`, we can get the facts that correspond to the *current* basic block. By calling `AvailableFactsAnalysis::Next()`, we move to the next basic block in reverse post-order.

The approach used for implementing the analysis is the iterative worklist algorithm.

*Algorithm:*
* For every basic block, we need the following sets: `Kill`, `GenElse`, `GenThen`, `In`, `OutElse`, and `OutThen`. All of them are initialized as empty sets.
    * `Gen*[B]`: for every block `B` of kind `if`-condition,
      1. if it is a comparison, add the comparison to `GenThen[B]` and add the negated comparison to `GenElse[B]`.
      2. if the condition is a logical AND, for each operand, if it is a comparison, add the comparison to `GenThen[B]`.
      3. if the condition is a logical OR, for each operand, if it is a comparison, add the negated comparison to `GenElse[B]`.
    * `Kill[B]`: first, all the variables that are defined in block `B` are collected. If any comparison `c` contains any of those variables, `c` must be added to `Kill[B]`. Also, if a comparison contains a pointer dereference, it should be added to the corresponding `Kill` set of any potential pointer assignment.
* We also need a vector `Worklist` that is initialized with all the basic blocks of the function in reverse post-order.
* After preparing the above sets and vectors, the main body of the algorithm will be:
```
while (!Worklist.empty()) {
  B = Worklist.pop();

  In[B] = the intersection of the proper Out* sets of B's predecessors.
  OutThen[B] = (In[B] \ Kill[B]) U GenThen[B].
  OutElse[B] = (In[B] \ Kill[B]) U GenElse[B].

  if (OutThen or OutElse changed)
    add successors of B to Worklist
}
```

### Notes on path-sensitivity
In order to implement path-sensitivity, for `Gen` and `Out` sets, there are separate sets for each branch, namely, `GenThen` and `GenElse` instead of `Gen`, and `OutThen` and `OutElse` instead of `Out`. When computing `Gen*` sets, generally, the `if`-condition will be added to `GenThen`, and the negated `if`-condition will be added to `GenElse`.
When updating the `In` set, depending on whether the predecessor is the else branch or then branch, we use `OutElse` or `OutThen`.
When updating the `Out*` sets, `GenThen` will be used for `OutThen` and `GenElse` will be used for `OutElse`.

### Notes on Conservativity
For the sake of conservativity, we had to follow the following rules:
1. The collected facts must be non-modifying.
2. If an expression in a fact contains a pointer dereference, we need to kill the fact at any potential pointer assignment.
3. Any call expression must be considered as possibly doing a pointer assignment.

### Notes on Debugging
In order to debug dataflow-related code, you might use the clang flag `-fdump-extracted-comparison-facts`. This will dump, in reverse post-order, the block ID, `Kill` set, and `In` set for every block.

### Future Improvements
1. For improving the performance, the analysis can be skipped in functions that do not need bounds declaration checking. Currently, the analysis is performed on every function.
2. Handle the logical negation (`!`) operator.