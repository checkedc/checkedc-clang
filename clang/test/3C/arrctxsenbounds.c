// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -alltypes %s -- | %clang -c -f3c-tool -fcheckedc-extension -x c -o %t1.unused -

/*
Context-sensitive array-bounds inference.
*/
#include <stdlib.h>
struct foo {
  int *x;
  unsigned olol;
  int *y;
  unsigned fail_y_len;
};
//CHECK:    _Array_ptr<int> x : count(olol);
//CHECK:    _Array_ptr<int> y : count(0 + 1);

void ctx_(struct foo *f, struct foo *f2) {
  f2->y = f->x;
  f2->y[0] = 1;
}
//CHECK: void ctx_(_Ptr<struct foo> f, _Ptr<struct foo> f2) {

int main(int argc, char **argv) {
  char *PN = argv[0];
  unsigned n = 10;
  struct foo po, po2;
  po.x = malloc(n * sizeof(int));
  po.x[0] = 0;
  po.olol = n;
  po2.fail_y_len = n;
  ctx_(&po, &po2);
  return 0;
}
//CHECK: int main(int argc, _Array_ptr<_Nt_array_ptr<char>> argv : count(argc)) {
//CHECK: _Ptr<char> PN = argv[0];
//CHECK:    struct foo po = {};
//CHECK: struct foo po2 = {};
