// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -itypes-for-extern -alltypes -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -itypes-for-extern -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -itypes-for-extern -alltypes -addcr %s -- -Wno-error=int-conversion | %clang -c -Wno-error=int-conversion -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -itypes-for-extern -alltypes -output-dir=%t.checked %s -- -Wno-error=int-conversion
// RUN: 3c -base-dir=%t.checked -itypes-for-extern -alltypes %t.checked/itypes_for_extern.c -- -Wno-error=int-conversion | diff %t.checked/itypes_for_extern.c -

// Simplest test case: a would normally get a checked type, but is given an
// itype because of the flag.
void foo(int *a) {}
//CHECK: void foo(int *a : itype(_Ptr<int>)) _Checked {}

// Since static function can't be included in other translation units, these
// can stay fully checked.
static void static_foo(int *a) {}
//CHECK: static void static_foo(_Ptr<int> a) _Checked {}

// Don't give a function an itype if it wouldn't normally be checked
void undef_foo(int *a);
//CHECK: void undef_foo(int *a);

int *bar() { return 0; }
//CHECK: int *bar(void) : itype(_Ptr<int>) _Checked { return 0; }

int *baz(int *a, int len, int *b) {
//CHECK_ALL: int *baz(int *a : itype(_Array_ptr<int>) count(len), int len, int *b : itype(_Ptr<int>)) : itype(_Array_ptr<int>) count(len) _Checked {
//CHECK_NOALL: int *baz(int *a : itype(_Ptr<int>), int len, int *b : itype(_Ptr<int>)) : itype(_Ptr<int>) {
  for (int i = 0; i < len; i++)
    a[i];
  return a;
}

void buz(int *(*f)(int *, int *)) {}
//CHECK: void buz(int *((*f)(int *, int *)) : itype(_Ptr<_Ptr<int> (_Ptr<int>, _Ptr<int>)>)) _Checked {}

typedef int * int_star;
void typedef_test(int_star p) {}
//CHECK: typedef int * int_star;
//CHECK: void typedef_test(int_star p : itype(_Ptr<int>)) _Checked {}

typedef void (*fn)(int *);
void fn_typedef_test(fn f) {}
//CHECK: typedef void (*fn)(int *);
//CHECK: void fn_typedef_test(fn f : itype(_Ptr<void (_Ptr<int>)>)) _Checked {}

struct foo {
  int *a;
  void (*fn)(int *);
  int *b, **c;
};
//CHECK: int *a : itype(_Ptr<int>);
//CHECK: void ((*fn)(int *)) : itype(_Ptr<void (_Ptr<int>)>);
//CHECK: int *b : itype(_Ptr<int>);
//CHECK: int **c : itype(_Ptr<_Ptr<int>>);

int *glob = 0;
extern int *extern_glob = 0;
static int *static_glob = 0;
//CHECK: int *glob : itype(_Ptr<int>) = 0;
//CHECK: extern int *extern_glob : itype(_Ptr<int>) = 0;
//CHECK: static _Ptr<int> static_glob = 0;

void (*glob_fn)(int *) = 0;
extern void (*extern_glob_fn)(int *) = 0;
static void (*static_glob_fn)(int *) = 0;
//CHECK: void ((*glob_fn)(int *)) : itype(_Ptr<void (_Ptr<int>)>) = 0;
//CHECK: extern void ((*extern_glob_fn)(int *)) : itype(_Ptr<void (_Ptr<int>)>) = 0;
//CHECK: static _Ptr<void (_Ptr<int>)> static_glob_fn = 0;

int_star typedef_glob = 0;
fn typedef_fn_glob = 0;
//CHECK: int_star typedef_glob : itype(_Ptr<int>) = 0;
//CHECK: fn typedef_fn_glob : itype(_Ptr<void (_Ptr<int>)>) = 0;

struct typedef_struct {
  int_star a;
  fn f;
};
//CHECK: int_star a : itype(_Ptr<int>);
//CHECK: fn f : itype(_Ptr<void (_Ptr<int>)>);

// Testing some cases where the itypes already exist. Earlier we made a change
// that lets itypes re-solve to checked types. That shouldn't happen with this
// flag. This is also covered by the idempotence check, but I wanted to make it
// explicit.

