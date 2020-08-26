// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/alloc_type_param.checked.c -- | count 0
// RUN: rm %S/alloc_type_param.checked.c

typedef unsigned long size_t;
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);

/* Check basic behavior with the three alloc functions */
void foo() {
  int *a = malloc(sizeof(int));
	//CHECK: _Ptr<int> a =  malloc<int>(sizeof(int));
  int *b = calloc(1, sizeof(int));
	//CHECK: _Ptr<int> b =  calloc<int>(1, sizeof(int));
  int *c = realloc(a, sizeof(int));
	//CHECK: _Ptr<int> c =  realloc<int>(a, sizeof(int));

  /* Explicit casts work fine also */

  int *d = (int*) malloc(sizeof(int));
	//CHECK: _Ptr<int> d =  (_Ptr<int>) malloc<int>(sizeof(int));
  int *e = (int*) calloc(1, sizeof(int));
	//CHECK: _Ptr<int> e =  (_Ptr<int>) calloc<int>(1, sizeof(int));
  int *f = (int*) realloc(d, sizeof(int));
	//CHECK: _Ptr<int> f =  (_Ptr<int>) realloc<int>(d, sizeof(int));
}

/* Allocating pointers to pointers */
void bar() {
  int **a = malloc(sizeof(int*));
	//CHECK: _Ptr<_Ptr<int>> a =  malloc<_Ptr<int>>(sizeof(int*));
  *a = malloc(sizeof(int));
	//CHECK: *a = malloc<int>(sizeof(int));

  /* It's also fine if the pointer is unchecked */
  int **b = malloc(sizeof(int*));
	//CHECK: _Ptr<int *> b =  malloc<int *>(sizeof(int*));
  *b = (int*) 1;
	//CHECK: *b = (int*) 1;
}

/* No conversion is done for void pointers, but this should just test that they
 convert and compile without crashing. We could insert void as the type
 parameter if there is anything to be gained from that. */
void baz() {
  void *v = malloc(sizeof(int));
	//CHECK: void *v = malloc(sizeof(int));
}

void buz() {
  struct {int a;} *b = malloc(10);
	//CHECK: struct {int a;} *b = malloc(10);

  /* Inline structs work just fine as long as they've been named.
   Note that c isn't made checked due to limitation in how inline structs are converted.
   If this test fails because it's made checked, that's great. */
  struct test {int a;} *c = malloc(sizeof(struct test));
	//CHECK: struct test {int a;} *c = malloc<struct test>(sizeof(struct test));

  /* typedefs are also OK. */
  typedef struct {int a;} other;
  other *d = malloc(sizeof(other));
	//CHECK: _Ptr<other> d =  malloc<other>(sizeof(other));
}

/* Don't mess with any existing type arguments. */
void fuz() {
  int *a = malloc<int>(sizeof(int));
	//CHECK: _Ptr<int> a =  malloc<int>(sizeof(int));
}
