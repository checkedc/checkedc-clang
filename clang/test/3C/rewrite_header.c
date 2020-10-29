// RUN: 3c -alltypes -addcr --output-postfix=checked %S/rewrite_header.c %S/rewrite_header.h
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK" %S/rewrite_header.checked.c < %S/rewrite_header.checked.c
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK" %S/rewrite_header.checked.h < %S/rewrite_header.checked.h
// RUN: sed -i -e "s!rewrite_header\.h!%S/rewrite_header\.checked\.h!" %S/rewrite_header.checked.c
// RUN: %clang -c -fcheckedc-extension -x c -o /dev/null %S/rewrite_header.checked.c
// RUN: 3c -alltypes -addcr --output-postfix=checked2 %S/rewrite_header.checked.c %S/rewrite_header.checked.h
// RUN: test ! -f %S/rewrite_header.checked2.h -a ! -f %S/rewrite_header.checked2.c
// RUN: rm %S/rewrite_header.checked.c %S/rewrite_header.checked.h

#include "rewrite_header.h"
int *foo(int *x) {
// CHECK: _Ptr<int> foo(_Ptr<int> x) _Checked {
  return x;
}
