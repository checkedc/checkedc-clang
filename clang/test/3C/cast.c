// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/cast.checked.c -- | count 0
// RUN: rm %S/cast.checked.c

int foo(int* p) {
	//CHECK: int foo(_Ptr<int> p) {
  *p = 5;
  int x = (int)p; /* cast is safe */
  return x;
}

void bar(void) {
  int a = 0;
  int *b = &a;
	//CHECK: int *b = &a;
  char *c = (char *)b;
	//CHECK: char *c = (char *)b;
  int *d = (int *)5;
	//CHECK: int *d = (int *)5;
  /*int *e = (int *)(a+5);*/
}
