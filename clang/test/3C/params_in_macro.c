// Test that if FunctionDeclBuilder::buildDeclVar cannot get the original source
// for a parameter declaration due to a macro, it reconstructs the declaration
// (including the name) instead of leaving a blank.

// RUN: 3c -base-dir=%S %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

// 3C is not idempotent on this file because the first pass constrains the
// pointers in the macros to wild but then inlines the macros, so the second
// pass will make those pointers checked. So don't try to test idempotence.

typedef double mydouble;

#define parms1 volatile mydouble d, void (*f)(void)
#define parms2 int *const y : count(7), _Ptr<char> z, int *zz : itype(_Ptr<int>)

void test(parms1, int *x, parms2) {}
// CHECK: void test(volatile mydouble d, void (*f)(void), _Ptr<int> x, int *const y : count(7), _Ptr<char> z, int *zz : itype(_Ptr<int>)) {}

// Before the bug fix, we got:
// void test(, , _Ptr<int> x, , , ) {}
