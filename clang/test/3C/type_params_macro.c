// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o %t1.unused -

// Test for invalid type arguments from a macro,
// recognized also my checked regions

#define baz foo<int>(i);
#define buz foo(i);
#define buzzy                                                                  \
  foo(i);                                                                      \
  foo(j);

_Itype_for_any(T) void foo(void *x : itype(_Ptr<T>));

void test_none() {
  int *i = 0;
  foo(i);
}
// CHECK: void test_none() _Checked {
// CHECK: _Ptr<int> i = 0;
// CHECK: foo<int>(i);

void test_one_given() {
  int *i = 0;
  baz
}
// CHECK: void test_one_given() _Checked {
// CHECK: _Ptr<int> i = 0;

void test_one() {
  int *i = 0;
  buz
}
// CHECK: void test_one() {
// CHECK:  int *i = 0;
// CHECK:  buz

void test_two() {
  int *i = 0;
  int *j = 0;
  buzzy
}

// CHECK: void test_two() {
// CHECK:  int *i = 0;
// CHECK:  int *j = 0;
// CHECK:  buzzy
