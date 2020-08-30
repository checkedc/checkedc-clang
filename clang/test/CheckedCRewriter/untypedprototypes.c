// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s

int *foo();
//CHECK: _Array_ptr<int> foo(_Array_ptr<int> q);

void bar(void) {
    int *x = 0;
    //CHECK: _Array_ptr<int> x = 0;
    int *y = foo(x);
    //CHECK: _Array_ptr<int> y = foo(x);
    y[10] = 1;
}

int *foo(int * q) {
  //CHECK: _Array_ptr<int> foo(_Array_ptr<int> q) {
    return q;
}
