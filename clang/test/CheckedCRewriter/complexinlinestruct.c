// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/complexinlinestruct.checked.c -- | count 0
// RUN: rm %S/complexinlinestruct.checked.c

/* a, b, c below all stay as WILD pointers; d can be a _Ptr<...>*/

 /* one decl; x rewrites to _Ptr<int> */
struct foo { int *x; } *a;
	//CHECK: struct foo { _Ptr<int> x; } *a;

struct baz { int *z; };
	//CHECK: struct baz { _Ptr<int> z; };
struct baz *d;
	//CHECK: _Ptr<struct baz> d = ((void *)0);

/* two decls, not one; y stays as int * */
struct bad { int* y; } *b, *c; 
	//CHECK: struct bad { _Ptr<int> y; } *b, *c; 

 /* two decls, y should be converted */
struct bar { int* y; } *e, *f;
	//CHECK: struct bar { _Ptr<int> y; } *e, *f;


void foo(void) {
  a->x = (void*)0;  
  b->y = (void*)0;
  d->z = (void*)0;
}

