// RUN: 3c -base-dir=%S -alltypes -output-postfix=checkedALL %s
// RUN: 3c -base-dir=%S -output-postfix=checkedNOALL %s
// RUN: %clang -c %S/qualifiers.checkedNOALL.c
// RUN: FileCheck -match-full-lines --input-file %S/qualifiers.checkedNOALL.c %s
// RUN: FileCheck -match-full-lines --input-file %S/qualifiers.checkedALL.c %s
// RUN: rm %S/qualifiers.checkedALL.c %S/qualifiers.checkedNOALL.c

void consts() {
  const int a;
  const int *b;
  int * const c;
  int * const * d;
  int ** const e ;
  int * const * const f;
}
//CHECK: const int a;
//CHECK: _Ptr<const int> b = ((void *)0);
//CHECK: const _Ptr<int> c = ((void *)0);
//CHECK: _Ptr<const _Ptr<int>> d = ((void *)0);
//CHECK: const _Ptr<_Ptr<int>> e = ((void *)0) ;
//CHECK: const _Ptr<const _Ptr<int>> f = ((void *)0);

void volatiles() {
  volatile int a;
  volatile int *b;
  int * volatile c;
  int * volatile * d;
  int ** volatile e ;
  int * volatile * volatile f;
}
//CHECK: volatile int a;
//CHECK: _Ptr<volatile int> b = ((void *)0);
//CHECK: volatile _Ptr<int> c = ((void *)0);
//CHECK: _Ptr<volatile _Ptr<int>> d = ((void *)0);
//CHECK: volatile _Ptr<_Ptr<int>> e = ((void *)0) ;
//CHECK: volatile _Ptr<volatile _Ptr<int>> f = ((void *)0);

void restricts() {
  int * restrict c;
  int * restrict * d;
  int ** restrict e ;
  int * restrict * restrict f;
}
//CHECK: restrict _Ptr<int> c = ((void *)0);
//CHECK: _Ptr<restrict _Ptr<int>> d = ((void *)0);
//CHECK: restrict _Ptr<_Ptr<int>> e = ((void *)0) ;
//CHECK: restrict _Ptr<restrict _Ptr<int>> f = ((void *)0);

void mixed() {
  int * const volatile restrict  a;
  const volatile int * const * volatile const * restrict b;
}
//CHECK: const volatile restrict _Ptr<int> a = ((void *)0);
//CHECK: restrict _Ptr<const volatile _Ptr<const _Ptr<const volatile int>>> b = ((void *)0);

struct qualifier_struct { int *a; };
void structs() {
  struct qualifier_struct a0;
  static struct qualifier_struct a;
  volatile static struct qualifier_struct b;
  const static struct qualifier_struct *c;
  const extern struct qualifier_struct d;
}
//CHECK: struct qualifier_struct a0 = {};
//CHECK: static struct qualifier_struct a = {};
//CHECK: static volatile struct qualifier_struct b = {};
//CHECK: static _Ptr<const struct qualifier_struct> c = ((void *)0);
//CHECK: const extern struct qualifier_struct d;
