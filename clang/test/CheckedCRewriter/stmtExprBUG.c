// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null - 
// XFAIL: *

typedef unsigned long size_t;
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

/*right now, even though our solving correctly identifies q ought to be checked 
  the rewriter fails to rewrite q to be checked so it appears WILD*/
void foo() {
  int *p = ({int *q = malloc(3*sizeof(int)); q[2] = 1; q;});
  //CHECK_ALL: _Array_ptr<int> p : count(3) =  ({_Array_ptr<int> q : count(3) = malloc<int>(3*sizeof(int)); q[2] = 1; q;});
  //CHECK_NOALL: int *p = ({int *q = malloc(3*sizeof(int)); q[2] = 1; q;});
  p[1] = 3;
} 

void bar() { 
  int *p = ({int *q = malloc(sizeof(int)); *q = 7; q;});
  //CHECK: _Ptr<int> p =  ({_Ptr<int> q = malloc(sizeof(int)); *q = 7; q;});
  *p = 4;
}