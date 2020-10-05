// RUN: cconv-standalone -extra-arg="-Wno-everything" -alltypes -warn-root-cause %s 2>&1 1>/dev/null | FileCheck %s

#include <stddef.h>
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

void *x;
// CHECK-DAG: Default void* type

void test0() {
  int *a;
  char *b;
  a = b;
  // CHECK-DAG: Cast from int * to char *

  int *c;
  (char*) c;
  // CHECK-DAG: Cast from int * to char *

  
  int *e;
  char *f;
  f = (char*) e;
  // CHECK-DAG: Cast from char * to int *
}

void test1() {
  int a;
  int *b;
  b = malloc(sizeof(int));
  b[0] = 1;
}

// CHECK-DAG: 4 warnings generated.