void has_itype0(int *a : itype(_Ptr<int>)) { a = 1; }
//CHECK: void has_itype0(int *a : itype(_Ptr<int>)) { a = 1; }

void has_itype1(int *a : itype(_Ptr<int>)) { a = 0; }
//CHECK: void has_itype1(int *a : itype(_Ptr<int>)) _Checked { a = 0; }

// Test rewriting itypes for constant sized arrays. As with function pointers,
// part of the type (the array size) occurs after the name of the variable
// being declared. This complicates rewriting. These examples caused errors in
// libjpeg.
//
// multivardecls_complex_types.c has tests of some similar cases as part of
// multi-decls, with and without -itypes-for-extern.

int const_arr0[10];
//CHECK_ALL: int const_arr0[10] : itype(int _Checked[10]);
//CHECK_NOALL: int const_arr0[10];

int *const_arr1[10];
//CHECK_ALL: int *const_arr1[10] : itype(_Ptr<int> _Checked[10]) = {((void *)0)};
//CHECK_NOALL: int *const_arr1[10];

int (*const_arr2)[10];
//CHECK_ALL: int (*const_arr2)[10] : itype(_Ptr<int _Checked[10]>) = ((void *)0);
//CHECK_NOALL: int (*const_arr2)[10] : itype(_Ptr<int[10]>) = ((void *)0);

// Itypes for constants sized arrays when there is a declaration with and
// without a parameter list take slightly different paths that need to be
// tested. If there is no parameter list, then the unchecked component of the
// itype can't be copied from the declaration, and it instead must be generated
// from the constraint variable.

void const_arr_fn();
void const_arr_fn(int a[10]) {}
//CHECK_ALL: void const_arr_fn(int *a : itype(int _Checked[10]));
//CHECK_ALL: void const_arr_fn(int *a : itype(int _Checked[10])) _Checked {}

// Rewriting an existing itype or bounds expression on a global variable. Doing
// this correctly requires replacing text until the end of the Checked C
// annotation expression. The m_* and s_* tests in multivardecls_complex_types.c
// test some similar cases in combination with multi-decls.
int *a : itype(_Ptr<int>);
int **b : itype(_Ptr<int *>);
int *c : count(2);
int **d : count(2);
int **e : itype(_Array_ptr<int *>) count(2);
int **f : count(2) itype(_Array_ptr<int *>);
int **g : count(2) itype(_Array_ptr<int *>) = 0;
//CHECK: int *a : itype(_Ptr<int>);
//CHECK: int **b : itype(_Ptr<_Ptr<int>>) = ((void *)0);
//CHECK: int *c : count(2);
//CHECK: int **d : itype(_Array_ptr<_Ptr<int>>) count(2) = ((void *)0);
//CHECK: int **e : itype(_Array_ptr<_Ptr<int>>) count(2) = ((void *)0);
//CHECK: int **f : itype(_Array_ptr<_Ptr<int>>) count(2) = ((void *)0);
//CHECK: int **g : itype(_Array_ptr<_Ptr<int>>) count(2) = 0;

// `a` gets a fresh lower bound because of the update `a = a + 2`. The fresh
// bound for itype parameters uses an unchecked type outside of
// -itypes-for-extern, but needs to still use a checked type with
// -itypes-for-extern to avoid type errors on assignment to checked pointers
// inside the function.
void test_fresh_lower_bound(int *a, int l) {
// CHECK_ALL: void test_fresh_lower_bound(int *__3c_lower_bound_a : itype(_Array_ptr<int>) count(l), int l) _Checked {
// CHECK_ALL: _Array_ptr<int> a : bounds(__3c_lower_bound_a, __3c_lower_bound_a + l) = __3c_lower_bound_a;
  for(int i = 0; i < l; i++)
    a[i];
  a = a + 2;
  int *b = a;
  // CHECK_ALL: _Ptr<int> b = a;
}

void test_fresh_lower_bound_itype(int *a, int l) {
// CHECK_ALL: void test_fresh_lower_bound_itype(int *__3c_lower_bound_a : itype(_Array_ptr<int>) count(l), int l) {
// CHECK_ALL: int *a = __3c_lower_bound_a;
  for(int i = 0; i < l; i++)
    a[i];
  a = a + 2;
  a = 1;
  int *b = a;
}
