// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/macro_end_of_decl.c -- | diff %t.checked/macro_end_of_decl.c -

// Tests for fix of issue 429. Macros occuring at the end of a rewritten source
// range caused an assertion failure.

// Original example of the bug found in zlib
#define OF(args) args
typedef void(*alloc_func) OF((int, int, int));
alloc_func zalloc;
//CHECK: typedef _Ptr<void (int, int, int)> alloc_func;
//CHECK: alloc_func zalloc = ((void *)0);

// Some more contrived examples demonstrating the same issue.
#define ARGS ()
typedef int(*a) ARGS;
a b;
int(*c) ARGS;
//CHECK: typedef _Ptr<int (void)> a;
//CHECK: a b = ((void *)0);
//CHECK: _Ptr<int (void)> c = ((void *)0);

#define SIZE [1]
int d SIZE;
int e SIZE[1];
//CHECK_NOALL: int d SIZE;
//CHECK_NOALL: int e SIZE[1];
//CHECK_ALL: int d _Checked SIZE;
//CHECK_ALL: int e _Checked SIZE _Checked[1];

#define EQ =
int *f EQ 0;
//CHECK: _Ptr<int> f EQ 0;

int(*g0) ARGS, g1 SIZE, *g2 EQ 0;
//CHECK: _Ptr<int (void)> g0 = ((void *)0);
//CHECK_NOALL: int g1[1];
//CHECK_ALL: int g1 _Checked SIZE;
//CHECK: _Ptr<int> g2 EQ 0;

#define RPAREN )
int * h ( int *a RPAREN {
  //CHECK: _Ptr<int> h(_Ptr<int> a) _Checked {
  return 0;
}

// This test case is a little different. Instead of failing an assert, 3C
// silently skipped rewriting the cast. This caused a compiler error from
// CheckedC's clang.

#define STAR *
void foo(int *a) {
  int *b = (int STAR)a;
  int *c = (int STAR){a};
}
//CHECK: void foo(_Ptr<int> a) {
//CHECK:   _Ptr<int> b = (_Ptr<int>)a;
//CHECK:   _Ptr<int> c = (_Ptr<int>){a};
//CHECK: }
