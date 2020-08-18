// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/gvar.checked.c -- | count 0
// RUN: rm %S/gvar.checked.c

int *x;
	//CHECK: _Ptr<int> x = ((void *)0);
extern int *x;
void foo(void) {
	//CHECK: void foo(void) _Checked {
  *x = 1;
}

extern int *y;
int *y;
	//CHECK: int *y;
int *bar(void) {
	//CHECK: _Ptr<int> bar(void) {
  y = (int*)5;
	//CHECK: y = (int*)5;
  return x;
}

