// RUN: 3c -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: 3c -alltypes %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -alltypes %s -- | %clang_cc1  -fno-builtin -verify -fcheckedc-extension -x c -
// RUN: 3c -output-postfix=checked -alltypes %s 
// RUN: 3c -alltypes %S/struct_init_list.checked.c -- | count 0
// RUN: rm %S/struct_init_list.checked.c
// expected-no-diagnostics

struct foo {
  int (*fp)(int *p); 
  // CHECK:  _Ptr<int (int *)> fp;
};

extern int xfunc(int *arg);

int func(int *q) {
// CHECK: int func(int *q) {
  return *q;
}

void bar(void) {
  struct foo f = { &xfunc };
  struct foo g = { &func };
}

struct buz {
  int *x;
  // CHECK: int *x;
};

void buz_test() {
  int x = 0;
  struct buz bar = { (int*) 1};
  bar.x = &x;
}

struct baz {
  int *x;
  // CHECK: _Ptr<int> x;
};

void baz_test() {
  int x = 0;
  int y = 1;
  struct buz bar = {&x};
  bar.x = &y;
}

struct a {
  int *x;
  // CHECK: _Ptr<int> x;
  int *y;
  // CHECK: int *y;
};

struct b {
  int *j;
  // CHECK: _Ptr<int> j;
  struct a a;
  int *k;
  // CHECK: int *k;
};

struct c {
  int *p;
  // int *p;
  struct a a;
  struct b b;
};

void nested_test(int *a, int *b){
// void nested_test(_Ptr<int> a, int *b){
  struct c test = {
    .p = b,
    .a = {
      .x = a,
      .y = b},
    .b = {
      .j = a,
      .a = {
        .x = a,
       	.y = (int*) 1}, 
      .k = b}};
}

struct good {
  int *arr[2];
  // _Ptr<int> arr _Checked[2];
};

struct bad {
  int *arr[2];
  // int *arr _Checked[2];
};

void arr_in_struct(int *a, int *b, int *c) {
  struct good test_good = {{a, b}};
  struct bad test_bad = {{c, (int*)5}};
}
