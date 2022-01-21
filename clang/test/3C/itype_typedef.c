// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/itype_typedef.c -- | diff %t.checked/itype_typedef.c -

// Itype typedef on the paramter

typedef int *td0;
void test0(td0 a) {
//CHECK: typedef _Ptr<int> td0;
//CHECK: void test0(int *a : itype(td0)) {
  a = 1;
}

typedef int *td1;
void test1(td1 a) {
//CHECK: typedef int *td1;
//CHECK: void test1(td1 a : itype(_Ptr<int>)) {
  td1 b = a;
  b = 1;
}

typedef int *td2;
void test2(td2 a) {
//CHECK: typedef int *td2;
//CHECK: void test2(td2 a : itype(_Ptr<int>)) {
  td2 b = 1;
}

// Itype typedef on the return

typedef int *td3;
td3 test3() {
//CHECK: typedef _Ptr<int> td3;
//CHECK: int *test3(void) : itype(td3) {
  return (int*) 1;
}

typedef int *td4;
td4 test4() {
//CHECK: typedef int *td4;
//CHECK: td4 test4(void) : itype(_Ptr<int>) {
  td4 a = 1;
  return a;
}

// Itype typedef with array bounds

typedef int *td5;
void test5(td5 a, int n) {
//CHECK: typedef int *td5;
//CHECK_ALL: void test5(td5 a : itype(_Array_ptr<int>) count(n), int n) {
//CHECK_NOALL: void test5(td5 a : itype(_Ptr<int>), int n) {
  for (int i = 0; i < n; i++)
    a[i];
  td5 b = 1;
}

typedef int *td6;
void test6(td6 a, int n) {
//CHECK_ALL: typedef _Array_ptr<int> td6;
//CHECK_ALL: void test6(int *a : itype(td6) count(n), int n) {
//CHECK_NOALL: typedef _Ptr<int> td6;
//CHECK_NOALL: void test6(int *a : itype(td6), int n) {
  for (int i = 0; i < n; i++)
    a[i];
  a = 1;
}

// Itype typedef with type qualifiers

typedef int *td7;
void test7(const td7 a) {
//CHECK: typedef _Ptr<int> td7;
//CHECK: void test7(int *const a : itype(const td7)) {
  int *b = a;
  b = 1;
}

typedef int *td8;
void test8(const td8 a) {
//CHECK: typedef int *td8;
//CHECK: void test8(const td8 a : itype(const _Ptr<int>)) {
  td8 b = a;
  b = 1;
}

typedef const int *td9;
void test9(td9 a) {
//CHECK: typedef _Ptr<const int> td9;
//CHECK: void test9(const int *a : itype(td9)) {
  int *b = a;
  b = 1;
}

typedef const int *td10;
void test10(td10 a) {
//CHECK: typedef const int *td10;
//CHECK: void test10(td10 a : itype(_Ptr<const int>)) {
  td10 b = a;
  b = 1;
}

// With functions pointers

typedef int (*fp_td0)(int);
void fp_test0(fp_td0 a) {
//CHECK: typedef _Ptr<int (int)> fp_td0;
//CHECK: void fp_test0(int ((*a)(int)) : itype(fp_td0)) {
  a = 1;
}

typedef int (*fp_td1)(int);
void fp_test1(fp_td1 a) {
//CHECK: typedef int (*fp_td1)(int);
//CHECK: void fp_test1(fp_td1 a : itype(_Ptr<int (int)>)) {
  fp_td1 b = 1;
}
