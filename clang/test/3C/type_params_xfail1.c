// RUN: 3c -addcr -alltypes %s | FileCheck -match-full-lines %s

// XFAIL: *

// This fails because the type variable is used to constrain both calls to `incoming_doubleptr`.
// To be correct, the usage of a type variable should be independent at each call site.

// adapted from type_params.c
#include <stddef.h>
_Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

_Itype_for_any(T)
void incoming_doubleptr(_Array_ptr<T> ptr : itype(_Array_ptr<T>)) {
  return;
}

void do_doubleptr(int count) {
  int **arr = malloc(sizeof(int*) * count);
  // CHECK: _Array_ptr<_Ptr<int>> arr : count(count) = malloc<_Ptr<int>>(sizeof(int*) * count);
  incoming_doubleptr(arr);
}

// adding this function changes the infered type of the previous one unnecessarily
void interfere_doubleptr(void)
{
  float fl _Checked[5][5] = {};
  incoming_doubleptr(fl);
}
