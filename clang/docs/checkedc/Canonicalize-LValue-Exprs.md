# Canonicalizing LValue Expressions

**Note made on 9th Sept 2021** - This doc represents the design that was followed in the initial implementation of this feature. The feature may have had improvements/changes/bug fixes added to it since the initial implementation.

This document describes modifications to the `PreorderAST` class in order to create canonical forms for `MemberExprs`, `UnaryOperators` (including pointer dereferences `*e`), and `ArraySubscriptExprs`. Creating these canonical forms is required in order to create `AbstractSets` for these kinds of lvalue expressions.

## Requirements
Member expressions should have the following canonical forms:
1. `a->f` should be canonicalized to `(a + 0)->f`
2. `(*a).f` should be canonicalized to `(a + 0)->f`
3. `(*(a + 0)).f` should be canonicalized to `(a + 0)->f`
4. `a[0].f` should be canonicalized to `(a + 0)->f`
5. `a.f` should be canonicalized to `a.f`

Pointer dereference expressions should have the following canonical forms:

1. `*e` should be canonicalized to `*(e + 0)`

Array subscript expressions should have the following canonical forms:

1. `e1[e2]` should be canonicalized to `*(e1 + e2 + 0)`

## New PreorderAST Node subclasses

We introduce three new subclasses of `Node` in PreorderAST.h. `Node` subclasses should closely mirror clang AST `Expr` subclasses. The existing `LeafExprNode` class in PreorderAST is used to represent `Expr` kinds for which the PreorderAST does not do any additional processing (e.g. constant folding, sorting, etc.).

#### MemberNode

The `MemberNode` class mirrors the `MemberExpr` clang AST class and has the following members:
1. Base of type `Node *`. This is the canonicalized form of the base expression of a member expression. For example, the canonical form for the expression `a->f` will have a `ImplicitCastNode *` as its Base, where Base has the Kind `LValueToRValue`. (The base expression of the `a->f` is `LValueToRValue(a)`.)
2. Field of type `FieldDecl *`. This is the declaration of the field of a member expression. For example, the canonical form for the expression `a->f` will have `f` as its Field.
3. IsArrow of type `bool`. This indicates whether a member expression is of the form `a->f` or `a.f`.

#### UnaryOperatorNode

The `UnaryOperatorNode` class mirrors the `UnaryOperator` clang AST class and has the following members:

1. Opc of type `clang::UnaryOperator::Opcode`. This is the operator of the node, e.g. `*`, `&`, etc.
2. Child of type `Node *`. This is the canonicalized form of the child of a unary operator. For example, the canonical form for the expression `*(p + i)` will have an `OperatorNode *` as its Child.

The `UnaryOperatorNode` class will also be used to represent `ArraySubscriptExpr` clang AST nodes. An `ArraySubscriptExpr p[i]` will have the canonical form of:

```
UnaryOperatorNode *
  OperatorNode +
    LeafExprNode p
    LeafExprNode i
    LeafExprNode 0
```

The `UnaryOperatorNode` class is introduced in order to canonicalize both dereference operators `*p` and array subscript expressions `p[i]`. `*(p + i)`, `p[i]`, `*(i + p)`, and `i[p]` should all have the same canonical form.

The `UnaryOperatorNode` class also allows `PreorderAST` to canonicalize nested dereference operators. For example, consider `*(*(p + 2))`. This expression should have the canonical form:

```
UnaryOperatorNode *
  ImplicitCastNode LValueToRValue
    UnaryOperatorNode *
      OperatorNode +
        ImplicitCastNode LValueToRValue
          LeafExprNode p
        LeafExprNode 2
```

The expression `*(*(p + i))` should **not** have the following canonical form:

```
LeafExprNode
  UnaryOperator *
    ImplicitCastExpr LValueToRValue
      UnaryOperator *
        BinaryOperator +
          ImplicitCastExpr LValueToRValue
            DeclRefExpr p
          IntegerLiteral 2
```

