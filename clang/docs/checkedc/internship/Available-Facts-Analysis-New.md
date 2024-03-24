
# Dataflow Analysis on Available Facts

## Overview

The design doc begins with the recap of facts and where-clauses in Checked C, gathered from the spec and program examples. After that, the design doc discusses the relation between the C language constructs and their representation in the Clang CFG on which the available facts dataflow analysis is applied. Then the dataflow analysis is discussed. The follow-up works after the dataflow analysis are covered in the end.

## Definition of Facts and Where-clauses

An **available fact** can either be inferred or can appear as a where-clause fact. We infer facts from conditions in if-statements, switch-statements, for-loops, and while-loops. A **where-clause fact** can be either a bounds declaration or a relational logic expression.  A where-clause may be specified at variable declaration in statements, in function parameters (spec 3.3, 9.1), or as a stand-alone statement (spec 9.1). In the last case, the where-clause, if specified, is the part of *null statement*.

### Where-clauses Examples

```c
// where clauses in the declaration of variables.
nt_array_ptr<char> p : bounds(p, p + 1) = "a";
int x = 1 where p : count(x);
// ----
// where clauses in the declaration of function parameters
void f(ptr<int> x where x : count(3)) {
  ptr<int> i = x;
}
// ----
int x = 1;
// standalone where clauses
where x < 42;
// ----
// Note that in where-clauses we use `and` rather than `&&` 
// to make the conjunction of facts
int x = 1 where 1 <= x and x < 42;
```
Code - where.c

## Facts Gathering

### Facts gathering on control flows 

Besides fact declaration, facts can be automatically inferred for if-statements, switch statements, loops, and assignment statements.

For **if-statements**, the test becomes a fact in the beginning of the then-block; the negation of the test becomes a fact in the beginning of the else-block. For **switch-statements**, in each non-default case, it gathers the fact that the switch expression equals the case value in the beginning of the case block; in the default case, it gathers the fact which is the negation of the disjunction of all the facts for case block. If there is no `break` in one case block and it passes through the other case block, it won't gather the fact of the latter block. For **loops**, the loop test is a fact in the beginning of the loop body, the negation of the loop test is a fact after the loop.

### Facts gathering on assignments 

(from spec 9.2 and 4.3.1. Ch 9.2 **Checking** Facts, however, it also generates facts)

For **assignments** like `x := e1`, facts immediately before the assignments are also true immediately after the assignment, if the assignment doesn't mutate the value of variable used in the facts. Otherwise, say a fact `Fact(x)` contains `x` whose var is the value before the assignment `x_before`.
Now the same fact `Fact(x)` may not true since `x`'s value is `x_after`. 

If the expression `e` being assigned is invertible, the fact that use `x` will be updated to use an expression that inverts the new value to compute the old value. Otherwise, any bounds expression that involves x is invalidated.

An **assignment** can generate new fact after it if `e1` is _a valid non-modifying expression_. The fact is `x == e1`.

For variables that are declared to be equal, a fact `x == y` is generated after the statement.

## Facts checking

Where-clause facts are expected to be true. In a sense, it behaves similar to `static_assert`. Facts checking intends to prove that a program invariant holds at every program point, given the available program invariants before this program point. When the dataflow analysis is done, both the program invariants before the statement and at the statement are known. We can check whether the implication holds at every statement. If it holds, the checking passes and the available facts can be used to prove other properties. Otherwise, we prompt where the checking fails.

The facts and checking are encoded into SMT constraints, and a Z3 solver either proves or disproves.

## C language constructs and the Clang CFG

From the perspective  of CFG(control-flow graph), the condition checking in C language constructs corresponds to a conditional jump at the *end* of the block. In clang, a conditional jump includes a logic test and two or more successor blocks. For compound logic expressions, clang generates more blocks and each block is for checking a condition in the compound logic expression.

## Dataflow Analysis

### Assumption

For the first implementation pass, we are only going to collect
(1) facts specified in where-clauses (relational facts and bound facts).
(2) facts inferred by the conditions in if-statement, switch-statement, for-loops, and while loops.
(3) declared bounds facts.

### Assumption Example (need discussion)

```c
_Array_ptr<int> bar(int x: _Where x > 0) 
_Where (return_value : bounds (return_value, return_value + x))
{
  return (_Array_ptr<int>)my_malloc(x);
}

void foo(int n) {
  if (n > 10) {
    // available fact : n > 10
    // fact checker needs to prove: n > 0
    s1: _Array_ptr<int> y1 = bar(n) _Where y1 : bounds (y1, y1+n);

  } else {
    // available fact : NOT (n > 10)
    // fact checker needs to prove: n > 0 (but it cannot prove)
    s2: _Array_ptr<int> y2 = bar(n) _Where y2 : bounds (y2, y2+n);
  }
}
```
Code - assumption.c

The example is to discuss the *intra-procedural* dataflow analysis and make clear what are the **available dataflow facts** at different program points, what are **fact checker** tasks, and what are the **available analysis** tasks.

