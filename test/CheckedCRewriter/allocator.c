// Tests for Checked C rewriter tool.
//
// Tests for malloc and friends. 
//
// RUN: checked-c-convert %s -- | FileCheck -match-full-lines %s
// RUN: checked-c-convert %s -- | %clang_cc1 -fno-builtin -verify -fcheckedc-extension -x c -
// expected-no-diagnostics
//
typedef __SIZE_TYPE__ size_t;
extern void *malloc(size_t n) : byte_count(n);
extern void free(void *);

void dosomething(void) {
  int a = 0;
  int *b = &a;
  *b = 1;
  return;
}

void foo(void) {
  int *a = (int *) malloc(sizeof(int));
  *a = 0;
  free((void *)a);
  return;
}
//CHECK: void foo(void) {
//CHECK-NEXT: int *a = (int *) malloc(sizeof(int));
//CHECK-NEXT: *a = 0;

typedef struct _listelt {
  struct _listelt *next;
  int val;
} listelt;

typedef struct _listhead {
  listelt *hd;
} listhead;

void add_some_stuff(listhead *hd) {
  listelt *l1 = (listelt *) malloc(sizeof(listelt));
  l1->next = 0;
  l1->val = 0;
  listelt *cur = hd->hd;
  while (cur) {
    cur = cur->next;
  }
  cur->next = l1;
  return;
}
//CHECK: void add_some_stuff(_Ptr<listhead>  hd) {
//CHECK-NEXT: _Ptr<listelt>  l1 = (listelt *) malloc(sizeof(listelt));
