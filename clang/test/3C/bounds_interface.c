// Tests for the Checked C rewriter tool.
//
// RUN: 3c -addcr %s -- -fcheckedc-extension | FileCheck -match-full-lines %s
// RUN: 3c -addcr %s -- -fcheckedc-extension | %clang_cc1  -verify -fcheckedc-extension -x c -
// RUN: 3c -addcr -output-postfix=checked %s 
// RUN: 3c -addcr %S/bounds_interface.checked.c -- | count 0
// RUN: rm %S/bounds_interface.checked.c
// expected-no-diagnostics
//


extern void bar(int *q : itype(_Ptr<int>));
//CHECK: extern void bar(int *q : itype(_Ptr<int>));

extern void bar2(int *q : itype(_Ptr<int>), int *z : itype(_Ptr<int>));
//CHECK: extern void bar2(int *q : itype(_Ptr<int>), int *z : itype(_Ptr<int>));

extern int *baz(void) : itype(_Ptr<int>);
//CHECK: extern int *baz(void) : itype(_Ptr<int>);

void foo(int *p : itype(_Ptr<int>)) {
	*p = 0;
	return;
}
//CHECK: void foo(int *p : itype(_Ptr<int>)) _Checked {

int foo2(int *j) {
	int *a = baz();
	return *a + *j; 
}
//CHECK: int foo2(_Ptr<int> j) _Checked {
//CHECK-NEXT: _Ptr<int> a = baz();

void bif(void) {
    int *x = (int *)5;
    foo(x);
}
//CHECK:     int *x = (int *)5;

