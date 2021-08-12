// RUN: 3c -use-malloc=my_malloc -base-dir=%S -alltypes -warn-all-root-cause %s -- -Xclang -verify -Wno-everything

// This test is unusual in that it checks for the errors in the code

// We define a prototype for malloc here instead of including <stdlib.h> 
// as including stdlib will create numerous root-cause warnings we don't want to deal with

// unwritable-expected-warning@+2 {{0 unchecked pointers: Source code in non-writable file}}
// expected-warning@+1 {{Unchecked pointer in parameter or return of external function my_malloc}}
_Itype_for_any(T) void *my_malloc(unsigned long size) : itype(_Array_ptr<T>) byte_count(size);


// unwritable-expected-warning@+1 {{0 unchecked pointers: Source code in non-writable file}}
void *x; // expected-warning {{1 unchecked pointer: Default void* type}}

void test0() {
  // unwritable-expected-warning@+1 {{0 unchecked pointers: Source code in non-writable file}}
  int *a;
  // unwritable-expected-warning@+1 {{0 unchecked pointers: Source code in non-writable file}}
  char *b;
  // unwritable-expected-warning@+1 {{0 unchecked pointers: Source code in non-writable file}}
  a = b; // expected-warning {{2 unchecked pointers: Cast from char * to int *}}

  // unwritable-expected-warning@+1 {{0 unchecked pointers: Source code in non-writable file}}
  int *c;
  (char *)c; // expected-warning {{1 unchecked pointer: Cast from int * to char *}}

  // unwritable-expected-warning@+1 {{0 unchecked pointers: Source code in non-writable file}}
  int *e;
  // unwritable-expected-warning@+1 {{0 unchecked pointers: Source code in non-writable file}}
  char *f;
  // unwritable-expected-warning@+1 {{0 unchecked pointers: Source code in non-writable file}}
  f = (char *)e; // expected-warning {{2 unchecked pointers: Cast from int * to char *}}
}

void test1() {
  int a;
  // unwritable-expected-warning@+1 {{0 unchecked pointers: Source code in non-writable file}}
  int *b; 
  // unwritable-expected-warning@+1 {{0 unchecked pointers: Source code in non-writable file}}
  b = my_malloc(sizeof(int)); // expected-warning {{1 unchecked pointer: Bad pointer type solution}}
  b[0] = 1;

  union u {
    // unwritable-expected-warning@+1 {{0 unchecked pointers: Source code in non-writable file}}
    int *a; // expected-warning {{1 unchecked pointer: Union or external struct field encountered}}
    // unwritable-expected-warning@+1 {{0 unchecked pointers: Source code in non-writable file}}
    int *b; // expected-warning {{1 unchecked pointer: Union or external struct field encountered}}
  };

  void (*c)(void); // unwritable-expected-warning {{0 unchecked pointers: Source code in non-writable file}}
  c++; // expected-warning {{1 unchecked pointer: Pointer arithmetic performed on a function pointer}}

  // unwritable-expected-warning@+2 {{0 unchecked pointers: Source code in non-writable file}}
  // unwritable-expected-warning@+1 {{0 unchecked pointers: Source code in non-writable file}}
  int *d = my_malloc(1); // expected-warning {{1 unchecked pointer: Unsafe call to allocator function}}
  // unwritable-expected-warning@+2 {{0 unchecked pointers: Source code in non-writable file}}
  // unwritable-expected-warning@+1 {{0 unchecked pointers: Source code in non-writable file}}
  int **e = my_malloc(1); // expected-warning {{1 unchecked pointer: Unsafe call to allocator function}}
}

// unwritable-expected-warning@+2 {{0 unchecked pointers: Source code in non-writable file}}
// expected-warning@+1 {{1 unchecked pointer: External global variable glob has no definition}}
extern int *glob;

// unwritable-expected-warning@+2 {{0 unchecked pointers: Source code in non-writable file}}
// expected-warning@+1 {{1 unchecked pointer: Unchecked pointer in parameter or return of external function glob_f}}
int *glob_f(void);

// unwritable-expected-warning@+1 {{0 unchecked pointers: Source code in non-writable file}}
void (*void_star_fptr)(void *); // expected-warning {{1 unchecked pointer: Default void* type}}
// unwritable-expected-warning@+1 {{ 0 unchecked pointers: Source code in non-writable file}}
void void_star_fn(void *p); // expected-warning {{1 unchecked pointer: Default void* type}}

typedef struct {
  int x;
  float f;
// unwritable-expected-warning@+1 {{0 unchecked pointers: Source code in non-writable file}}
} A, *PA;
// expected-warning@-1 {{2 unchecked pointers: Unable to rewrite a typedef with multiple names}}
// Two pointers affected by the above root cause. Do not count the typedef
// itself as an affected pointer even though that's where the star is written.
// Count each of the variables below even though no star is actually written.
// unwritable-expected-warning@+2 {{0 unchecked pointers: Source code in non-writable file}}
// unwritable-expected-warning@+1 {{0 unchecked pointers: Source code in non-writable file}}
PA pa_test0, pa_test1;

// unwritable-expected-warning@+2 {{0 unchecked pointers: Source code in non-writable file}}
// expected-warning@+1 {{1 unchecked pointer: Internal constraint for generic function declaration, for which 3C currently does not support re-solving.}}
_Itype_for_any(T) void remember(void *p : itype(_Ptr<T>)) {}
