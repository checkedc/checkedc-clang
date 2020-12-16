// RUN: 3c -addcr %s | FileCheck -match-full-lines --check-prefixes="CHECK" %s
// RUN: 3c -addcr %s | %clang -c -fcheckedc-extension -x c -o %t1.unused -

void unsafe(void *);// Dummy to cause output
void f(int *x) {}
//CHECK: void f(_Ptr<int> x) _Checked {}

void foo(char c) { 
  //CHECK: void foo(char c) {
  unsafe(&c);
}