If the canonical form was simply a `LeafExprNode`, then `PreorderAST` could not perform any normalization on the subexpressions within the `LeafExprNode` such as sorting and constant folding. If this were the case, then `*(*(p + 2))`, `*(*(2 + p))`, and `*(*(p + 1 + 1))` would all have different canonical forms, when they should all have the same canonical form.

#### ImplicitCastNode

The `ImplicitCastNode` cast mirrors the `ImplicitCastExpr` clang AST class and has the following members:

1. CK of type `clang::CastExpr::CastKind`. This is the kind of implicit cast, e.g. `LValueToRValue`, `ArrayToPointerDecay`, etc.
2. Child of type `Node *`. This is the canonicalized form of the child of an implicit cast expression. For example, the canonical form for the expression `LValueToRValue(p)` (where `p` is a `DeclRefExpr *`) will have a `LeafExprNode *` as its Child, where the Child has the `DeclRefExpr *p` as its expression member.

The `ImplicitCastNode` class is introduced in order to differentiate between lvalue and rvalue expressions. `LValueToRValue` casts are not considered value-preserving operations, so `LValueToRValue(a->f)` and `a->f` should not have the same canonical form.

The `ImplicitCastNode` class also allows `PreorderAST` to canonicalize nested `MemberExprs`. For example, consider the expression `a->b->c`. The canonical form for this expression should be:

```
MemberNode ->
  ImplicitCastNode LValueToRValue
    MemberNode ->
      ImplicitCastNode LValueToRValue
        LeafExprNode a
      FieldDecl b
  FieldDecl c
```

The expression `a->b->c` should **not** have the following canonical form:

```
MemberNode ->
  LeafExprNode
    ImplicitCastExpr LValueToRValue
      MemberExpr ->
        ImplicitCastExpr LValueToRValue
          DeclRefExpr a
        FieldDecl b
  FieldDecl c
```

If the base node of the root `MemberNode` were a `LeafExprNode`, then `PreorderAST` could not perform any normalization or further canonicalization on the subexpressions within the `LeafExprNode`. For example, this would mean that `a->b->c` and `((*a).b)->c` would have different canonical forms, when they should have the same canonical form.

### Examples

Note: the `PreorderAST::Create` method represents an expression `e` as `e + 0` in order to correctly determine pointer offsets for a dereference expression (this is used in bounds widening to determine whether an `_Nt_array_ptr` is being dereferenced at its upper offset). Therefore, when creating a new `PreorderAST` for an `Expr *E`, the canonical form for `E` will be an `OperatorNode *` with a `+` operator.

**Unary operator dereference**

Consider the clang AST node of type `MemberExpr *`: `*(p + i + j).f`. The canonical form for this expression will be:

```
OperatorNode +
  LeafExprNode
    IntegerLiteral 0
  MemberNode ->
    ImplicitCastNode LValueToRValue
      UnaryOperatorNode *
        OperatorNode +
          OperatorNode +
            OperatorNode +
              ImplicitCastNode LValueToRValue
                LeafExprNode p
              ImplicitCastNode LValueToRValue
                LeafExprNode i
            ImplicitCastNode LValueToRValue
              LeafExprNode j
          LeafExprNode 0
    FieldDecl f
```

After normalization, `p`, `i`, `j`, and `0` will be coalesced into a single `OperatorNode` and nodes will be sorted where applicable:

```
OperatorNode +
  MemberNode ->
    ImplicitCastNode LValueToRValue
      UnaryOperatorNode *
        OperatorNode +
          ImplicitCastNode LValueToRValue
            LeafExprNode p
          ImplicitCastNode LValueToRValue
            LeafExprNode i
          ImplicitCastNode LValueToRValue
            LeafExprNode j
          LeafExprNode 0
    FieldDecl f
    LeafExprNode
      IntegerLiteral 0
```

**Single-dimensional array subscript**

Consider the clang AST node of type `MemberExpr *`: `p[j + i].f`. (Note: `p[j + i]` is equivalent to `*(p + j + i)`.) The canonical form for this expression will be:

```
OperatorNode +
  LeafExprNode
    IntegerLiteral 0
  MemberNode ->
    ImplicitCastNode LValueToRValue
      UnaryOperatorNode *
        OperatorNode +
          OperatorNode +
            OperatorNode +
              ImplicitCastNode LValueToRValue
                LeafExprNode p
              ImplicitCastNode LValueToRValue
                LeafExprNode j
            ImplicitCastNode LValueToRValue
              LeafExprNode i
          LeafExprNode 0
    FieldDecl f
```

After normalization, `p`, `i`, `j`, and `0` will be coalesced into a single `OperatorNode` and nodes will be sorted where applicable:

```
OperatorNode +
  MemberNode ->
    ImplicitCastNode LValueToRValue
      UnaryOperatorNode *
        OperatorNode +
          ImplicitCastNode LValueToRValue
            LeafExprNode p
          ImplicitCastNode LValueToRValue
            LeafExprNode i
          ImplicitCastNode LValueToRValue
            LeafExprNode j
          LeafExprNode 0
    FieldDecl f
  LeafExprNode 0
```

Note that this normalized canonical form is equivalent to the normalized canonical form for the expression `*(p + i + j).f` described above.

**Multi-dimensional array subscript**

Consider the clang AST node of type `MemberExpr *`: `p[i][j].f`. (Note: `p[i][j]` is equivalent to `*(*(p + i) + j)`.) The canonical form for this expression will be: 

```
OperatorNode +
  LeafExprNode
    IntegerLiteral 0
  MemberNode ->
    ImplicitCastNode LValueToRValue
      UnaryOperatorNode *
        OperatorNode +
          OperatorNode +
            ImplicitCastNode LValueToRValue
              UnaryOperatorNode *
                OperatorNode +
                  OperatorNode +
                    ImplicitCastNode LValueToRValue
                      LeafExprNode p
                    ImplicitCastNode LValueToRValue
                      LeafExprNode i
                  LeafExprNode 0
            ImplicitCastNode LValueToRValue
              LeafExprNode j
          LeafExprNode 0
    FieldDecl f
```

After normalization: `p`, `i`, and `0` will be coalesced into a single `OperatorNode`, and `*(p + i + 0)`, `j`, and `0` will be coalesced into a single `OperatorNode`. Note that `p`, `i`, and `j` will **not** be coalesced into a single `OperatorNode`, since `j` is a child of a different `UnaryOperatorNode` than `p` and `i`. Also note that `p[i][j]` will **not** have the same canonical form as `p[j][i]`. `p[i][j]` is equivalent to `*(*(p + i) + j)`, while `p[j][i]` is equivalent to `*(*(p + j) + i)`. These expressions are not equivalent and therefore should not have the same canonical form.

```
OperatorNode +
  MemberNode ->
    ImplicitCastNode LValueToRValue
      UnaryOperatorNode *
        OperatorNode +
          ImplicitCastNode LValueToRValue
            UnaryOperatorNode *
              OperatorNode +
                LeafExprNode p
                LeafExprNode i
                LeafExprNode 0
            ImplicitCastNode LValueToRValue
              LeafExprNode j
            LeafExprNode 0
    FieldDecl f
  LeafExprNode 0
```



### Creating the new Node subclasses

#### Creating a MemberNode

The `PreorderAST::Create(Expr *E, Node *Parent)` method behaves as follows if `E` is a `MemberExpr *`:

1. Let `Base` = `E->getBase()`, `Field` = `E->getMemberDecl()`

2. Let `ArrowBase` = `nullptr`

3. If `E` is an arrow member expression (`E` is of the form `Base->Field`):

   * `ArrowBase` = `Base`

4. Otherwise (`E` is of the form `Base.Field`):

   - If `Base` is of the form `*e1`:
     - `ArrowBase` = `e1`

   * Otherwise, if `Base` is of the form `e1[e2]`:
     * `ArrowBase` = `e1 + e2`

5. If `ArrowBase` is non-null:

   * Create an arrow MemberNode with `ArrowBase + 0` as its base:
     * Let `M` = `new MemberNode(Field, true, Parent)`
     * `AddNode(M, Parent)`
     * `AddZero(ArrowBase, M)`

