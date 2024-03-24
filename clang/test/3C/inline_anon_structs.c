// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked %t.checked/inline_anon_structs.c -- | diff %t.checked/inline_anon_structs.c -

#include <stdlib.h>

/*This code ensures conversion happens as expected when
an inlinestruct and its associated VarDecl have different locations*/
int valuable;

// When -alltypes is on, the addition of _Checked to the array triggers
// rewriting of the multi-decl, including splitting of the struct definition.
// When it is off, the multi-decl as such is not rewritten, but in either case,
// the fields of the struct are rewritten as appropriate.
static struct foo {
  // When an inline struct definition is separated from variable declarations,
  // if there was a `static` keyword that applied to the variables, we should
  // remove it from the separated struct (where it is not meaningful).
  //CHECK_NOALL: static struct foo {
  //CHECK_ALL: struct foo {
  const char *name;
  // See https://github.com/correctcomputation/checkedc-clang/issues/470.
  //CHECK_NOALL: const char *name;
  //CHECK_ALL:   _Ptr<const char> name;
  int *p_valuable;
  //CHECK: _Ptr<int> p_valuable;
} array[] = {{"mystery", &valuable}};
// {{...}} in a CHECK directive delimits a regular expression
// (https://llvm.org/docs/CommandGuide/FileCheck.html#filecheck-regex-matching-syntax)
// and there isn't a way to escape that construct itself, so we use a regular
// expression and escape the contents as needed.
//CHECK_NOALL: {{\} array\[\] = \{\{"mystery", &valuable\}\};}}
//CHECK_ALL: {{static struct foo array _Checked\[\] = \{\{"mystery", &valuable\}\};}}

/*This code is a series of more complex tests for inline structs*/
/* a, b, c below all stay as WILD pointers; d can be a _Ptr<...>*/

/* one decl; x rewrites to _Ptr<int> */
struct foo1 {
  int *x;
} * a;
//CHECK:      struct foo1 {
//CHECK-NEXT:   _Ptr<int> x;
//CHECK-NEXT: };
//CHECK-NEXT: _Ptr<struct foo1> a = ((void *)0);

struct baz {
  int *z;
};
//CHECK:      struct baz {
//CHECK-NEXT:   _Ptr<int> z;
//CHECK-NEXT: };
struct baz *d;
//CHECK: _Ptr<struct baz> d = ((void *)0);

struct bad {
  int *y;
} * b, *c;
//CHECK:      struct bad {
//CHECK-NEXT:   int *y;
//CHECK-NEXT: };
//CHECK: _Ptr<struct bad> b = ((void *)0);
//CHECK: _Ptr<struct bad> c = ((void *)0);

/* two decls, y should be converted */
struct bar {
  int *y;
} * e, *f;
//CHECK:      struct bar {
//CHECK-NEXT:   _Ptr<int> y;
//CHECK-NEXT: };
//CHECK: _Ptr<struct bar> e = ((void *)0);
//CHECK: _Ptr<struct bar> f = ((void *)0);

void foo(void) {
  a->x = (void *)0;
  b->y = (int *)5;
  d->z = (void *)0;
}

/*This code tests anonymous structs */
struct {
  //CHECK: struct x_struct_1 {
  /*the fields of the anonymous struct are free to be marked checked*/
  int *data;
  //CHECK_NOALL: int *data;
  //CHECK_ALL: _Array_ptr<int> data : count(4);
} * x;
//CHECK:      };
//CHECK-NEXT: _Ptr<struct x_struct_1> x = ((void *)0);

/*ensure trivial conversion*/
void foo1(int *w) {
  //CHECK: void foo1(_Ptr<int> w) {
  x->data = malloc(sizeof(int) * 4);
  x->data[1] = 4;
}

/*This code tests more complex variable declarations*/
struct alpha {
  int *data;
  //CHECK: _Ptr<int> data;
};
struct alpha *al[4];
//CHECK_NOALL: struct alpha *al[4];
//CHECK_ALL: _Ptr<struct alpha> al _Checked[4] = {((void *)0)};

// A similar test with an unnamed struct.
struct {
  int *a;
  //CHECK: _Ptr<int> a;
} * be[4];
//CHECK_NOALL:    } * be[4];
//CHECK_ALL:      };
//CHECK_ALL-NEXT: _Ptr<struct be_struct_1> be _Checked[4] = {((void *)0)};

// The following is explained in second_tu_fn in inline_anon_structs_cross_tu.c.
struct { int x; } *cross_tu_numbering_test;
//CHECK_AB:      struct cross_tu_numbering_test_struct_1 { int x; };
//CHECK_AB-NEXT: _Ptr<struct cross_tu_numbering_test_struct_1> cross_tu_numbering_test = ((void *)0);
//CHECK_BA:      struct cross_tu_numbering_test_struct_2 { int x; };
//CHECK_BA-NEXT: _Ptr<struct cross_tu_numbering_test_struct_2> cross_tu_numbering_test = ((void *)0);

