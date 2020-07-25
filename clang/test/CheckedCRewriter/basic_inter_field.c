// Tests for Checked C rewriter tool.
//
// Tests properties about constraint propagation of structure fields
// across functions
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s
// RUN: cconv-standalone %s -- | %clang_cc1  -verify -fcheckedc-extension -x c -
// expected-no-diagnostics
//

typedef struct {
  int *ptr;
  char *ptr2;
} foo;

//CHECK: _Ptr<int> ptr;
//CHECK-NEXT: _Ptr<char> ptr2;

foo obj1 = {};

int* func(int *ptr, char *iwild) {
  // both the arguments are pointers
  // within function body
  obj1.ptr = ptr;
  obj1.ptr2 = iwild;
  return ptr;
}

//CHECK: _Ptr<int> func(_Ptr<int> ptr, char *iwild : itype(_Ptr<char>)) {

int main() {
  int a;
  int *b = 0;
  char *wil;
  wil = (char*)0xdeadbeef;
  b = func(&a, wil);
  return 0;
}

//CHECK: int main() {
//CHECK-NEXT: int a;
//CHECK-NEXT: _Ptr<int> b =  0;


