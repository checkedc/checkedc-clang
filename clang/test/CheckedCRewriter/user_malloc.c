// RUN: cconv-standalone -alltypes -use-malloc=my_malloc,your_malloc %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -use-malloc=my_malloc,your_malloc %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -use-malloc=my_malloc,your_malloc %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

#include <stddef.h>
extern _Itype_for_any(T) void *my_malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *your_malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

int * foo(void) {
  int *p = my_malloc(sizeof(int));
  //CHECK: _Ptr<int> p =  my_malloc<int>(sizeof(int));
  *p = 1;
  int *q = my_malloc(sizeof(int)*5);
  //CHECK_ALL: _Array_ptr<int> q : count(5) =  my_malloc<int>(sizeof(int)*5); 
  //CHECK_NOALL: int *q = my_malloc<int>(sizeof(int)*5);
  int *w = your_malloc(sizeof(int)*3); 
  //CHECK_ALL: _Array_ptr<int> w : count(3) =  your_malloc<int>(sizeof(int)*3); 
  //CHECK_NOALL: int *w = your_malloc<int>(sizeof(int)*3);  
  int *x = malloc(sizeof(int)*2); 
  //CHECK_ALL: _Array_ptr<int> x : count(2) =  malloc<int>(sizeof(int)*2);
  //CHECK_NOALL: int *x = malloc<int>(sizeof(int)*2); 
  w[1] = 0; 
  x[1] = 9;
  q[2] = 3;
  return p;
}