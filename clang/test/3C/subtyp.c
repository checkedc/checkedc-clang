// RUN: 3c -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -output-postfix=checked -alltypes %s
// RUN: 3c -alltypes %S/subtyp.checked.c -- | count 0
// RUN: rm %S/subtyp.checked.c

void take(int *p : itype(_Nt_array_ptr<int>));
	//CHECK: void take(int *p : itype(_Nt_array_ptr<int>));
int *foo(int *x) {
	//CHECK_NOALL: int *foo(int *x) {
	//CHECK_ALL: _Nt_array_ptr<int> foo(int *x : itype(_Nt_array_ptr<int>)) {
  take(x);
  return x;
}
void bar() {
	//CHECK_NOALL: void bar() {
	//CHECK_ALL: void bar() _Checked {
  int *x = 0;
	//CHECK_NOALL: int *x = 0;
	//CHECK_ALL:   _Nt_array_ptr<int> x =  0;
  foo(x);
}
void baz() {
  int *x = (int *)5;
	//CHECK: int *x = (int *)5;
  foo(x);
} 
void force(int *x){}
	//CHECK: void force(_Ptr<int> x)_Checked {}
