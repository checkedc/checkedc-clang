// Test for CodeGen when injecting verifier bad states in unchecked codes
// with bounds-safe interface. There should be a __VERIFIER_error at each
// dynamic check failed block.
//
//
// RUN: %clang -finject-verifier-calls -funchecked-pointers-dynamic-check -emit-llvm -S %s -o - | FileCheck %s

extern void __VERIFIER_error (void);
extern void __VERIFIER_assume (int);
#define assume __VERIFIER_assume

int foo(int *a : count(n), int n);
int foo(int *a, int n)
{
  assume(n > 0);
  assume(a != 0);

  a[n / 2] = 1;
  // CHECK: _Dynamic_check.failed{{.*}}:
  // CHECK-NEXT: call void @__VERIFIER_error()
  // CHECK-NEXT: call void @llvm.trap()
  // CHECK-NEXT: unreachable

  int i = 0, s = 0;
  for(i=0; i<n; i++)
    s += a[i];
  // CHECK: _Dynamic_check.failed{{.*}}:
  // CHECK-NEXT: call void @__VERIFIER_error()
  // CHECK-NEXT: call void @llvm.trap()
  // CHECK-NEXT: unreachable

  return s;
}


