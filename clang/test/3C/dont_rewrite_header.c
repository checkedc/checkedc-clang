// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr -output-dir=%t.checked %S/dont_rewrite_header.c %S/dont_rewrite_header.h --
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK" %t.checked/dont_rewrite_header.c --input-file %t.checked/dont_rewrite_header.c
// RUN: test ! -f %t.checked/dont_rewrite_header.h
// RUN: cp %S/dont_rewrite_header.h %t.checked/dont_rewrite_header.h
// RUN: %clang -c -fcheckedc-extension -x c -o /dev/null %t.checked/dont_rewrite_header.c
// RUN: 3c -base-dir=%t.checked -alltypes -addcr -output-dir=%t.checked2 %t.checked/dont_rewrite_header.c %t.checked/dont_rewrite_header.h --
// RUN: test ! -f %t.checked2/dont_rewrite_header.h -a ! -f %t.checked2/dont_rewrite_header.c

#include "dont_rewrite_header.h"
int *foo(int *x) {
  // CHECK: _Ptr<int> foo(_Ptr<int> x) _Checked {
  return x;
}

int bar(int *x : itype(_Ptr<int>)) {
  x = (int *)1;
  return 0;
}