/*this code checks inline structs withiin functions*/
void foo2(int *x) {
  //CHECK: void foo2(_Ptr<int> x) _Checked {
  struct bar {
    int *x;
  } *y = 0;
  //CHECK:      struct bar {
  //CHECK-NEXT:   _Ptr<int> x;
  //CHECK-NEXT: };
  //CHECK-NEXT: _Ptr<struct bar> y = 0;

  // A similar test with an automatically added struct initializer.
  struct something {
    int *x;
  } z;
  //CHECK:      struct something {
  //CHECK-NEXT:   _Ptr<int> x;
  //CHECK-NEXT: };
  //CHECK-NEXT: struct something z = {};

  // Ditto with an anonymous struct.
  struct {
    int *x;
  } a;
  //CHECK:      struct a_struct_1 {
  //CHECK-NEXT:   _Ptr<int> x;
  //CHECK-NEXT: };
  //CHECK-NEXT: struct a_struct_1 a = {};

  // If the variable already has an initializer, then there is no initializer
  // addition to trigger rewriting and splitting of the struct.
  struct {
    int *c;
  } b = {};
  //CHECK:      struct {
  //CHECK-NEXT:   _Ptr<int> c;
  //CHECK-NEXT: } b = {};

  // Additional regression tests (only checking compilation with no crash) from
  // https://github.com/correctcomputation/checkedc-clang/pull/497.
  struct {
    int *i;
  } * f;
  struct {
    int *il
  } * g, *h, *i;
}

// Tests of new functionality from
// https://github.com/correctcomputation/checkedc-clang/pull/657.

// Test that 3C doesn't mangle the code by attempting to split a forward
// declaration of a struct out of a multi-decl as if it were a definition
// (https://github.com/correctcomputation/checkedc-clang/issues/644).
struct fwd *p;
//CHECK: _Ptr<struct fwd> p = ((void *)0);

// Test the handling of inline TagDecls when the containing multi-decl is
// rewritten:
// - TagDecls are de-nested in postorder whether or not they were originally
//   named, and this does not interfere with rewrites inside those TagDecls
//   (https://github.com/correctcomputation/checkedc-clang/issues/531).
// - Storage and type qualifiers preceding an inline TagDecl are not copied to
//   the split TypeDecl (where they wouldn't be meaningful) but remain
//   associated with the fields to which they originally applied
//   (https://github.com/correctcomputation/checkedc-clang/issues/647).
// - Unnamed TagDecls are automatically named
//   (https://github.com/correctcomputation/checkedc-clang/issues/542).
// This big combined test may be a bit hard to understand, but it may provide
// better coverage of any interactions among these features than separate tests
// would.

// Use up the c_struct_1 name.
struct c_struct_1 {};

static struct A {
  const struct B {
    struct {
      int *c2i;
    } *c;
  } *ab, ab_arr[2];
  volatile struct D {
    struct {
      int *c3i;
    } *c;
    struct E {
      int *ei;
    } *de;
    const enum F { F_0, F_1 } *df;
    enum { G_0, G_1 } *dg;
    struct H {
      int *hi;
    } *dh;
    union U {
      int *ui;
    } *du;
    union {
      int *vi;
    } *dv;
  } *ad;
} *global_a, global_a_arr[2];

void constrain_dh(void) {
  struct D d;
  d.dh = (struct H *)1;
}

// Points of note:
// - We have two unnamed inline structs that get automatically named after a
//   field `c`, but the name `c_struct_1` is taken, so the names `c_struct_2`
//   and `c_struct_3` are assigned.
// - All kinds of TagDecls (structs, unions, and enums) can be moved out of a
//   containing struct and automatically named if needed. In principle, 3C
//   should be able to move TagDecls out of a union, but I couldn't find any way
//   to force a union field to be rewritten in order to demonstrate this, since
//   3C constrains union fields to wild. As far as I know, TagDecls can't be
//   nested in an enum in C.
// - `struct H` does not get moved because there is nothing to trigger rewriting
//   of the containing multi-decl since `dh` is constrained wild by
//   `constrain_dh`.

//CHECK:      struct c_struct_2 {
//CHECK-NEXT:   _Ptr<int> c2i;
//CHECK-NEXT: };
//CHECK-NEXT: struct B {
//CHECK-NEXT:   _Ptr<struct c_struct_2> c;
//CHECK-NEXT: };
//CHECK-NEXT: struct c_struct_3 {
//CHECK-NEXT:   _Ptr<int> c3i;
//CHECK-NEXT: };
//CHECK-NEXT: struct E {
//CHECK-NEXT:   _Ptr<int> ei;
//CHECK-NEXT: };
//CHECK-NEXT: enum F { F_0, F_1 };
//CHECK-NEXT: enum dg_enum_1 { G_0, G_1 };
//CHECK-NEXT: union U {
//CHECK-NEXT:   int *ui;
//CHECK-NEXT: };
//CHECK-NEXT: union dv_union_1 {
//CHECK-NEXT:   int *vi;
//CHECK-NEXT: };
//CHECK-NEXT: struct D {
//CHECK-NEXT:   _Ptr<struct c_struct_3> c;
//CHECK-NEXT:   _Ptr<struct E> de;
//CHECK-NEXT:   _Ptr<const enum F> df;
//CHECK-NEXT:   _Ptr<enum dg_enum_1> dg;
//CHECK-NEXT:   struct H {
//CHECK-NEXT:     _Ptr<int> hi;
//CHECK-NEXT:   } *dh;
//CHECK-NEXT:   _Ptr<union U> du;
//CHECK-NEXT:   _Ptr<union dv_union_1> dv;
//CHECK-NEXT: };
//CHECK-NEXT: struct A {
//CHECK-NEXT:   _Ptr<const struct B> ab;
//CHECK_NOALL-NEXT:   const struct B ab_arr[2];
//CHECK_ALL-NEXT:     const struct B ab_arr _Checked[2];
//CHECK-NEXT:   _Ptr<volatile struct D> ad;
//CHECK-NEXT: };
//CHECK-NEXT: static _Ptr<struct A> global_a = ((void *)0);
//CHECK_NOALL-NEXT: static struct A global_a_arr[2];
//CHECK_ALL-NEXT:   static struct A global_a_arr _Checked[2];

