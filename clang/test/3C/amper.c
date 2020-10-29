// RUN: 3c -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -output-postfix=checked -alltypes %s
// RUN: 3c -alltypes %S/amper.checked.c -- | count 0
// RUN: rm %S/amper.checked.c

void foo(int *x) {
	//CHECK: void foo(int *x) {
  x = (int *)5;
	//CHECK: x = (int *)5;
  int **y = &x;
	//CHECK: _Ptr<int *> y =  &x;
}

void bar(int *x) {
	//CHECK: void bar(int *x) {
  x = (int *)5;
	//CHECK: x = (int *)5;
  int *y = *(&x);
	//CHECK: int *y = *(&x);
}

int *id(int *x) {
	//CHECK: _Ptr<int> id(_Ptr<int> x) _Checked {
  return &(*x);
}

int f(int *x) {
	//CHECK: int f(_Ptr<int> x) _Checked {
  return *x;
}

void baz(void) {
  int (*fp)(int *) = f;
	//CHECK: _Ptr<int (_Ptr<int> )> fp =  f;
  int (*fp2)(int *) = &f;
	//CHECK: _Ptr<int (_Ptr<int> )> fp2 =  &f;
  f((void*)0);
}

extern int xfunc(int *arg);
int (*fp)(int *);
	//CHECK: _Ptr<int (int *)> fp = ((void *)0);

void addrof(void){
  fp = &xfunc;
}

void bif(int **x) {
	//CHECK: void bif(_Ptr<_Ptr<int>> x) _Checked {
  int **w = 0;
	//CHECK: _Ptr<_Ptr<int>> w =  0;
  int *y = *(x = w);
	//CHECK: _Ptr<int> y =  *(x = w);
  w = &y;
}

