// RUN: cconv-standalone -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -alltypes -output-postfix=checked %s
// RUN: cconv-standalone -alltypes %S/multivardecls.checked.c -- | diff %S/multivardecls.checked.c -
// RUN: rm %S/multivardecls.checked.c

typedef unsigned long size_t;
_Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

void test() {
  int *a = (int*) 0,*b = (int*) 0;
//CHECK: _Ptr<int> a = (_Ptr<int>) 0;
//CHECK: _Ptr<int> b = (_Ptr<int>) 0;

  int *c = (int*) 1 ,*d = (int*){0};
//CHECK: int *c = (int*) 1 ;
//CHECK: _Ptr<int> d = (_Ptr<int>){0};

  int *e, *f = malloc(sizeof(int));
//CHECK: _Ptr<int> e = ((void *)0);
//CHECK: _Ptr<int> f = malloc<int>(sizeof(int));

  int g[1] = {*&(int){1}}, *h;
//CHECK_ALL: int g _Checked[1] = {*&(int){1}};
//CHECK_NOALL: int g[1] = {*&(int){1}};
//CHECK: _Ptr<int> h = ((void *)0);

  float *i = 1, j = 0, *k = (float*){0}, l;
//CHECK: float *i = 1;
//CHECK: float j = 0;
//CHECK: _Ptr<float> k = (_Ptr<float>){0};
//CHECK: float l;

  int m = 1, *n = &m;
//CHECK: int m = 1, *n = &m;
  n++;

  int o, p[1], q[] = {1, 2}, r[1][1], *s;
//CHECK: int o;
//CHECK_ALL: int p _Checked[1];
//CHECK_NOALL: int p[1];
//CHECK_ALL: int q _Checked[2] = {1, 2};
//CHECK_NOALL: int q[] = {1, 2};
//CHECK_ALL: int r _Checked[1] _Checked[1];
//CHECK_NOALL: int r[1][1];
//CHECK: _Ptr<int> s = ((void *)0);

  int *t[1], *u = malloc(2*sizeof(int)), *v;
//CHECK_ALL: _Ptr<int> t _Checked[1];
//CHECK_NOALL: int *t[1];
//CHECK_ALL: _Ptr<int> u = malloc<int>(2*sizeof(int));
//CHECK_NOALL: int *u = malloc<int>(2*sizeof(int));
//CHECK: _Ptr<int> v = ((void *)0);

  int *w = (int*) 0, *x = (int*) 0, *z = (int *) 1;
//CHECK: _Ptr<int> w = (_Ptr<int>) 0;
//CHECK: int *x = (int*) 0;
//CHECK: int *z = (int *) 1;
  x = (int*) 1;
//CHECK: x = (int*) 1;
}

void test2() {
  int a, b[1][1], *c;
// CHECK: int a;
// CHECK_ALL: int b _Checked[1] _Checked[1];
// CHECK_NOALL: int b[1][1];
// CHECK: _Ptr<int> c = ((void *)0);

  int e[1][1], *f, *g[1] = {(int*) 0};
// CHECK_ALL: int e _Checked[1] _Checked[1];
// CHECK_NOALL: int e[1][1];
// CHECK: _Ptr<int> f = ((void *)0);
// CHECK_ALL: _Ptr<int> g _Checked[1] = {(_Ptr<int>) 0};
// CHECK_NOALL: int *g[1] = {(int*) 0};

  int h, (*i)(int), *j, (*k)(int);
// CHECK: int h;
// CHECK: _Ptr<int (int )> i = ((void *)0);
// CHECK: _Ptr<int> j = ((void *)0);
// CHECK: int (*k)(int);

  k = 1;
}

void test3() {
  int *a,b;
// CHECK: _Ptr<int> a = ((void *)0);
// CHECK: int b;
 
  int *c, d;
// CHECK: _Ptr<int> c = ((void *)0);
// CHECK: int d;
 
  int *e , f;
// CHECK:  _Ptr<int> e = ((void *)0) ;
// CHECK: int f;
 

  int *h ,g;
// CHECK:_Ptr<int> h = ((void *)0) ;
// CHECK:int g;
}

void test4() {
  struct foo {
    int *a, b;
// CHECK:_Ptr<int> a;
// CHECK:int b;

    int c, *d;
// CHECK: int c;
// CHECK: _Ptr<int> d;
 
    int *f, **g;
// CHECK: _Ptr<int> f;
// CHECK: _Ptr<_Ptr<int>> g;
  };
}
