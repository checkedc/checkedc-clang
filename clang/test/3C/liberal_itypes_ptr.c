// RUN: 3c -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -alltypes -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -alltypes -output-postfix=checked %s
// RUN: 3c -alltypes %S/liberal_itypes_ptr.checked.c -- | count 0
// RUN: rm %S/liberal_itypes_ptr.checked.c

void foo(int *a) { }
// CHECK: void foo(_Ptr<int> a) _Checked { }

void bar() {
  int *b = 1;
  // CHECK: int *b = 1;
  foo(b);
  // CHECK: foo(_Assume_bounds_cast<_Ptr<int>>(b));
}

void baz() {
  int *b = 0;
  // CHECK: _Ptr<int> b = 0;
  foo(b);
  // CHECK: foo(b);
}

int *buz() { return 0; }
// CHECK: _Ptr<int> buz(void) _Checked { return 0; }

void boz() {
// CHECK: void boz() {
  int *b  = 1;
  // CHECK: int *b  = 1;
  b = buz();
  // CHECK: b = ((int *)buz());
}

void biz() {
// CHECK: void biz() _Checked {
  int *b  = 0;
  // CHECK: _Ptr<int> b = 0;
  b = buz();
  // CHECK: b = buz();
}

typedef unsigned long size_t;
_Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
_Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));

void malloc_test() {
    int *p = malloc(sizeof(int));
    // CHECK: int *p = malloc<int>(sizeof(int));
    p = 1;
}

void free_test() {
   int *a;
   // CHECK: _Ptr<int> a = ((void *)0);
   free(a);
   // CHECK: free<int>(a);
}

void unsafe(int *a) {
// CHECK: void unsafe(int *a : itype(_Ptr<int>)) {
  a = 1;
}

int *unsafe_return() {
// CHECK: int *unsafe_return(void) : itype(_Ptr<int>) _Checked {
  return 1;
}

void caller() {
  int *b;
  // CHECK: _Ptr<int> b = ((void *)0);
  unsafe(b);
  // CHECK: unsafe(b);

  int *c = 1;
  // CHECK: int *c = 1;
  unsafe(c);
  // CHECK: unsafe(c);

  int *d = unsafe_return();
  // CHECK: _Ptr<int> d = unsafe_return();

  int *e = unsafe_return();
  // CHECK: int *e = unsafe_return();
  e = 1;
}

void checked(_Ptr<int> i){ }
// CHECK: void checked(_Ptr<int> i)_Checked { }

void itype_unsafe(int *i : itype(_Ptr<int>)){ i = 1; }
// CHECK: void itype_unsafe(int *i : itype(_Ptr<int>)){ i = 1; }

void itype_safe(int *i : itype(_Ptr<int>)){ i = 0; }
// CHECK: void itype_safe(int *i : itype(_Ptr<int>))_Checked { i = 0; }

void void_ptr(void *p, void *q) {
// CHECK: void void_ptr(void *p, void *q) {
  p = 1;
}

void int_ptr_arg(int *a) {}
// CHECK: void int_ptr_arg(_Ptr<int> a) _Checked {}

void char_ptr_param() {
  int_ptr_arg((char*) 1);
  // CHECK: int_ptr_arg(_Assume_bounds_cast<_Ptr<int>>((char*) 1));

  int *c;
  // CHECK: _Ptr<int> c = ((void *)0);
  int_ptr_arg(c);
  // CHECK: int_ptr_arg(c);
}

void bounds_fn(void *b : byte_count(1));
// CHECK: void bounds_fn(void *b : byte_count(1));

void bounds_call(void *p) {
// CHECK: void bounds_call(void *p) {
   bounds_fn(p);
   // CHECK: bounds_fn(p);
}

#define macro_cast(x) macro_cast_fn(x)

void macro_cast_fn(int *y) { }
// CHECK: void macro_cast_fn(_Ptr<int> y) _Checked { }

void macro_cast_caller() {
  int *z = 1;
  // CHECK: int *z = 1;
  macro_cast(z);
  // CHECK: macro_cast(_Assume_bounds_cast<_Ptr<int>>(z));
}

char *unused_return_unchecked();
char *unused_return_checked() {return 0;}
char *unused_return_itype() {return 1;}
char **unused_return_unchecked_ptrptr();
//CHECK: char *unused_return_unchecked();
//CHECK: _Ptr<char> unused_return_checked(void) _Checked {return 0;}
//CHECK: char *unused_return_itype(void) : itype(_Ptr<char>) _Checked {return 1;}
//CHECK: char **unused_return_unchecked_ptrptr();

void dont_cast_unused_return() {
   unused_return_unchecked();
   *unused_return_unchecked();
   (void) unused_return_unchecked();
   //CHECK: unused_return_unchecked();
   //CHECK: *unused_return_unchecked();
   //CHECK: (void) unused_return_unchecked();

   unused_return_checked();
   *unused_return_checked();
   (void) unused_return_checked();
   //CHECK: unused_return_checked();
   //CHECK: *unused_return_checked();
   //CHECK: (void) unused_return_checked();

   unused_return_itype();
   *unused_return_itype();
   (void) unused_return_itype();
   //CHECK: unused_return_itype();
   //CHECK: *unused_return_itype();
   //CHECK: (void) unused_return_itype();

   unused_return_unchecked_ptrptr();
   *unused_return_unchecked_ptrptr();
   (void) unused_return_unchecked_ptrptr();
   //CHECK: unused_return_unchecked_ptrptr();
   //CHECK: *unused_return_unchecked_ptrptr();
   //CHECK: (void) unused_return_unchecked_ptrptr();
}
