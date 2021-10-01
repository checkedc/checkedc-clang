# Synthesizing Member Expressions

[TOC]

**Note made on 9th Sept 2021** - This doc represents the design that was followed in the initial implementation of this feature. The feature may have had improvements/changes/bug fixes added to it since the initial implementation.

## Problem description

When a `MemberExpr` `M` is modified via an assignment, we need to create an `AbstractSet` for each `MemberExpr` `E` whose target bounds use the value of `M`. These created `AbstractSets` should have their inferred bounds recorded in the `ObservedBounds` map.

In this document, we will refer to the following struct definitions:

```
struct C {
	int len;
	_Array_ptr<int> r : count(len);
};

struct B {
	int i;
	int j;
	struct C *c;
	_Array_ptr<int> q : count(c->len);
};

struct A {
	struct B *b;
	int i;
	int j;
	_Array_ptr<int> start;
	_Array_ptr<int> end;
	_Array_ptr<int> p : count(b->c->len + b->i);
	_Array_ptr<int> q : bounds(start, end);
	_Array_ptr<int> r : bounds(q, q + i);
};
```

## Examples

Consider the following function that modifies member expressions:

```
void f(struct A *a) {
	// The following member expressions' target bounds use the value of a->b->c->len:
	//   1. a->b->c->r
	//   2. a->b->q
	//   3. a->p
	a->b->c->len = 0;
	
	// The following member expressions' target bounds use the value of a[0].b[0].c[0].len:
	//   1. a[0].b[0].c[0].r
	//   2. a[0].b[0].q
	//   3. a[0].p
	// Note: the declared bounds of a[0].p reference b->c->len, which is canonically equivalent
	// to b[0].c[0].len. We use a PreorderAST to do semantic expression comparison when we
	// determine whether a target bounds expression uses the value of a given member expression.
	// (A similar idea applies to a[0].b[0].q, whose declared bounds use b->c->len.)
	a[0].b[0].c[0].len = 0;
}
```

For each assignment to a member expression `M` in the above function `f`, the member expressions whose target bounds use the value of `M` have the following canonical forms. These canonical forms are used to create the resulting AbstractSets that are used as keys in `ObservedBounds`.

1. `(((a + 0)->b + 0)->c + 0)->r`
2. `((a + 0)->b + 0)->q`
3. `(a + 0)->p`

Consider the following function that modifies member expressions:

```
void g(struct A *a, int i) {
	// The following member expressions' target bounds use the value of a[i].b[0].c[0].len:
	//   1. a[i].b[0].c[0].r
	//   2. a[i].b[0].q
	//   3. a[i].p
	// Note: the declared bounds of a[i].p reference b->c->len, which is canonically
	// equivalent to b[0].c[0].len. (Similarly for the declared bounds of a[i].b[0].q.)
	a[i].b[0].c[0].len = 0;
	
	// The following member expressions' target bounds use the value of a[i].b[i].c[0].len:
	//   1. a[i].b[i].c[0].r
	//   2. a[i].b[i].q
	// Note: a[i].p's target bounds do not use this member expression, since the declared bounds of
	// a[i].p reference b->c->len, which is not equivalent to b[i].c[0].len.
	a[i].b[i].c[0].len = 0;
	
	// The following member expressions' target bounds use the value of a[i].b[i].c[i].len:
	//   1. a[i].b[i].c[i].r
	// Note: a[i].p's target bounds do not use this member expression, since the declared bounds of a[i].p
	// reference b->c->len, which is not equivalent to b[i].c[i].len.
	// Note: a[i].b[i].q's target bounds do not use this member expression, since the declared bounds of
	// a[i].b[i].q reference c->len, which is not equivalent to c[i].len.
	a[i].b[i].c[i].len = 0;
}
```

## High-level approach

Synthesizing the set of AbstractSets that contain member expressions whose target bounds depend on a member expression `M` is done in a `SynthesizeMembers` method, which takes the following arguments:

