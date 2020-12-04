// RUN: 3c -alltypes -addcr --output-postfix=checked %S/dont_rewrite_header.c %S/dont_rewrite_header.h
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK" %S/dont_rewrite_header.checked.c < %S/dont_rewrite_header.checked.c
// RUN: test ! -f %S/dont_rewrite_header.checked.h
// RUN: %clang -c -fcheckedc-extension -x c -o /dev/null %S/dont_rewrite_header.checked.c
// RUN: 3c -alltypes -addcr --output-postfix=checked2 %S/dont_rewrite_header.checked.c %S/dont_rewrite_header.h
// RUN: test ! -f %S/dont_rewrite_header.checked.checked2.h -a ! -f %S/dont_rewrite_header.checked.checked2.c
// RUN: rm %S/dont_rewrite_header.checked.c

#include "dont_rewrite_header.h"
int *foo(int *x) {
// CHECK: _Ptr<int> foo(_Ptr<int> x) _Checked {
  return x;
}

int bar(int *x) {
  x = (int*) 1;
  return 0;
}
