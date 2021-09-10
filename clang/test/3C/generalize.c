// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/generalize.c -- | diff %t.checked/generalize.c -

// Test the code that adds generics to replace void*

// Basic functionality
void viewer(void *i) { return; }
// CHECK: _For_any(T) void viewer(_Ptr<T> i) { return; }
void viewer_badnum(void *i, int *j) {
// CHECK: _Itype_for_any(T) void viewer_badnum(_Ptr<T> i, int *j : itype(_Ptr<int>)) {
  j = (int*)3;
  return;
}
void viewer_update(void *i) {
// CHECK: void viewer_update(void *i) {
  i = 2;
  return;
}
void *getNull() { return 0; }
// CHECK: _For_any(T) _Ptr<T> getNull(void) { return 0; }
void *getOne() { return 1; }
// CHECK: void *getOne() { return 1; }
extern void *glob = 0;
void *getGlobal() { return glob; }
// CHECK: void *getGlobal() { return glob; }

// check type parameters
void call_from_fn() {
  // CHECK: void call_from_fn() _Checked {
  int *i;
  viewer(i);
  // CHECK: viewer<int>(i);
}
void call_from_gen_fn(void *i) {
  // CHECK: _For_any(T) void call_from_gen_fn(_Ptr<T> i) {
  viewer(i);
  // CHECK: viewer<T>(i);
}

// nameless decls use a different code path for rewriting,
// second param forces rewrite
// check to see that we don't rewrite unsafe vals. ie T*
void nameless(void *, char *);
void nameless(void *a, char *b)
{
  a = 1; // make it unsafe
}
// CHECK: void nameless(void *a, _Ptr<char> b);
// CHECK: void nameless(void *a, _Ptr<char> b)

// Safe functions should be upgraded from "_Itype_for_any" to "_For_any"
_Itype_for_any(T) void has_safe_params(_Ptr<T> i, int *t : itype(_Ptr<int>)) {}
// CHECK: _For_any(T) void has_safe_params(_Ptr<T> i, _Ptr<int> t) _Checked {}

// itypes are not converted to generics
int recv0(void *buf : itype(_Array_ptr<void>) byte_count(n), int n) {}
// CHECK: int recv0(void *buf : itype(_Array_ptr<void>) byte_count(n), int n) {}

// greater depth pointers are not converted to generics
void double_ptr(void** dp) {}
// CHECK: void double_ptr(void** dp) {}

// externs are not converted to generics
void elsewhere(void *x, int *y);
// CHECK: void elsewhere(void *x, int *y);

// existing generics are not rewritten
_For_any(T) void forany(T* t, _Ptr<T> p, void * v) {}
// CHECK: _For_any(T) void forany(_Ptr<T> t, _Ptr<T> p, void * v) {}

// functions with multiple potential generics are not rewritten
void twoparams(void *a, void *b) {}
void* inout(void *in) {}
// CHECK: void twoparams(void *a, void *b) {}
// CHECK: void* inout(void *in) {}

// safe pointers are not converted to generics
void safevoid(_Ptr<void> s){}
// CHECK: void safevoid(_Ptr<void> s){}



// Code reduced from parsons
_Itype_for_any(T) void sys_free(void *free_ptr : itype(_Ptr<T>));
void extern_fp((*free_fun)(void*));
static void wrap_free(void *wrap_ptr) {
  sys_free(wrap_ptr);
  // CHECK: sys_free<void>(wrap_ptr);
}
void make_wild(void) {
  extern_fp(wrap_free);
}

// Code from vsftpd
#include <stdlib.h>
#include <limits.h>
extern void bug(char*);
extern void die(char*);

// generics can be added, but not internally, so the result is unchecked
// and we don't add generics for unchecked potential params
void* vsf_sysutil_malloc(unsigned int size)
// CHECK: void* vsf_sysutil_malloc(unsigned int size)
{
  void* p_ret;
  /* Paranoia - what if we got an integer overflow/underflow? */
  if (size == 0 || size > INT_MAX)
  {
    bug("zero or big size in vsf_sysutil_malloc");
  }
  p_ret = malloc(size);
  if (p_ret == NULL)
  {
    die("malloc");
  }
  return p_ret;
}

// current generics strategy only converts protos with 1 `void*`
void* vsf_sysutil_realloc(void* p_ptr, unsigned int size)
// CHECK: void* vsf_sysutil_realloc(void* p_ptr, unsigned int size)
{
  void* p_ret;
  if (size == 0 || size > INT_MAX)
  {
    bug("zero or big size in vsf_sysutil_realloc");
  }
  p_ret = realloc(p_ptr, size);
  if (p_ret == NULL)
  {
    die("realloc");
  }
  return p_ret;
}

void
vsf_sysutil_free(void* p_ptr)
// CHECK: _For_any(T) void vsf_sysutil_free(_Ptr<T> p_ptr)
{
  if (p_ptr == NULL)
  {
    bug("vsf_sysutil_free got a null pointer");
  }
  free(p_ptr);
}

void run_vsf_sysutil (void) {
  typedef struct {char a; char b;} char_node;
  char_node *node1;
  // These currently return unsafe values, which hits a bug in 3c:
  // https://github.com/correctcomputation/checkedc-clang/issues/622
  //node1 = vsf_sysutil_malloc(sizeof(*node1));
  //node1 = vsf_sysutil_realloc(node1, sizeof(*node1));
  vsf_sysutil_free(node1);
  // CHECK: vsf_sysutil_free<char_node>(node1);
}
