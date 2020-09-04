// RUN: cconv-standalone -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

#include <stddef.h>

extern void *memset(void * dest : byte_count(n),
             int c,
             size_t n) : bounds(dest, (_Array_ptr<char>)dest + n);

// don't mess with this
_Itype_for_any(T) void vsf_sysutil_memclr(void* p_dest : itype(_Array_ptr<T>) byte_count(size), unsigned int size) {
// CHECK: _Itype_for_any(T) void vsf_sysutil_memclr(void* p_dest : itype(_Array_ptr<T>) byte_count(size), unsigned int size) {
  memset(p_dest, '\0', size);
}


int *foo( _Ptr<int> q) {
// CHECK: _Ptr<int> foo(_Ptr<int> q) _Checked {
  return q;
}
void bar(void) {
// CHECK: void bar(void) _Checked {
  int *x = 0;
  // CHECK: _Ptr<int> x = 0;
  int *y = foo(x);
  // CHECK: _Ptr<int> y = foo(x);
}
