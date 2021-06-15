// Verification of checked codes using Seahorn
//
// REQUIRES: seahorn
//
// RUN: %clang -finject-verifier-calls -funchecked-pointers-dynamic-check -c -emit-llvm -O0 -m64 %s -o %t1
// RUN: %sea_pp %t1 -o %t2
// RUN: %sea_ms %t2 -o %t3
// RUN: %sea_opt %t3 -o %t4
// RUN: %sea_horn --solve %t4 | FileCheck %s

extern void __VERIFIER_error (void);
extern void __VERIFIER_assume (int);
#define assume __VERIFIER_assume

int foo(_Array_ptr<int> p : count(n), int n)
{
  assume(n > 0);
  assume(p != 0);
  _Array_ptr<int> a : count(n) = p;

  a[n / 2] = 1;

  int i = 0, s = 0;
  for(i=0; i<2*n; i++)
    s += a[i - 1];       // There exists a value of i that goes out of bounds

  return s;
}

// CHECK: sat

