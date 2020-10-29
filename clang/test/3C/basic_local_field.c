// Tests for Checked C rewriter tool.
//
// Tests properties about type re-writing and replacement of structure members
//
// RUN: 3c %s -- | FileCheck -match-full-lines %s
// RUN: 3c %s -- | %clang_cc1  -verify -fcheckedc-extension -x c -
// expected-no-diagnostics
//

typedef struct {
  int a;
  int *b;
} foo;
//CHECK: typedef struct {
//CHECK-NEXT: int a;
//CHECK-NEXT: _Ptr<int> b;

// here we use b in a safe way, hence
// it will be a _Ptr
typedef struct {
  float *b;
} foo2;
//CHECK: typedef struct {
//CHECK-NEXT: _Ptr<float> b;

// here we use p in unsafe way
// and hence will not be a safe ptr
typedef struct {
  float c;
  int *p;
  char d;
} foo3;
int main() {
  foo obj = {};
  float b;
  foo2 obj2 = {};
  obj2.b = &b;
  foo3 obj3 = {};
  obj3.p = (int*)0xcafebabe;
}