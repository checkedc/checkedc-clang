// RUN: cconv-standalone -alltypes -use-malloc=my_malloc %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -use-malloc=my_malloc %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -use-malloc=my_malloc %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

typedef unsigned long size_t;
extern _Itype_for_any(T) void *my_malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

int * foo(void) {
  int *p = my_malloc(sizeof(int));
  //CHECK: _Ptr<int> p =  my_malloc<int>(sizeof(int));
  *p = 1;
  int *q = my_malloc(sizeof(int)*5);
  //CHECK_ALL: _Array_ptr<int> q : count(5) =  my_malloc<int>(sizeof(int)*5); 
  //CHECK_NOALL: int *q = my_malloc<int>(sizeof(int)*5);
  q[2] = 3;
  return p;
}