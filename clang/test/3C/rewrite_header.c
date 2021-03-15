// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr -output-dir=%t.checked %S/rewrite_header.c %S/rewrite_header.h --
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK" %t.checked/rewrite_header.c --input-file %t.checked/rewrite_header.c
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK" %t.checked/rewrite_header.h --input-file %t.checked/rewrite_header.h
// RUN: %clang -c -fcheckedc-extension -x c -o /dev/null %t.checked/rewrite_header.c
// RUN: 3c -base-dir=%t.checked -alltypes -addcr -output-dir=%t.checked2 %t.checked/rewrite_header.c %t.checked/rewrite_header.h --
// RUN: test ! -f %t.checked2/rewrite_header.h -a ! -f %t.checked2/rewrite_header.c

#include "rewrite_header.h"
int *foo(int *x) {
  // CHECK: _Ptr<int> foo(_Ptr<int> x) _Checked {
  return x;
}
