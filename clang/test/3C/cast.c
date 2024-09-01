// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/cast.c -- | diff %t.checked/cast.c -

int foo(int *p) {
  //CHECK: int foo(_Ptr<int> p) {
  *p = 5;
  int x = (int)p; /* cast is safe */
  return x;
}

void bar(void) {
  int a = 0;
  int *b = &a;
  //CHECK: int *b = &a;
  char *c = (char *)b;
  //CHECK: char *c = (char *)b;
  int *d = (int *)5;
  //CHECK: int *d = (int *)5;
  /*int *e = (int *)(a+5);*/
}

// malloc and generics are exceptions to our policy of unsafe void* casts
#include<stdlib.h>
_Itype_for_any(T)
void *ident(void *i : itype(_Ptr<T>))
//CHECK: void *ident(void *i : itype(_Ptr<T>))
: itype(_Ptr<T>) { return i;};
//CHECK: : itype(_Ptr<T>) { return i;};
void test_generic_casts(void) {
  int *ptr_cast = (int *)malloc(3 * sizeof(int));
  //CHECK_NOALL: int *ptr_cast = (int *)malloc<int>(3 * sizeof(int));
  //CHECK_ALL: _Ptr<int> ptr_cast = (_Ptr<int>)malloc<int>(3 * sizeof(int));
  int *ptr_nocast = malloc(3 * sizeof(int));
  //CHECK_NOALL: int *ptr_nocast = malloc<int>(3 * sizeof(int));
  //CHECK_ALL: _Ptr<int> ptr_nocast = malloc<int>(3 * sizeof(int));
  char *ptr_badcast = (int *)malloc(3 * sizeof(int));
  //CHECK: char *ptr_badcast = (int *)malloc<int>(3 * sizeof(int));
  int *ptr_custom = (int *)ident<int>(ptr_cast);
  //CHECK_NOALL: int *ptr_custom = (int *)ident<int>(ptr_cast);
  //CHECK_ALL: _Ptr<int> ptr_custom = (_Ptr<int>)ident<int>(ptr_cast);

  int *ptr_cast_c = (int *)calloc(3, sizeof(int));
  //CHECK_NOALL: int *ptr_cast_c = (int *)calloc<int>(3, sizeof(int));
  //CHECK_ALL: _Ptr<int> ptr_cast_c = (_Ptr<int>)calloc<int>(3, sizeof(int));
  int *ptr_cast_r = (int *)realloc(ptr_cast_c, 3 * sizeof(int));
  //CHECK: _Ptr<int> ptr_cast_r = (_Ptr<int>)realloc<int>(ptr_cast_c, 3 * sizeof(int));
}

// Check that casts to generics use the local type variables
int *mk_ptr(void);
//CHECK: int *mk_ptr(void);
_For_any(T) void free_ptr(_Ptr<T> p_ptr) {}
//CHECK: _For_any(T) void free_ptr(_Ptr<T> p_ptr) _Checked {}
void work (void) {
  int *node = mk_ptr(); // wild because extern unavailable
  free_ptr<int>(node);
  //CHECK: free_ptr<int>(_Assume_bounds_cast<_Ptr<int>>(node));
}

// cast from wild to checked needs to cast to the new destination type
_Itype_for_any(T) void has_itype(void *a : itype(_Array_ptr<T>) byte_count(l), unsigned int l);
void infers_generic(void *a, unsigned int l) {
  //CHECK_ALL: _Itype_for_any(T) void infers_generic(_Array_ptr<T> a : byte_count(l), unsigned int l) {
  has_itype(a, l);
}
void test_cast_to_inferred() {
  int *p = 1;
  int l;

  has_itype(p, l);
  //CHECK: has_itype<int>(p, l);
  infers_generic(p, l);
  //CHECK_ALL: infers_generic<int>(_Assume_bounds_cast<_Array_ptr<int>>(p,  bounds(unknown)), l);
}