1. `E` of type `Expr *`. This the current expression that we are using to search for member expressions whose bounds use the value of `M`. The initial value of `E` is `M`. Recursive calls to `SynthesizeMembers` are made for certain subexpressions of `E`.
2. `M` of type `MemberExpr *`. This is the member expression that is being modified via an assignment. We want to create the set of AbstractSets whose target bounds use the value of `M`. `M` is of the form `MBase->MField` or `MBase.MField`.
3. `BoundsSiblingFields` of type `map<FieldDecl *, set<FieldDecl *>>`. This maps a `FieldDecl *` `F` to the set of all sibling fields of `F` in whose (non-concrete, non-expanded) declared bounds `F` occurs. (See below for an extended description of `BoundsSiblingFields`.)
4. `AbstractSets` of type `set<const AbstractSet *>`. This is the out parameter that holds the resulting AbstractSets whose target bounds use the value of `M`.

`SynthesizeMembers` behaves as follows for arguments `E`, `M`, `BoundsSiblingFields`, and `AbstractSets`:

* If `E` is a member expression of the form `base->field` or `base.field`:
  * Get the set `BoundsSiblingFields[field]` of `FieldDecls` in whose declared bounds `field` appears.
  * For each `FieldDecl *` `F` in `BoundsSiblingFields[field]`:
  * Create a `MemberExpr *` of the form `base->F` or `base.F`. Call this `MemberExpr *` `N`.
  * Create the concrete, expanded target bounds of `N` by calling `MemberExprTargetBounds(N)`. Call these target bounds `B`.
  * Traverse the target bounds `B` to determine whether the member expression `M` occurs in `B`.
  * If `M` occurs in `B`, create an `AbstractSet *` for `N`. Add the `AbstractSet *` to the result set `AbstractSets`.
  * Call `SynthesizeMembers(base, M, BoundsSiblingFields, AbstractSets)`.
* Otherwise (`E` is not a member expression): recursively call `SynthesizeMembers` on certain subexpressions of `E`:
  * If `E` is a cast expression of the form `(T)e1`: (note that `T` may be an implicit cast, e.g. an `LValueToRValue` cast):
    * Call `SynthesizeMembers(e1, M, BoundsSiblingFields, AbstractSets)`.
    * We recurse on the child of a cast expression in order to handle expressions such as `(LValueToRValue(a->b))->c`. Here, `a->b` is a member expression within a cast expression.
  * If `E` is a unary operator of the form `@e1` (where `@` is a unary operator such as `*`, `&`, `+`, `-`, etc.):
    * Call `SynthesizeMembers(e1, M, BoundsSiblingFields, AbstractSets)`.
    * We recurse on the child of a unary operator in order to handle expressions such as `(*(a->b)).c`. Here, `a->b` is a member expression within a unary operator.
  * If `E` is an array subscript expression of the form `base[index]` or `index[base]` (note: `base` is the pointer-typed expression and `index` is the integer-typed expression):
    * Call `SynthesizeMembers(base, M, BoundsSiblingFields, AbstractSets)`.
    * We recurse on the child of an array subscript expression in order to handle expressions such as `(a->b)[0].c`. Here, `a->b` is a member expression that is the base of an array subscript expression.

  * If `E` is a binary operator of the form `e1 @ e2` (where `@` is a binary operator such as `+`, `-`, `&&`, etc.):
    * If `e1` is a pointer-typed expression, call `SynthesizeMembers(e1, M, BoundsSiblingFields, AbstractSets)`.
    * If `e2` is a pointer-typed expression, call `SynthesizeMembers(e2, M, BoundsSiblingFields, AbstractSets)`.
    * We recurse on the pointer-typed children of a binary operator in order to handle expressions such as `(*(a->b + 2)).c`. Here, `a->b` is a member expression that is a pointer-typed child of a binary operator.

## Design issue: avoiding the need to create new MemberExprs

In the above approach, we construct a new clang AST node of type `MemberExpr *` for every field with declared bounds of every struct declaration that is involved in a `MemberExpr` `M`. This can become expensive since we currently do not deallocate these constructed `MemberExprs`.

We currently construct these `MemberExprs` for three reasons. In order to avoid constructing these `MemberExprs`, we will need to design a solution that works around all of these reasons.

**Reason 1: constructing concrete, expanded target bounds**

