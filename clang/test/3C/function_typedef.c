// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/function_typedef.c -- | diff %t.checked/function_typedef.c -

// Tests for the single file case in issue #430
// Functions declared using a typedef should be rewritten in a way that doesn't
// crash 3C or generate uncompilable code.

typedef void foo(int *);
foo foo_impl;
void foo_imp(int *a) {}
//CHECK: foo foo_impl;
//CHECK: void foo_imp(_Ptr<int> a) _Checked {}

typedef int *bar();
bar bar_impl;
int *bar_impl() { return 0; }
//CHECK: _Ptr<int> bar_impl(void);
//CHECK: _Ptr<int> bar_impl(void) _Checked { return 0; }

typedef int *baz(int *);
baz baz_impl;
int *baz_impl(int *a) { return 0; }
//CHECK: _Ptr<int> baz_impl(_Ptr<int> a);
//CHECK: _Ptr<int> baz_impl(_Ptr<int> a) _Checked { return 0; }
