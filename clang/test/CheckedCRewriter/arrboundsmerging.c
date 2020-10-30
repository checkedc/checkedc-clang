// RUN: cconv-standalone -disable-arr-hu -alltypes %s -- | FileCheck -match-full-lines %s


/*
Advanced array-bounds inference (based on control-dependencies).
*/

typedef unsigned long size_t;
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

// Here x bounds will be c
void foo(int *x, int c) {
//CHECK: void foo(_Array_ptr<int> x : count(c), int c) {

  x[3] = c;
}

// Here x will be of constant size
void foo2(int *x, int c) {
//CHECK: void foo2(_Array_ptr<int> x : count(8), int c) {
  x[3] = c;
}

// Here x bounds is c but the violates bounds.
void foo3(int *x, int c) {
//CHECK: void foo3(_Array_ptr<int> x, int c) {
  x[0] = c;
}

void foo4(int *x, int c) {
//CHECK: void foo4(_Array_ptr<int> x, int c) {
  x[0] = c;
}

void bar(void) {
  int *p = malloc(sizeof(int)*8);
  int *q = malloc(sizeof(int)*8);
  int *p1 = malloc(sizeof(int)*8);
  int *q1 = malloc(sizeof(int)*8);
//CHECK: _Array_ptr<int> p : count(8) = malloc<int>(sizeof(int)*8);
//CHECK: _Array_ptr<int> q : count(8) = malloc<int>(sizeof(int)*8);
//CHECK: _Array_ptr<int> p1 : count(8) = malloc<int>(sizeof(int)*8);
//CHECK: _Array_ptr<int> q1 : count(8) = malloc<int>(sizeof(int)*8);

  int n = 8;
  int l = 4;
  int *q2 = malloc(sizeof(int)*l);
  int *q4 = malloc(sizeof(int)*n);

  // Variation 1
  // Correct size association: second argument is indeed the size.
  foo(p,n);
  foo(q,8);

  // Variation 2
  // passing fixed size array: No size association.
  foo2(p1,8);
  foo2(q1,28);

  // Variation 3
  // Variable sized arrays: One correct and one wrong bounds.
  foo3(q2,l);
  foo3(q1,28);

  // Variation 4
  // Passing wrong lengths to check bounds.
  foo4(q2,n);
  foo4(q4,l);
}
