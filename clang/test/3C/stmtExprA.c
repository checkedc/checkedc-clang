// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL" %s
// RUN: 3c -base-dir=%S %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL" %s
// RUN: 3c -base-dir=%S %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

#include <stdlib.h>

void foo2(int *x) {
  int *p;
  //CHECK_ALL: _Array_ptr<int> p : count(3) = ((void *)0);
  //CHECK_NOALL: int *p;
  p = ({
    int *q = malloc(3 * sizeof(int));
    q[2] = 1;
    q;
  });
  //CHECK_ALL:      p = ({
  //CHECK_ALL-NEXT:   _Array_ptr<int> q : count(3) = malloc<int>(3 * sizeof(int));
  //CHECK_ALL-NEXT:   q[2] = 1;
  //CHECK_ALL-NEXT:   q;
  //CHECK_ALL-NEXT: });
  //CHECK_NOALL:      p = ({
  //CHECK_NOALL-NEXT:   int *q = malloc<int>(3 * sizeof(int));
  //CHECK_NOALL-NEXT:   q[2] = 1;
  //CHECK_NOALL-NEXT:   q;
  //CHECK_NOALL-NEXT: });
  p[1] = 3;
}
