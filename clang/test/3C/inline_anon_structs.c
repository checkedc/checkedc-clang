// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked %t.checked/inline_anon_structs.c -- | diff %t.checked/inline_anon_structs.c -

#include <stddef.h>
extern _Itype_for_any(T) void *malloc(size_t size)
    : itype(_Array_ptr<T>) byte_count(size);

/*This code ensures conversion happens as expected when
an inlinestruct and its associated VarDecl have different locations*/
int valuable;

static struct foo {
  const char *name;
  //CHECK_NOALL: const char *name;
  //CHECK_ALL:   _Ptr<const char> name;
  int *p_valuable;
  //CHECK: _Ptr<int> p_valuable;
} array[] = {{"mystery", &valuable}};

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
  /*the fields of the anonymous struct are free to be marked checked*/
  int *data;
  //CHECK_NOALL: int *data;

  /* but the actual pointer can't be when alltypes is disabled */
  /* when alltypes is enabled, this whole structure is rewritten
   improperly, but that's OK, because we signal a warning to the user*/
} * x;
//CHECK_ALL: _Ptr<struct> x = ((void *)0);

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

/*be should be made wild, whereas a should be converted*/
struct {
  int *a;
  //CHECK_NOALL: _Ptr<int> a;
} * be[4];

/*this code checks inline structs withiin functions*/
void foo2(int *x) {
  //CHECK: void foo2(_Ptr<int> x) {
  struct bar {
    int *x;
  } *y = 0;
  //CHECK:      struct bar {
  //CHECK-NEXT:   _Ptr<int> x;
  //CHECK-NEXT: };
  //CHECK-NEXT: _Ptr<struct bar> y = 0;

  /*A non-pointer struct without an init will be marked wild*/
  struct something {
    int *x;
  } z;
  //CHECK:      struct something {
  //CHECK-NEXT:   int *x;
  //CHECK-NEXT: } z;

  /*so will ones that are anonymous*/
  struct {
    int *x;
  } a;
  //CHECK:      struct {
  //CHECK-NEXT:   int *x;
  //CHECK-NEXT: } a;

  /*if it have an initializer, the rewriter won't have trouble*/
  struct {
    int *c;
  } b = {};
  //CHECK:      struct {
  //CHECK-NEXT:   _Ptr<int> c;
  //CHECK-NEXT: } b = {};
}
