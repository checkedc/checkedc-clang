// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- | %clang -c -Xclang -verify -Wno-unused-value -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/range_bounds.c -- | diff %t.checked/range_bounds.c -

#include<stdlib.h>

void test0(size_t l) {
  // Would get bounds, but there's pointer arithmetic. Now we generate a fresh
  // lower bound and use it in range bounds.
  int *p = malloc(l * sizeof(int));
  // CHECK_ALL: _Array_ptr<int> __3c_lower_bound_p : count(l) = malloc<int>(l * sizeof(int));
  // CHECK_ALL: _Array_ptr<int> p : bounds(__3c_lower_bound_p, __3c_lower_bound_p + l) = __3c_lower_bound_p;
  p++;

  // No bounds are inferred, but pointer arithemtic is used; don't split
  int *q = 0;
  // CHECK_ALL: _Array_ptr<int> q = 0;
  q++;
}

// Parameters must be inserted inside function body. This also also checks
// that a pre-declaration gets the correct bounds and does not generate a
// second alias. In this case, the predeclaration doesn't need to use the new
// variable name, but it doesn't hurt and is required in some more complex
// cases.
void test1(int *a, int l);
// CHECK_ALL: void test1(_Array_ptr<int> __3c_lower_bound_a : count(l), int l);
void test1(int *a, int l) {
  // CHECK_ALL: void test1(_Array_ptr<int> __3c_lower_bound_a : count(l), int l) _Checked {
  // CHECK_ALL: _Array_ptr<int> a : bounds(__3c_lower_bound_a, __3c_lower_bound_a + l) = __3c_lower_bound_a;

  // Also check that other types of assignment are recognized.
  a = a + 1;

  // The increment above means this loop reads out of bounds if `l` is the
  // length of `a`.  3c won't consider this, but, now that we give `a` a bound,
  // the access `a[l-1]` can be caught by Checked C, and the programmer can
  // correct the loop limit or the declared bound as appropriate.
  for(int i = 0; i < l; i++)
    a[i];
}

// Also check for itypes. They're interesting because the alias isn't checked.
void test2(int *a, int l);
// CHECK_ALL: void test2(int *__3c_lower_bound_a : itype(_Array_ptr<int>) count(l), int l);
void test2(int *a, int l) {
  // CHECK_ALL: void test2(int *__3c_lower_bound_a : itype(_Array_ptr<int>) count(l), int l) {
  // CHECK_ALL: int *a  = __3c_lower_bound_a;
  for(int i = 0; i < l; i++)
    a[i];

  a = a + 2;
  a = (int*) 1;
}

// Something more complex with multiple parameters.
void test3(int *a, int *b, int *c, int *d) {
  // CHECK_ALL: void test3(_Array_ptr<int> __3c_lower_bound_a : count(10), int *b : itype(_Array_ptr<int>) bounds(__3c_lower_bound_d, __3c_lower_bound_d + 10), _Array_ptr<int> __3c_lower_bound_c : count(10), int *__3c_lower_bound_d : itype(_Array_ptr<int>) count(10)) {
  // CHECK_ALL: _Array_ptr<int> a : bounds(__3c_lower_bound_a, __3c_lower_bound_a + 10) = __3c_lower_bound_a;
  // CHECK_ALL: _Array_ptr<int> c : bounds(__3c_lower_bound_c, __3c_lower_bound_c + 10) = __3c_lower_bound_c;
  // CHECK_ALL: int *d = __3c_lower_bound_d;
  a += 1, b += 2, c--, d -= 1;
  b = d = (int*) 1;

  for (int i = 0; i < 10; i++)
    a[i], b[i], c[i], d[i];
}

