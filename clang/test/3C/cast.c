// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/cast.c -- | diff %t.checked/cast.c -

int foo(int *p) {
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