For a given base expression `s` of type `struct S *` and a field declaration `f` (where `f` is a field of `struct S`), we need to be able to determine whether the target bounds of `s->f` use the value of a member expression `m`. In order to do so, we need to construct the concrete, expanded target bounds of `s->f`. Here, "Concrete" means that the declared bounds of a field `f` depend on the value of the base expression `s`. "Expanded" means that `count` and `byte_count` bounds have been expanded to `range` bounds.

For example: suppose we are synthesizing member expression AbstractSets whose target bounds depend on the expression `a->b->c->len`.

Suppose the current base expression is `a->b->c` of type `struct C *`. The fields of `struct C` are `len` of type `int` and `r` of type `_Array_ptr<int>`. When considering the field `r` with declared bounds of `count(len)`, we need to:

1. Concretize the declared bounds to `count(a->b->c->len)`, and:
2. Expand the concretized `count` bounds to `bounds(a->b->c->r, a->b->c->r + a->b->c->len)`.

Then, we can determine that these (expanded, concrete) target bounds use the value of the member expression `a->b->c->len`.

In order to do these steps, we need the `MemberExpr *` `a->b->c->r`, so we construct this expression from the base `a->b->c` and the field `r`.

Similarly, if the current base expression is `a->b` of type `struct B *` and we are considering the field `q` in `struct B` with declared bounds `count(c->len)`, we need to concretize and expand these bounds to `bounds(a->b->q, a->b->q + a->b->c->len)`.

Finally, if the current base expression is `a` of type `struct A *` and we are considering the field `p` in `struct A` with declared bounds `count(b->c->len)`, we need to concretize and expand these bounds to `bounds(a->p, a->p + a->b->c->len)`.

**Reason 2: creating an AbstractSet: creating a PreorderAST**

Once we have determined that the target bounds of a base expression `s` of type `struct S *` and a field declaration `f` use the value of a member expression `M`, we need to create an AbstractSet for `s->f`. To do so, we first need to create a PreorderAST for `s->f`. The PreorderAST constructor (which calls `PreorderAST::Create`) requires an argument of type `Expr *`. Thus, we need to create a `MemberExpr *` `s->f` in order to create a PreorderAST and then use the PreorderAST to create an AbstractSet.

**Reason 3: creating an AbstractSet: representative expression**

The AbstractSet class has a `Representative` member of type `Expr *`. This member is set to the first `Expr *` that is used to create a new AbstractSet. During bounds checking, the `Representative` field is used for the following purposes:

1. Creating the lvalue target bounds that apply to all lvalue expressions in the AbstractSet. Bounds validation checks if `ObservedBounds[A]` imply the target bounds of `A` for an `AbstractSet *` `A`.
2. Emitting diagnostic messages if the bounds of lvalue expressions in the AbstractSet are not provably valid. We need the `Representative` field to emit a diagnostic message such as "error: bounds of `Representative` are unknown after assignment". For example, if the bounds of a variable `p` are unknown after an assignment, we emit "error: bounds of 'p' are unknown after assignment". Once member expressions are included in `ObservedBounds`, we can emit messages such as "error: bounds of 's->f' are unknown after assignment".

Besides needing an `Expr *` to create a PreorderAST, this is the other reason that the `GetOrCreateAbstractSet` method requires an argument of type `Expr *`.

**Example: creating many MemberExpr nodes**

Consider the following example:

```
struct Z {
	int len;
	_Array_ptr<int> r1 : count(len + 1);
	_Array_ptr<int> r2 : count(len + 2);
	_Array_ptr<int> r3 : count(0);
};

struct Y {
	struct Z *z;
	_Array_ptr<int> q1 : count(z->len + 1);
	_Array_ptr<int> q2 : count(z->len + 2);
	_Array_ptr<int> q3 : count(0);
};

struct X {
	struct Y *y;
	_Array_ptr<int> p1 : count(y->z->len + 1);
	_Array_ptr<int> p2 : count(y->z->len + 2);
	_Array_ptr<int> p3 : count(0);
};

void f(struct X *x) {
	x->y->z->len = 0;
}
```

Without gathering any information about the (non-concrete, non-expanded) declared bounds of each field, the current design would construct the following `MemberExps`:

1. `x->y->z->r1`
2. `x->y->z->r2`
3. `x->y->z->r3`
4. `x->y->q1`
5. `x->y->q2`
6. `x->y->q3`
7. `x->p1`
8. `x->p2`
9. `x->p3`

## Design idea: cheaply avoiding creating MemberExprs for unaffected fields

Based on the reasons outlined in the above section, we need to create a `MemberExpr *` `s->f` (or `s.f`) for a base `s` and field `f` if the concrete, expanded target bounds of `f` depend on a member expression `M`. In order to create as few of these `MemberExprs` as possible, we want to avoid creating a `MemberExpr *` for `s` and `f` if we can prove that the bounds of `f` do not depend on `M` without concretizing or expanding the declared bounds of `f`. In order to ensure soundness, we must be able to guarantee that the bounds of `f` do not depend on `M` if we decide to skip creating a `MemberExpr *` for `s` and `f`. If there is any possibility that the bounds of `f` may depend on `M`, we have to create a `MemberExpr *` for `s` and `f`, use the created `MemberExpr *` to concretize and expand the declared bounds of `f`, and traverse the concrete, expanded bounds of `f` to look for uses of `M`.

### Definitions

**Definition**: A type `T` is **associated** with a record declaration `S` if:

1. `T` is `struct S` or `union S`, or:
2. `T` is a pointer type `T1 *`, `_Ptr<T1>`, `_Array_ptr<T1>`, or `_Nt_array_ptr<T1>`, and `T1` is associated with `S`, or:
3. `T` is an array type `T1[]` and `T1` is associated with `S`.

**Definition**: Let `F` be a field declaration in a record declaration `S`. The **sibling fields** of `F` are the set of all field declarations in `S` (including the field `F`, which is considered a sibling of itself).

**Definition**: Let `B` be the declared bounds of a field `F`. The **concrete** bounds of a member expression `Base->F` are a bounds expression where, for each `DeclRefExpr` `V` in `B`, `V` is replaced with `Base->V`. Note that the declaration of `V` must be a `FieldDecl` that is a sibling of `F`.

For example:

```
struct S {
  int i;
  int j;
  struct S *s;
  _Array_ptr<int> p : bounds(p, p + i + s->j); // The DeclRefExprs in the declared bounds of p are p, i, and s.
};
```

If `s` is a `DeclRefExpr` of type `struct S *`, then the following expressions would have the following concrete bounds:

| **Member expression** | **Base expression** | **Concrete bounds**                                          |
| --------------------- | ------------------- | ------------------------------------------------------------ |
| `s->p`                | `s`                 | `bounds(s->p, s->p + s->i + s->s->j)`                        |
| `s[0].p`              | `s[0]`              | `bounds(s[0].p, s[0].p + s[0].i + s[0].s->j)`                |
| `s->s->p`             | `s->s`              | `bounds(s->s->p, s->s->p + s->s->i + s->s->s->j)`            |
| `((struct S){ }).p`   | `((struct S){ })`   | `bounds(((struct S){ }).p, ((struct S){ }).p + ((struct S){ }).i + ((struct S){ }).s->j)` |

**Definition**: Let `F` and `G` be field declarations (`FieldDecls`) in a record declaration `S`. We say that `F` **appears** in the bounds of `G` if:

1. The (non-concrete, non-expanded) declared bounds of `G` use the value of a `DeclRefExpr *` `V`, and:
2. The declaration of `V` is `F` (i.e. `V->getDecl() == F`).

In addition, if the declared bounds of a field `G` are `count(e)` or `byte_count(e)`, then `G` appears in its own declared bounds.

Note: this definition does not consider `F` to appear in the bounds of `G` if `G` uses the value of a member expression of the form `E->F` or `E.F`. For example:

```
struct S {
  struct S *s;
  int i;
  _Array_ptr<int> p : count(s->i);
};
```

The fields `s` and `p` appear in the bounds of `p`, but the field `i` does not appear in the bounds of `p`.

### Nested sibling fields in bounds