// Multi-declarations might need to add new declarations. The order of the new
// declarations is important because a later declaration in the same multi-decl
// might reference the variable being emitted. The new declaration of `c` must
// come before `d`.
void test4() {
  int *a = malloc(10*sizeof(int)), b, *c = malloc(10*sizeof(int)), *d = c;
  // CHECK_ALL: _Array_ptr<int> __3c_lower_bound_a : count(10) = malloc<int>(10*sizeof(int));
  // CHECK_ALL: _Array_ptr<int> a : bounds(__3c_lower_bound_a, __3c_lower_bound_a + 10) = __3c_lower_bound_a;
  // CHECK_ALL: int b;
  // CHECK_ALL: _Array_ptr<int> __3c_lower_bound_c : count(10) = malloc<int>(10*sizeof(int));
  // CHECK_ALL: _Array_ptr<int> c : bounds(__3c_lower_bound_c, __3c_lower_bound_c + 10) = __3c_lower_bound_c;
  // CHECK_ALL: _Ptr<int> d = c;

  b;
  a++, c++;

  // This is another bit of tricky multi-decl rewriting. There are be spaces or
  // comments between the end of one declaration and the beginning of the next.
  // The fresh lower bound needs to be inserted after the comma delimiting the
  // declarations.
  int *x = malloc(5 * sizeof(int)) , *y = malloc(2 * sizeof(int)) /*foo*/, z;
  // CHECK_ALL: _Array_ptr<int> __3c_lower_bound_x : count(5) = malloc<int>(5 * sizeof(int)) ;
  // CHECK_ALL: _Array_ptr<int> x : bounds(__3c_lower_bound_x, __3c_lower_bound_x + 5) = __3c_lower_bound_x;
  // CHECK_ALL: _Array_ptr<int> __3c_lower_bound_y : count(2) = malloc<int>(2 * sizeof(int)) /*foo*/;
  // CHECK_ALL: _Array_ptr<int> y : bounds(__3c_lower_bound_y, __3c_lower_bound_y + 2) = __3c_lower_bound_y;
  // CHECK_ALL: int z;

  x++;
  y++;
}

// Test that bounds don't propagate through pointers that are updated with
// pointer arithmetic. In this example, `b` can *not* have bounds `count(2)`,
// but it can get `bounds(__3c_lower_bound_a, __3c_lower_bound_a + 2)`.  The same restriction
// also applies to bounds on the return, but, for the return, `a` can't be used
// as a lower bound, so no bound is given.
int *test5() {
  // CHECK_ALL: _Array_ptr<int> test5(void) {
  int *a = malloc(2 * sizeof(int));
  // CHECK_ALL: _Array_ptr<int> __3c_lower_bound_a : count(2) = malloc<int>(2 * sizeof(int));
  // CHECK_ALL: _Array_ptr<int> a : bounds(__3c_lower_bound_a, __3c_lower_bound_a + 2) = __3c_lower_bound_a;
  a++;
  int *b = a;
  // CHECK_ALL: _Array_ptr<int> b : bounds(__3c_lower_bound_a, __3c_lower_bound_a + 2) = a;
  b[0];

  return a;
}

// Assignments to the variable should update the original and the copy, as long
// as the value being assigned doesn't depend on the pointer.
void test6() {
  int *p = malloc(10 * sizeof(int));
  // CHECK_ALL: _Array_ptr<int> __3c_lower_bound_p : count(10) = malloc<int>(10 * sizeof(int));
  // CHECK_ALL: _Array_ptr<int> p : bounds(__3c_lower_bound_p, __3c_lower_bound_p + 10) = __3c_lower_bound_p;
  p++;

  // This assignment isn't touched because `p` is on the RHS.
  p = p + 1;
  // CHECK_ALL: p = p + 1;

  // Null out `p`, so we need to null the original and the duplicate.
  p = 0;
  // CHECK_ALL: __3c_lower_bound_p = 0, p = __3c_lower_bound_p;

  // A slightly more complex update to a different pointer value.
  int *q = malloc(10 * sizeof(int));
  p = q;
  //CHECK_ALL: _Array_ptr<int> q : count(10) = malloc<int>(10 * sizeof(int));
  //CHECK_ALL: __3c_lower_bound_p = q, p = __3c_lower_bound_p;

  // Don't treat a call to realloc as pointer arithmetic. Freeing `p` after
  // `p++` is highly questionable, but that's not the point here.
  p = realloc(p, 10 * sizeof(int));
  // CHECK_ALL: __3c_lower_bound_p = realloc<int>(p, 10 * sizeof(int)), p = __3c_lower_bound_p;

  // Assignment rewriting should work in more complex expression and around
  // other 3C rewriting without breaking anything.
  int *v = 1 + (p = (int*) 0, p = p + 1) + 1;
  // CHECK_ALL: _Ptr<int> v = 1 + (__3c_lower_bound_p = (_Array_ptr<int>) 0, p = __3c_lower_bound_p, p = p + 1) + 1;
}