6. Otherwise (`ArrowBase` is null):

   * Create a dot MemberNode with `Base` as its base:
     * Let `M` = `new MemberNode(Field, false, Parent)`

     * `AddNode(M, Parent)`

     * `Create(Base, M)`

#### Creating a UnaryOperator

The `PreorderAST::Create(Expr *E, Node *Parent)` method behaves as follows if `E` is a `UnaryOperator *`:

1. If `E` is a dereference operator (`E` is of the form `*e`):
   * Create a UnaryOperator with `e + 0` as its child:
     * Let `U` = `new UnaryOperatorNode(E->getOpcode(), Parent)`
     * `AddNode(U, Parent)`
     * `AddZero(e, U)`
2. Otherwise, if `E` is a unary plus or minus operator (`E` is of the form `+e` or `-e`) and `E` is an integer constant expression:
   * Create a LeafExprNode with `E` as its child so that expressions like `+2` and `-3` can be constant folded:
     * Let `L` = `new LeafExprNode(E, Parent)`
     * `AddNode(L, Parent)`
3. Otherwise (`E` is any other kind of unary operator with a subexpression `e1`):
   * Create an UnaryOperatorNode with `e1` as its child:
     * Let `U` = `new UnaryOperatorNode(E->getOpcode(), Parent)`
     * `AddNode(U, Parent)`
     * `CreateNode(e1, U)`

The `PreorderAST::Create(Expr *E, Node *Parent)` method behaves as follows if `E` is an `ArraySubscriptExpr *` (`E` is of the form `e1[e2]`):

1. Let `DerefExpr` = `e1 + e2`
2. Create a UnaryOperatorNode with a dereference operator and `DerefExpr + 0` as its child:
   * Let `U` = `new UnaryOperatorNode(*, Parent)`
   * `AddNode(U, Parent)`
   * `AddZero(DerefExpr, U)`

The new helper method `PreorderAST::AddZero(Expr *E, Node *Parent)` creates an `OperatorNode` with the operator `+` and the children `E` and `0`, and adds the created `OperatorNode` as a child of `Parent`.

For example, if `Parent` is a `UnaryOperatorNode` with the `*` operator and `E` is the expression `LValueToRValue(p)`, then calling `AddZero(E, Parent)` will result in:

```
UnaryOperatorNode *
  OperatorNode +
    ImplicitCastNode LValueToRValue
      LeafExprNode p
    LeafExprNode 0
```

### Changes to PreorderAST
#### Parent node type
One notable change to the current PreorderAST implementation is the current assumption that all parent nodes are of type `OperatorNode *`. With the addition of the `MemberNode` class, this is no longer the case. A node can be the child of either an `OperatorNode *` or a `MemberNode *`. The `Parent` property of the `Node` class changes from `OperatorNode *` to `Node *`. This is relevant in the following PreorderAST methods:
1. AddNode. This method adds a `Node *N` to a `Node *Parent`. Previously, the `Parent` argument had type `OperatorNode *`.
2. CanCoalesceNode. This method determines whether an `OperatorNode *O` can be coalesced into its parent. An additional check is needed here to determine whether the parent of `O` is an `OperatorNode *`.
3. CoalesceNode. This method coalesces an `OperatorNode *O` into its parent. A cast is needed here to cast the parent of `O` to an `OperatorNode *` in order to remove `O` from the list of children of its parent.

#### Node comparisons
The `CompareNodes` and `Compare` methods also need to change in order to compare `MemberNodes`, `UnaryOperatorNodes`, and `ImplicitCastExprNodes` in addition to `OperatorNodes` and `LeafExprNodes`.

In the `CompareNodes` method for `Node *N1` and `Node *N2`, we use the following rules:

1. `OperatorNode < UnaryOperatorNode < MemberNode < ImplicitCastNode < LeafExprNode`.

2. If `N1` and `N2` are both `UnaryOperatorNodes`:

   * If `N1->Opc < N2->Opc`, then `N1 < N2`.
   * Else if `N1->Child < N2->Child`, then `N1 < N2`.