**Claim**: Consider a call to `SynthesizeMembers` on an expression `E` of the form `EBase.EField` and a member expression `M` of the form `MBase.MField`. Let `F` be a sibling field of `EField`. If the (concrete, expanded) target bounds of `EBase.F` use the value of `M`, then `EField` must appear in the bounds of `F` (according to the above definition of "appears").

Note: `MemberExprs` can be of the form `Base.Field` or `Base->Field`. We can always write `Base->Field` in the form `(*Base).Field`. The proof below uses `MemberExprs` of the form `Base.Field`.

**Proof**: Consider a call to `SynthesizeMembers(EBase.EField, MBase.MField)`. Let `F` be a sibling field of `EField`.

`MBase.MField` can be written as `EBase.EField.....En.MField`, where `n >= 0` and each `Ei` is an `Expr`. (In the initial call to `SynthesizeMembers`, `n` is `0` and `EBase.EField` is `MBase.MField`).

Let the declared bounds of the field `F` be `D`. Let the concrete, expanded target bounds of `EBase.F` be `B`. When `D` is concretized and expanded to produce `B`, each `DeclRefExpr` `V` in `D` is replaced with `EBase.F`. We show that, if `MBase.MField` occurs in `B`, then at least one of the following must be true:

1. [Case 1] `F == EField`, and `D` is a `count` or `byte_count` bounds expression. (In this case, `EField` implicitly appears in its own declared bounds).
2. [Case 2] There is at least one `DeclRefExpr` `W` in `D` where the declaration of `W` is `EField`.

We show `![Case1] => [Case 2]`. Suppose `F != EField` or `D` is not a `count` or `byte_count` bounds expression. We show there must be at least one `DeclRefExpr` `W` in `D` where the declaration of `W` is `EField`.

1. Base case: `n == 0`, so `MBase.MField == EBase.EField`.

   If `V` is a `DeclRefExpr` that occurs in `D`, then `B` will contain `EBase.VField`, where `VField` is the declaration of `V`. If `B` contains `EBase.EField`, there must be a `DeclRefExpr` `W` in `D` where the declaration of `W` is `EField`.

1. Recursive case: `n > 0`, so `MBase.MField == EBase.EField.....En.MField`.

   If `E1.E2` is a `MemberExpr` that occurs in `D`, then `B` will contain `EBase.E1.E2`. If `B` contains `EBase.EField.....En.MField`, there must be a `MemberExpr` `W.....En.MField` in `D`, where `W` is a `DeclRefExpr` whose declaration is `EField`.

**Example**:

In the following example, `len` and `s` are sibling fields of `p` that occur in the declared bounds of `p` via the use of a `MemberExpr`, but do not appear in the bounds of `p` according to the above definition of "appears". For call to `SynthesizeMembers(EBase.EField, M)` where `EField` is `len` or `r`, `M` does not appear in the concrete, expanded bounds of `EBase.p`, so there is no need to create a `MemberExpr` `EBase.p`.

```
struct S;

struct R {
	// BoundsSiblingFields[s]: { }.
	struct S *s;
};

struct S {
	// BoundsSiblingFields[r]: { p }.
	struct R *r;
	
	// BoundsSiblingFields[len]: { }.
	int len;
	
	// BoundsSiblingFields[p]: { p }.
	_Array_ptr<int> p : bounds(p, p + r->s->len);
};

void f(struct S *S) {
	// MBase: s[0].r[0].s[0], MField: len.
	// EBase: s[0].r[0].s[0], EField: len.
	//   len occurs in the declared bounds of p via a MemberExpr, but s does not appear in the bounds of p.
	//   Concrete, expanded bounds of s[0].r[0].s[0].p: bounds(s[0].r[0].s[0].p, s->r->s->p + s->r->s->r->s->len).
	//   s->r->s->len does not occur in the bounds of s->r->s->p.
	//   Skip creating a MemberExpr for s->r->s->p.
	// EBase: s[0].r, EField: s.
	//   s occurs in the declared bounds of p via a MemberExpr, but s does not appear in the bounds of p.
	//   Concrete, expanded bounds of s->r->p: bounds(s->r->p, s->r->p + s->r->r->s->len).
	//   s->r->s->len does not occur in the bounds of s->r->p.
	//   Skip creating a MemberExpr for s->r->p.
	// EBase: s[0], EField: r.
	//   r appears in the declared bounds of p. Create a MemberExpr for s->p.
	//   Concrete, expanded bounds of s->p: bounds(s->p, s->p + s->r->s->len).
	//   s->r->s->len occurs in the bounds of s->p. Create an AbstractSet for s->p.
	s->r->s->len = 0;
}
```

