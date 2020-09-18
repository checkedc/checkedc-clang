// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

/* a, b, c below all stay as WILD pointers; d can be a _Ptr<...>*/

 /* one decl; x rewrites to _Ptr<int> */
struct foo { int *x; } *a;
//CHECK: struct foo { _Ptr<int> x; } *a;

struct baz { int *z; };
struct baz *d;
//CHECK: struct baz { _Ptr<int> z; };
//CHECK: _Ptr<struct baz> d = ((void *)0);

/* two decls, not one; y stays as int * */
struct bad { int* y; } *b, *c; 
//CHECK: struct bad { int* y; } *b, *c;

 /* two decls, y should be converted */
struct bar { int* y; } *e, *f;
//CHECK: struct bar { _Ptr<int> y; } *e, *f;


void foo(void) {
  a->x = (void*)0;  
  b->y = (void*)0;
  d->z = (void*)0;
  c->y = (int *)5; // forces it to WILD
}

