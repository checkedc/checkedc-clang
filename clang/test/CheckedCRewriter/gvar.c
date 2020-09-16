// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK","CHECK_NEXT" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

int *x;
extern int *x;
void foo(void) {
  *x = 1;
}
//CHECK: _Ptr<int> x = ((void *)0);
//CHECK-NEXT: extern _Ptr<int> x;

extern int *y;
int *y;
int *bar(void) {
  y = (int*)5;
  return x;
}
//CHECK: _Ptr<int> bar(void) {