### Implementation details

**Gathering bounds sibling fields:**

During the Checked C prepass which runs on a function, we can compute a mapping from `FieldDecl * => set<FieldDecl *>`. (Call this map `BoundsSiblingFields`.) For a given `FieldDecl *` `F` which belongs to a struct definition `struct S`, `BoundsSiblingFields[F]` will contain all `FieldDecls` in `struct S` in whose (non-concrete, non-expanded) declared bounds `F` appears.

When the prepass is run on a function `f`, the prepass will fill in the `BoundsSiblingFields` map for each `FieldDecl *` `F` that belongs to a record declaration `S` if:

1. `f` declares a variable whose type is associated with `S`, or:
2. The body of `f` contains a call expression `g()` whose type is associated with `S`, or:
3. The body of `f` contains a Checked C temporary binding expression whose type is associated with `S`.

**Using bounds sibling fields:**

Consider a call to `SynthesizeMembers` with arguments `E` and `M`, where `E` is a `MemberExpr *` of the form `EBase->EField` (or `EBase.EField`) and `M` is a `MemberExpr *` that is modified via an assignment. Let `EFieldSiblings` be `BoundsSiblingFields[EField]` (i.e. the set of all sibling fields of `EField` in whose declared bounds `EField` appears). For each `FieldDecl *` `F` in `EFieldSiblings`:

1. Create a `MemberExpr *` of the form `EBase->F` or `EBase.F`. Call this `MemberExpr *` `N`.
2. Compute the concrete, expanded target bounds of `N`. Call these bounds `B`.
3. If `M` appears in `B`:
   1. Create an `AbstractSet` for `N` and add the `AbstractSet` to the result set of `AbstractSets`.

If a sibling field `G` of `EField` does not appear in `BoundsSiblingFields[EField]`, this means we can prove that the (concrete, expanded) target bounds of `G` do not depend on `M`, without needing to actually concretize or expand the declared bounds of `G`. We also do not to create an `AbstractSet` for `EBase->G` (or `EBase.G`) if the bounds of `G` do not depend on `M`, so there is no need to create a `MemberExpr *` for `EBase` and `G`.

In this approach, if a sibling field `G` does appear in `BoundsSiblingFields[EField]` (i.e. we cannot determine that we can safely skip creating a `MemberExpr *` for `EBase` and `G`), this does not guarantee that we will need to create an `AbstractSet` for `EBase` and `G` (i.e. it is not guaranteed that the concrete, expanded bounds of `G` do depend on `M`). The goal of this approach is to reduce the number of unnecessary `MemberExprs` we create, while keeping the complexity of the information gathered in the Checked C prepass to a reasonable level. We may not be able to eliminate all unnecessary creations of `MemberExprs` using this approach, but we can soundly eliminate some of them without overly complicating the prepass.

**Example:**

```
void f(struct A *a) // The parameter declaration 'a' has type struct A *.
                    // Fill in the BoundsSiblingFields map for each field declaration in struct A.
{
  struct B *b = 0; // The local variable declaration 'b' has type struct B *.
                   // Fill in the BoundsSiblingFields map for each field declaration in struct B.
  (struct C){ 0 }; // The temporary binding expression 'TempBinding((struct C){ 0 })' has type struct C.
                 // Fill in the BoundsSiblingFields map for each field declaration in struct C.
}
```

After performing the prepass on the function `f`, the `BoundsSiblingFields` map will have the following contents:

