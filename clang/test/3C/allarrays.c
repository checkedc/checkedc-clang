// RUN: 3c -alltypes %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s

int* foo();
//CHECK: _Array_ptr<int> foo(_Array_ptr<int> r);
void bar(void) {
    int *p = 0;
    //CHECK: _Array_ptr<int> p = 0;
    int *q = foo(p);
    //CHECK: _Array_ptr<int> q = foo(p);
    q[1] = 0;
}
int* foo(int *r);
//CHECK: _Array_ptr<int> foo(_Array_ptr<int> r);
void baz(int *t) {
//CHECK: void baz(_Array_ptr<int> t) {
    foo(t);
}
int* foo(int *r) {
//CHECK: _Array_ptr<int> foo(_Array_ptr<int> r) {
    return r;
}