// This case is not intentionally supported but works "by accident" because 3C
// deletes everything between the start location of the first member and the
// start of the TagDecl (here, `_Ptr<`) and replaces everything between the end
// location of the TagDecl and the end of the first member (here,
// `> *chkptr_struct_var`) with the new text of the first member. It may well be
// the right decision to break this case in the future, but it would be nice to
// be aware that we're doing so, and we might want to keep a test that this case
// merely produces wrong output and doesn't crash the rewriter.
_Ptr<struct chkptr_struct { int *x; }> *chkptr_struct_var;
//CHECK:      struct chkptr_struct { _Ptr<int> x; };
//CHECK-NEXT: _Ptr<_Ptr<struct chkptr_struct>> chkptr_struct_var = ((void *)0);

// Tests of the special case where we use the first member of a typedef
// multi-decl as the name of the inline TagDecl.

typedef struct { int *x; } SFOO, *PSFOO;
//CHECK:      typedef struct { _Ptr<int> x; } SFOO;
//CHECK-NEXT: typedef _Ptr<SFOO> PSFOO;

// The other way around, we can't do it because SBAR would be defined too late
// to use it in the definition of PSBAR.
typedef struct { int *x; } *PSBAR, SBAR;
//CHECK:      struct PSBAR_struct_1 { _Ptr<int> x; };
//CHECK-NEXT: typedef _Ptr<struct PSBAR_struct_1> PSBAR;
//CHECK-NEXT: typedef struct PSBAR_struct_1 SBAR;

// Borderline case: Since SFOO_CONST has a qualifier, we don't use it as the
// name of the inline struct. If we did, we'd end up with `typedef
// _Ptr<const SFOO_CONST> PSFOO_CONST`, which still expands to
// `_Ptr<const struct { ... }>`, but the duplicate `const` may be a bit
// confusing to the reader, so we don't do that.
typedef const struct { int *x; } SFOO_CONST, *PSFOO_CONST;
//CHECK:      struct SFOO_CONST_struct_1 { _Ptr<int> x; };
//CHECK-NEXT: typedef const struct SFOO_CONST_struct_1 SFOO_CONST;
//CHECK-NEXT: typedef _Ptr<const struct SFOO_CONST_struct_1> PSFOO_CONST;

// Test that when the outer struct is preceded by a qualifier, de-nesting
// inserts inner structs before the qualifier, not between the qualifier and the
// outer `struct` keyword. This is moot if the outer struct is split as part of
// multi-decl rewriting because multi-decl rewriting will delete the qualifier
// and add it before the first member, but the problem can happen if the
// multi-decl isn't rewritten or the outer struct isn't split because we have a
// typedef for it.

typedef struct { struct q_not_rewritten_inner {} *x; } *q_not_rewritten_outer;
q_not_rewritten_outer onr = (q_not_rewritten_outer)1;
//CHECK:      struct q_not_rewritten_inner {};
//CHECK-NEXT: typedef struct { _Ptr<struct q_not_rewritten_inner> x; } *q_not_rewritten_outer;

typedef struct {
  struct q_typedef_inner {} *x;
} q_typedef_outer, *q_typedef_outer_trigger_rewrite;
//CHECK:      struct q_typedef_inner {};
//CHECK-NEXT: typedef struct {
//CHECK-NEXT:   _Ptr<struct q_typedef_inner> x;
//CHECK-NEXT: } q_typedef_outer;
//CHECK-NEXT: typedef _Ptr<q_typedef_outer> q_typedef_outer_trigger_rewrite;

// As noted in the comment in DeclRewriter::denestTagDecls, when the outer
// struct isn't part of a multi-decl, we don't have an easy way to find the
// location before the (useless) qualifier, so the output is a bit weird but
// still has only a compiler warning, though in a different place than it
// should.
typedef struct { struct q_pointless_inner {} *x; };
//CHECK:      typedef struct q_pointless_inner {};
//CHECK-NEXT: struct { _Ptr<struct q_pointless_inner> x; };