// Check interaction with declaration merging. Identifiers are added on the
// first two declarations even though it's not required.
void test7(int *);
void test7();
void test7(int *a);
// CHECK_ALL: void test7(_Array_ptr<int> __3c_lower_bound_s : count(5));
// CHECK_ALL: void test7(_Array_ptr<int> __3c_lower_bound_s : count(5));
// CHECK_ALL: void test7(_Array_ptr<int> __3c_lower_bound_s : count(5));
void test7(int *s) {
// CHECK_ALL: void test7(_Array_ptr<int> __3c_lower_bound_s : count(5)) _Checked {
// CHECK_ALL: _Array_ptr<int> s : bounds(__3c_lower_bound_s, __3c_lower_bound_s + 5) = __3c_lower_bound_s;
  s++;
  for (int i = 0; i < 5; i++)
    s[i];
}

// A structure field is handled as it was before implementing lower bound
// inference. Future work could insert a new field, and update all struct
// initializer to include it.
struct s {
  int *a;
  // CHECK_ALL: _Array_ptr<int> a;
};
void test8() {
  struct s t;
  t.a++;
  t.a[0];
  // expected-error@-1 {{expression has unknown bounds}}
}

// Same as above. Future work might figure out how to generate fresh lower
// bounds for global variables.
int *glob;
// CHECK_ALL: _Array_ptr<int> glob = ((void *)0);
void test9() {
  glob++;
  glob[0];
  // expected-error@-1 {{expression has unknown bounds}}
}

// Creating a temporary local `int a _Checked[10]` would be incorrect here
// because the local variable array type does not decay to a pointer type.
// Future work can instead use checked array pointer type for the local.
void test10(int a[10]) {
// CHECK_ALL: void test10(int a _Checked[10]) _Checked {
// expected-note@-2 {{}}

  // A warning is expected because we keep the bound for the checked array. It
  // could instead be rewritten to `_Array_ptr<int> a` without a bound.
  a++;
  // expected-warning@-1 {{cannot prove declared bounds for 'a' are valid after increment}}
  // expected-note@-2 {{}}
}

// Another case where fresh lower bounds can't be generated: if we would have
// to update an assignment expression in a macro (which would be a rewriting
// error), then the pointer cannot get fresh lower bound.
#define set_a_to_null a = 0
#define null_with_semi 0;
#define another_macro d =
void test11(size_t n){
  int *a = malloc(sizeof(int) * n);
  //CHECK: _Array_ptr<int> a = malloc<int>(sizeof(int) * n);
  a++;
  set_a_to_null;

  // The RHS is a macro, but we should still be able to rewrite.
  int *b = malloc(sizeof(int) * n);
  // CHECK: _Array_ptr<int> __3c_lower_bound_b : count(n) = malloc<int>(sizeof(int) * n);
  // CHECK: _Array_ptr<int> b : bounds(__3c_lower_bound_b, __3c_lower_bound_b + n) = __3c_lower_bound_b;
  b++;
  b = NULL;
  // CHECK: __3c_lower_bound_b = NULL, b = __3c_lower_bound_b;

  // Like the above case, but the macro includes the semicolon, so we can't
  // rewrite.
  int *c = malloc(sizeof(int) * n);
  // CHECK: _Array_ptr<int> c = malloc<int>(sizeof(int) * n);
  c++;
  c = null_with_semi

  int *d = malloc(sizeof(int) * n);
  // CHECK: _Array_ptr<int> d = malloc<int>(sizeof(int) * n);
  d++;
  another_macro 0;
}

// Check byte_count rewriting. Interesting because range bound needs a cast to
// `_Array_ptr<char>` for pointer arithmetic with offset to be correct.
void byte_count_fn(int *a : byte_count(n), unsigned int n);
void test12(int *b, unsigned int n) {
// CHECK: void test12(_Array_ptr<int> __3c_lower_bound_b : byte_count(n), unsigned int n) _Checked {
// CHECK: _Array_ptr<int> b : bounds(((_Array_ptr<char>)__3c_lower_bound_b), ((_Array_ptr<char>)__3c_lower_bound_b) + n) = __3c_lower_bound_b;

  byte_count_fn(b, n);
  b++;

  // And also check count-plus-ones bounds.
  int *c;
  // _Array_ptr<int> __3c_lower_bound_c : count(0 + 1) = ((void *)0);
  // _Array_ptr<int> c : bounds(__3c_lower_bound_c, __3c_lower_bound_c + 0 + 1) = __3c_lower_bound_c;
  c[0];
  c++;
}
