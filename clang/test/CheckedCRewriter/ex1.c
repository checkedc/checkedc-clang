// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/ex1.checked.c -- | count 0
// RUN: rm %S/ex1.checked.c

void foo() {
  int m = 2;
  int *s = &m;
	//CHECK: _Ptr<int> s =  &m;
  int q[5] = { 0 };
  int *p = (int *)5;
	//CHECK: int *p = (int *)5;
  p = q + 3;
}
