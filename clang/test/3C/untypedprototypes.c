// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -alltypes %s -- | %clang -c -f3c-tool -fcheckedc-extension -x c -o %t.unused -

int *foo();
//CHECK: _Array_ptr<int> foo(_Array_ptr<int> q) : count(10 + 1);

void bar(void) {
  int *x = 0;
  //CHECK: _Array_ptr<int> x = 0;
  int *y = foo(x);
  //CHECK: _Array_ptr<int> y : count(10 + 1) = foo(x);
  y[10] = 1;
}

int *foo(int *q) {
  //CHECK: _Array_ptr<int> foo(_Array_ptr<int> q) : count(10 + 1) {
  return q;
}
