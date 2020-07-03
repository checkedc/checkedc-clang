// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -output-postfix=checkedNOALL %s
// RUN: %clang -c %S/unsafeunion.checkedNOALL.c
// RUN: rm %S/unsafeunion.checkedNOALL.c

union foo {
  /*fields of a union should never be converted*/
  int * p;
  char * c;
};
//CHECK: int * p;
//CHECK: char * c;

void bar(int *x) {
  /*but pointers to unions can be*/
  union foo *g = (void *) 0;
  union foo *h = calloc(5, sizeof(union foo)); 
  int y = 3;
  h[2].p = &y;

  char c;
  union foo f = { .c = &c };
  *f.p = 1;
} 
//CHECK: _Ptr<union foo> g = (void *) 0;
//CHECK_NOALL: union foo *h = calloc(5, sizeof(union foo)); 
//CHECK_ALL: _Array_ptr<union foo> h : count(5) =  calloc(5, sizeof(union foo));
