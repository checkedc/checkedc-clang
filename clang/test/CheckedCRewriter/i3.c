// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/i3.checked.c -- | count 0
// RUN: rm %S/i3.checked.c

static int * f(int *x) {
	//CHECK: static int * f(int *x) {
  x = (int *)5;
	//CHECK: x = (int *)5;
  return x;
}
/* force output */
int *p;
	//CHECK: _Ptr<int> p = ((void *)0);
