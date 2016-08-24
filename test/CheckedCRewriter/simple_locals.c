// Tests for Checked C rewriter tool.
//
// Checks very simple inference properties for local variables.
//
// RUN: checked-c-convert %s -- | FileCheck %s
// RUN: checked-c-convert %s -- | %clang_cc1 -verify -fcheckedc-extension -x c -
// expected-no-diagnostics

void f1() {
    int b = 0;
    int *a = &b;
    *a = 1;
}
// CHECK: void f1() {
// CHECK-NEXT: int b = 0;
// CHECK-NEXT: ptr<int> a = &b;

void f2() {
    char b = 'a';
    char *a = &b;
    *a = 'b';
}
//CHECK: void f2() {
//CHECK-NEXT: char b = 'a';
//CHECK-NEXT: ptr<char> a = &b;

typedef struct _BarRec {
  int a;
  int b;
  int c;
  int *d;
} BarRec;

void upd(BarRec *P, int a) {
  P->a = a;
}
//CHECK: void upd(ptr<BarRec> P, int a) {
//CHECK-NEXT: P->a = a;
//CHECK-NEXT: }

void canthelp(int *a, int b, int c) {
  *(a + b) = c;
}
//CHECK: void canthelp(int *a, int b, int c) {
//CHECK-NEXT:  *(a + b) = c;
//CHECK-NEXT: }

void partialhelp(int *a, int b, int c) {
  int *d = a;
  *d = 0;
  *(a + b) = c;
}
//CHECK: void partialhelp(int *a, int b, int c) {
//CHECK-NEXT: ptr<int> d = a;
//CHECK-NEXT: *d = 0;
//CHECK-NEXT:  *(a + b) = c;
//CHECK-NEXT: }

void g(void) {
    int a = 0, *b = &a;
    *b = 1;
}
//CHECK: void g(void) {
//CHECK-NEXT: int a = 0;
//CHECK-NEXT: ptr<int> b = &a;

void gg(void) {
  int a = 0, *b = &a, **c = &b;

  *b = 1;
  **c = 2;
}
//CHECK: void gg(void) {
//CHECK-NEXT: int a = 0;
//CHECK-NEXT: ptr<int> b = &a;
//CHECK-NEXT: ptr<ptr<int> > c = &b;

#define ONE 1

int goo(int *, int);
//CHECK: int goo(ptr<int>, int);

struct blah {
  int a;
  int b;
  struct blah *c;
};

int bar(int, int);

int foo(int a, int b) {
  int tmp = a + ONE;
  int *tmp2 = &tmp;
  return tmp + b + *tmp2;
}
//CHECK: int foo(int a, int b) {
//CHECK-NEXT: int tmp = a + ONE;
//CHECK-NEXT: ptr<int> tmp2 = &tmp;
//CHECK-NEXT: return tmp + b + *tmp2;
//CHECK-NEXT: }

int bar(int a, int b) {
  return a + b;
}
//CHECK: int bar(int a, int b) {
//CHECK-NEXT: return a + b;
//CHECK-NEXT: }

int baz(int *a, int b, int c) {
  int tmp = b + c;
  int *aa = a;
  *aa = tmp;
  return tmp;
}
//CHECK: int baz(ptr<int> a, int b, int c) {
//CHECK-NEXT: int tmp = b + c;
//CHECK-NEXT: ptr<int> aa = a;
//CHECK-NEXT: *aa = tmp;
//CHECK-NEXT: return tmp;

int arrcheck(int *a, int b) {
  return a[b];
}
//CHECK: int arrcheck(int *a, int b) {
//CHECK-NEXT: return a[b];
//CHECK-NEXT: }

int badcall(int *a, int b) {
  return arrcheck(a, b);
}
//CHECK: int badcall(int *a, int b) {
//CHECK-NEXT: return arrcheck(a, b); 
//CHECK-NEXT: }

void pullit(char *base, char *out, int *index) {
  char tmp = base[*index];
  *out = tmp;
  *index = *index + 1;

  return;
}
//CHECK: void pullit(char* base, ptr<char> out, ptr<int> index) {

void driver() {
  char buf[10] = { 0 };
  int index = 0;
  char v;

  pullit(buf, &v, &index);
  pullit(buf, &v, &index);
  pullit(buf, &v, &index);
}