// RUN: 3c -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s
// RUN: 3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// XFAIL: *

#include <stdarg.h>
#include <stddef.h>

void sum(int *ptr, int count, ...) { 
  //CHECK: void sum(_Ptr<int> ptr, int count, ...) {
  va_list ap;
  int sum = 0;

  va_start(ap, count);

  for(int i = 0; i < count; i++) 
    sum += va_arg(ap, int);

  *ptr = sum;
  va_end(ap);
  return;
}

void bad_sum(int **ptr, int count, ...) { 
  //CHECK: void bad_sum(_Ptr<int*> ptr, int count, ...) {
  va_list ap;
  int sum = 0;

  *ptr = (int*) 3;

  va_start(ap, count);

  for(int i = 0; i < count; i++) 
    sum += va_arg(ap, int);

  **ptr = sum;
  va_end(ap);
  return;
}

int foo(int *ptr) {
  //CHECK: int foo(_Ptr<int> ptr) _Checked {
  *ptr += 2;
  sum(ptr,1,2,3);
  //CHECK: _Unchecked { sum(ptr,1,2,3); };
  return *ptr;
}

int bar(int *ptr) {
  //CHECK: int bar(int *ptr) { 
  *ptr += 2;
  bad_sum(&ptr,1,2,3);
  //CHECK: bad_sum(((int **)&ptr),1,2,3);
  return *ptr;
}

void baz(void) { 
  //CHECK: void baz(void) {
  sum(NULL, 3, 1, 2, 3);
  //CHECK: sum(NULL, 3, 1, 2, 3);
}