The **available fact analysis** provides the set of facts at every program point. These are facts:
1. The fact `n > 10` is available at `s1` (inferred fact).
2. The fact `n > 10` is available at `s1` (inferred fact).
3. The fact `x > 0` is available at the beginning of `bar` (where-clause fact).
4. The fact `y1 : bounds (y1, y1+n)` is available after `s1` and proceeds further (where-clause fact).
5. The fact `y2 : bounds (y2, y2+n)` is available after `s2` and proceeds further (where-clause fact).

The fact checker can use all the available facts at a statement when it needs to discharge a proof after considering the effect of statement S. These are expected **fact checker** tasks:
1. In `foo`, the **fact checker** has to prove `n > 0` at `s1` to call `bar`. The fact checker can prove it with the fact `n > 10`. 
2. In `foo`, the **fact checker** has to prove `n > 0` at `s2` to call `bar`. The fact checker cannot prove it with the fact `NOT (n > 10)`. The compiler will issue an error message. 
3. In `foo`, the **fact checker** has to prove that `return_value : bounds (return_value, return_value + x)` with the available facts implies `y1 : bounds (y1, y1+n)` at `s1`. 
4. In `foo`, the **fact checker** will never need to prove `return_value : bounds(return_value, return_value + x)` with the available facts  implies `y2 : bounds (y2, y2+n)` at `s2`. 

### Properties

1. Forward & Flow-sensitive: The facts are propagated forward along the statements and the block edges.
2. Path-sensitive: Conditional jumps gather different facts on edges.
3. Intra-procedural: The analysis is done per function.

### Dataflow equations

Given definitions cfg block `B`, cfg edge `Edge` (which is a pair of block `(B1,B2)`), fact `Fact`, fact set `FactSet`, we can define the dataflow functions:

For block:
`Kill : B -> FactSet`
`In   : B -> FactSet`

For block-block edge:
`Gen : Edge -> FactSet`
`Out : Edge -> FactSet`

Control-flow related dataflow facts are stored in `Gen(B,B')`. It includes **if-statements**, **switch-statements**, **for-statements**, and **while-statements**. Dataflow facts that are not control-flow related are stored in `Gen(B,AllSucc)`. `AllSucc` is a special (abstract) block which represents all the successors of block `B`. 

In order to compute `Gen(B,B')`, we compute `StmtGen[B][Stmt]` and `StmtKill[B][Stmt]` for each statement `Stmt` in block `B`. It includes **variable declarations** and **where-clause facts** for this implementation pass.

`StmtGen  : B * Stmt -> FactSet`
`StmtKill : B * Stmt -> FactSet`

The inferred facts that belong to `Gen(B,B')` are computed based on the edge kind:

```
for each block B,
  for each successor B' of B,

    case (B is if-test-block with `test`) and (B' is then-block):
      Gen(B,B') = {test}

    case (B is if-test-block with `test`) and (B' is else-block):
      Gen(B,B') = {NOT test}

    case (B is switch-block with `e`) and (B' is a case-block with `case v:`):
      Gen(B,B') = {e == v}

    case (B is switch-block with `e`) and (B' is a case-block with `default:`):
      find all case `v_i` that is the successor of B
      Gen(B,B') = {Not (e == v1 OR e == v2 OR ...)}

    case B is while-test-block 
      ...
```
List - Compute `Gen(B,B')`

The facts specified via where-clauses and declared bounds that belong to `Gen[B,AllSucc]` are computed 
`Gen[B,AllSucc] = StmtGen[B, LastStmt(B)]`

`StmtGen[B][Stmt]` is computed as follows:

For each statement we compute `StmtGen[B][Stmt]` and `StmtKill[B][Stmt]`, and compute `Gen[B,AllSucc]` as described by the equation below.

```
Gen[B,AllSucc] = 
    StmtGen[B][S_n] 
    ∪ (StmtGen[B][S_n-1] - StmtKill[B][S_n])
    ∪ (StmtGen[B][S_n-2] - StmtKill[B][S_n-1] - StmtKill[B][S_n])
    ∪ ... 
    ∪ (StmtGen[B][S_1] - StmtKill[B][S_2] - StmtKill[B][S_3] - ... - StmtKill[B][S_n])
```

```
for each block B,
  for each statement Stmt in B in the forward order,
    case (Stmt is variable declaration with bound declaration `x = v : bound`):
      add bound to StmtGen[B][Stmt]

    case (Stmt is variable declaration with where-clause WFacts
              or null-statement with where-clause facts WFacts):
      for each Fact in WFacts:
        add Fact to StmtGen[B][Stmt]

    case (Stmt ...)
      ...
```
List - Compute `StmtGen[B][Stmt]`

