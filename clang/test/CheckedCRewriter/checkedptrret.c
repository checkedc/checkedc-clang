// RUN: cconv-standalone -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -


int* foo() {
    int *c = 0;
    return c;
}

// CHECK:   _Ptr<int> foo(void) _Checked {
// CHECK: _Ptr<int> c = 0;

int *bar() {
// CHECK: _Ptr<int> bar(void) _Checked {
    int *c;
    // CHECK:  _Ptr<int> c = ((void *)0);
    int a = 1;
    if (1) {
    // CHECK: if (1) {
      c = foo();
    }
    return c;
}