| Struct definition | Field declaration | **Sibling fields in whose declared bounds Field appears** |
| ----------------- | ----------------- | --------------------------------------------------------- |
| A                 | `b`               | `p`                                                       |
| A                 | `i`               | `r`                                                       |
| A                 | `j`               |                                                           |
| A                 | `start`           | `q`                                                       |
| A                 | `end`             | `q`                                                       |
| A                 | `p`               | `p`                                                       |
| A                 | `q`               | `r`                                                       |
| A                 | `r`               |                                                           |
| B                 | `c`               | `q`                                                       |
| B                 | `i`               |                                                           |
| B                 | `q`               | `q`                                                       |
| C                 | `len`             | `r`                                                       |
| C                 | `r`               | `r`                                                       |

After the declarations in the function `f`, suppose `f` continues with the following assignments.

Check for `EField` in `BoundsSiblingFields` of a field `F`:

1. If `BoundsSiblingFields[G]` is the set of all fields that appear in the bounds of `G`:
   1. For each field `F` in the parent of `EField`:
      1. Let `Sibs` = `BoundsSiblingFields[F]`.
      2. If `Sibs` contains`EField`: create a `MemberExpr *` for `EBase->F`
2. If `BoundsSiblingFields[G]` is the set of all fields in whose bounds `G` appears:
   1. Let `Sibs` = `BoundsSiblingFields[EField]`
   2. For each field `F` in `Sibs`: create a `MemberExpr *` for `EBase->F`

In the comments below, the current `M` argument to `SynthesizeMembers` is of the form `MBase->MField`. The current `E` argument to `SynthesizeMembers` is `EBase->EField`. In a call to `SynthesizeMembers`, we create a `MemberExpr *` for each field in `BoundsSiblingFields[EField]` (i.e. all sibling fields of `EField` in whose declared bounds `EField` appears). We do not create a `MemberExpr *` for any sibling field of `EField` in whose declared bounds `EField` does not appear.

```
void f(struct A *a) {
	...
	
	// MBase: a, MField: j.
	// EBase: a, EField: j. BoundsSiblingFields[j]: { }.
	// Do not create MemberExprs for a->p, a->q, or a->r.
	a->j = 0;
	
	// MBase: a->b, MField: j.
	// EBase: a->b, EField: j. BoundsSiblingFields[j]: { }.
	// Do not create a MemberExpr for a->b->q.
	// EBase: a, EField: b. BoundsSiblingFields[b]: p.
	//   Create a MemberExpr for a->p.
	//   Concrete, expanded bounds of a->p: bounds(a->p, a->p + a->b->c->len).
	//   a->b->j does not appear in the concrete, expanded bounds of a->p.
	//   Do not create an AbstractSet for a->p.
	// Do not create MemberExprs for a->q or a->r.
	a->b->j = 0;
	
	// MBase: a, MField: start.
	// EBase: a, EField: start. BoundsSiblingFields[start]: q.
	//   Create a MemberExpr for a->q.
	//   Concrete, expanded bounds of a->q: bounds(a->start, a->end).
	//   a->start appears in the concrete, expanded bounds of a->q.
	//   Create an AbstractSet for a->q.
	// Do not create MemberExprs for a->p or a->r.
	a->start = 0;
	
	// MBase: a->b->c, MField: len.
	// EBase: a->b->c, EField: len. BoundsSiblingFields[len]: r.
	//   Create a MemberExpr for a->b->c->r.
	//   Concrete, expanded bounds of a->b->c->r: bounds(a->b->c->r, a->b->c->r + a->b->c->len).
	//   a->b->c->len appears in the concrete, expanded bounds of a->b->c->r.
	//   Create an AbstractSet for a->b->c->r.
	// EBase: a->b, EField: c. BoundsSiblingFields[c]: q.
	//   Create a MemberExpr for a->b->q.
	//   Concrete, expanded bounds of a->b->q: bounds(a->b->q, a->b->q + a->b->c->len).
	//   a->b->c->len appears in the concrete, expanded bounds of a->b->q.
	//   Create an AbstractSet for a->b->q.
	// EBase: a, EField: b. BoundsSiblingFields[b]: p.
	//   Create a MemberExpr for a->p.
	//   Concrete, expanded bounds of a->p: bounds(a->p, a->p + a->b->c->len).
	//   a->b->c->len appears in the concrete, expanded bounds of a->p.
	//   Create an AbstractSet for a->p.
	// Do not create MemberExprs for a->q or a->r.
	a->b->c->len = 0;
}
```