```
for each block B,
  for each statement Stmt in B in the forward order,
    case (Stmt is assignment `x = ...` and let Fact be a relational fact which uses `x`):
      add Fact to StmtKill[B][Stmt]

    case (Stmt is with where-clause bound fact `v: bound` and let Fact be a bound fact on `v`):
      add Fact to StmtKill[B][Stmt]
```
List - Compute `StmtKill[B][Stmt]`

`Out[B,B'] = Gen[B,B'] ∪ (In[B] - Kill[B])` for every `B' ∈ succ(B)` (where `∪` is set union)
`Out[B,AllSucc] = Gen[B,AllSucc] ∪ (In[B] - Kill[B])` 

`In[B] = ∩ (Out['B][B] ∪ Out['B][AllSucc])` for all `'B ∈ pred(B)` (where `∩` is set intersection)

#### Initialization

`In[B] = {}`

`Out[B,B'] = ∪ (StmtGen[B,Stmt]` for all `B`)
union of all the inferred dataflow facts

`Out[B,AllSucc] = ∪ (StmtGen[B,AllSucc]` for all `B`)
union of all the where-clauses and declared bound facts

### Data structures

Most of the data structures share the same definitions of the other analysis for reference.
The fact definition needs to be extended to hold:
1. The original source of the fact. It's used for diagnosis to prompt which is the original source of the fact. To support this, the temporary solution is to have a fact-statement map in the Analysis class.
2. InferredFact, the fact gathered on control-flow conditions.
3. NegationFact, though it's not difficult to find the negation operation of an equality-or-relational-expression, it will be convenient if we can use a negation flag to mark a fact in the fact gathering.

## Scenario discussion - bound recovery

Bound recovery provides an extra way to collect the available fact. The scenario is the bound fact is killed in one block before the merge point. Therefore, no available fact of this bound will be after the merge point. It's expected that the analysis can recovery the most recent agreed bound as available fact.

```c
extern _Itype_for_any(T) void *malloc(unsigned long size)
    : itype(_Array_ptr<T>) byte_count(size);

extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>),
                                         unsigned long size)
    : itype(_Array_ptr<T>) byte_count(size);

void foo(int extra)
_Checked{
   _Array_ptr<char> p = 0;
   p = (_Array_ptr<char>)malloc<char>(20) _Where p : count(20); 
   if (extra)
   {
      p = (_Array_ptr<char>)realloc<char>(p, 40) _Where p : count(40); 
   }
   // Currently, compiler issues an error message for the stmt below:
   // t1.c:19:16: error: expression has unknown bounds
   char test = p[15];
}
```
Code - t1.c

At the statement `test`, no fact on `p`'s bound is available with no bound recovery, so the compiler issues an error. The error is not very useful and can be eliminated if we can recover `p`'s bound as `p : count(20)` from the fact before the if-expression.

```c
extern _Itype_for_any(T) void *malloc(unsigned long size)
    : itype(_Array_ptr<T>) byte_count(size);

extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>),
                                         unsigned long size)
    : itype(_Array_ptr<T>) byte_count(size);

void foo(int extra)
_Checked{
   _Array_ptr<char> p = 0;
   int n;
   p = (_Array_ptr<char>)malloc<char>(20) _Where p : count(20); 
   if (extra)
   {
      n = 40;
      p = (_Array_ptr<char>)realloc<char>(p, n) _Where p : count(n); 
   }
   else
   {
      n = 10;
      p = (_Array_ptr<char>)realloc<char>(p, n) _Where p : count(n); 
   }
   // Currently, compiler issues an error message for the stmt below:
   // t2.c:26:16: error: expression has unknown bounds
   char test = p[15];
}
```
Code - t2.c

At the statement `test`, no fact on `p`'s bound is available since the then-block and the else-block has different facts on `p`'s bound. The compiler issues an error. The error can also be eliminated if we can recover `p`'s bound as `p : count(20)` from the fact before the if-expression.

## Project plan

The discussion of facts on more statements and fact checkings is orthogonal to the implementation of the framework. In the first pass of the implementation, the tasks are

1. [ ] Implement the Gen only on available facts in where-clauses and control-flow statements `while/for/goto`.
2. [ ] Implement the Kill, In, Out to finish the dataflow analysis.
3. [ ] Integrate LLVM Z3 support library (incremental solving / use bitvector / unsat core).
4. [ ] Encode facts to SMT constraints and solve them with Z3 API.
5. [ ] Discuss, design and implement more available facts.
6. [ ] Testing, evaluating this project on benchmarks and practical code.

## Future topics

- Implement fact checker and diagnose results.
- Work through more scenario examples.
- fact gathering on parameter types e.g. `unsigned x` means `x >= 0`
- fact gathering on pointers (alias problem?)
- no way yet to specify the program invariant on the return values except for its type
- dataflow analysis optimization (quick converging)
- `in_bounds` (seems unsupported)
- (beyond the topic) equivalent expression reasoning with the help of SMT solver
- check the cfg for (the cfg should be the same)
`if (e) {return} ;      {}`
`if (e) {return}   else {}`