3. If `N1` and `N2` are both `MemberNodes`:

   * If `N1->IsArrow` is true and `N2->IsArrow` is false, then `N1 < N2`.

   * Else if`N1->Field < N2->Field`, then `N1 < N2`.

   * Else if `N1->Base < N2->Base`, then `N1 < N2`.

4. If `N1` and `N2` are both `ImplicitCastNodes`:

   * If `N1->CK < N2->CK`, then `N1 < N2`.
   * Else if `N1->Child < N2->Child`, then `N1 < N2`.

In the `Compare` method for `Node *N1` and `Node *N2`, we use the following rules:

1. `LeafExprNode < ImplicitCastNode < MemberNode < UnaryOperator < OperatorNode`.
2. If `N1` and `N2` are both the same kind of node (e.g. both are `MemberNodes`):
   * See the rules for comparing `UnaryOperatorNodes`, `MemberNodes`, and `ImplicitCastNodes` in the `CompareNodes` method above.

#### Recursive normalization operations

For normalization passes such as sorting, coalescing, and constant folding, `OperatorNodes` are no longer the only kind of node that can have children. For `MemberNodes`, `UnaryOperatorNodes`, and `ImplicitCastNodes`, we now need to recursively sort, coalesce, and constant fold the base or child node.

For example, in `PreorderAST::Sort`, if the `Node *N` is a `MemberNode`, we call `Sort(N->Base)`.

## Canonicalizing nested expressions
### Not handled: expressions within LeafExprNodes
Not all clang AST expression kinds have a corresponding `Node` kind in `PreorderAST`. As a result, some expressions which `PreorderAST` should fully normalize will be `Exprs` nested within a `LeafExprNode` rather than the appropriate kind of `Node`.  Consider:
```
struct S {
  int n;
};

int getInt(int n) { return n; }

void f(struct S *s) {
  (*(s + getInt(s->n))).n;
  (*(s + getInt((*s).n))).n;
}
```
There is no `CallNode` in `PreorderAST`. `CallExprs` are one kind of expression for which `PreorderAST` performs no additional processing, so a `CallExpr` will be wrapped in a `LeafExprNode`. Therefore, the member expressions `s->n` and `(*s).n` within the call expressions `getInt(s->n)` and `getInt((*s).n)` will not be traversed by `PreorderAST::Create`, so `MemberNodes` will not be created for these member expressions. Instead, `LeafExprNodes` will be created for the call expressions. The member expressions in the example function will not be considered equivalent. For example, the canonical form of `getInt(s->n)` will be:

```
LeafExprNode
  CallExpr
    Function: getInt
    Arguments:
      MemberExpr ->
        ImplicitCastExpr LValueToRValue
          DeclRefExpr s
        FieldDecl n
```

### Nested dereference expressions
The expressions `(*a).f`, `(*(a + 0)).f`,`a[0].f`, and `0[a].f` should all have the same canonical form. For any expression `e`, PreorderAST needs to treat `*e`, `*(e + 0)`, `e[0]`, and `0[e]` as equal. Consider:

```
struct S {
  _Array_ptr<int> f : count(1);
};

void f(_Array_ptr<struct S *> arr : count(5)) {
 (*(*arr)).f;
 (*(*(arr + 0))).f;
 (*(*(arr + 0) + 0)).f;
 (*(arr[0])).f;
 ((*arr)[0]).f;
 (arr[0][0]).f;
}
```

All of the dereference expressions `*(*arr)`, `*(*(arr + 0))`, `*(*(arr + 0) + 0)`, `*(arr[0])`, `(*arr)[0]`, and `arr[0][0]` should be considered equivalent expressions so that each of the member expressions will have the same canonical form. These dereference expressions should have the canonical form:

```
UnaryOperatorNode *
  BinaryOperator +
    ImplicitCastNode LValueToRValue
      UnaryOperatorNode *
        BinaryOperator +
          ImplicitCastNode LValueToRValue
            LeafExprNode
              DeclRefExpr arr
          LeafExprNode
          	IntegerLiteral 0
    LeafExprNode
      IntegerLiteral 0
```

