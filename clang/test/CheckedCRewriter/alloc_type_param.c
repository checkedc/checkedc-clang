// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -output-postfix=checkedNOALL %s
// RUN: %clang -c %S/alloc_type_param.checkedNOALL.c
// RUN: rm %S/alloc_type_param.checkedNOALL.c

typedef unsigned long size_t;
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);

// Check basic behavior with the three alloc functions
void foo() {
  int *a = malloc(sizeof(int));
  // CHECK: _Ptr<int> a = malloc<int>(sizeof(int));
  int *b = calloc(1, sizeof(int));
  // CHECK: _Ptr<int> b = calloc<int>(1, sizeof(int));
  int *c = realloc(a, sizeof(int));
  // CHECK: _Ptr<int> c = realloc<int>(a, sizeof(int));

  // Explicit casts work fine also

  int *d = (int*) malloc(sizeof(int));
  // CHECK: _Ptr<int> d = (int*) malloc<int>(sizeof(int));
  int *e = (int*) calloc(1, sizeof(int));
  // CHECK: _Ptr<int> e = (int*) calloc<int>(1, sizeof(int));
  int *f = (int*) realloc(d, sizeof(int));
  // CHECK: _Ptr<int> f = (int*) realloc<int>(d, sizeof(int));
}

// Allocating pointers to pointers
void bar() {
  // The type parameter doesn't need to be _Ptr<int> even though it is checked
  int **a = malloc(sizeof(int*));
  // CHECK: _Ptr<_Ptr<int>> a = malloc<int *>(sizeof(int*));
  *a = malloc(sizeof(int));
  // CHECK: *a = malloc<int>(sizeof(int));

  // It's also fine if the pointer is unchecked
  int **b = malloc(sizeof(int*));
  // CHECK: _Ptr<int*> b = malloc<int *>(sizeof(int*));
  *b = (int*) 1;
}

// No conversion is done for void pointers, but this should just test that they
// convert and compile without crashing. We could insert void as the type
// parameter if there is anything to be gained from that.
void baz() {
  void *v = malloc(sizeof(int));
  // CHECK: void *v = malloc(sizeof(int));
}

// Don't mess with any existing type arguments.
void fuz() {
  int *a = malloc<int>(sizeof(int));
  // CHECK: _Ptr<int> a  = malloc<int>(sizeof(int));
}
