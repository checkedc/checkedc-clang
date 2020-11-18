// RUN: 3c %s > %S/temp_bodiless.c
// RUN: %clang -c %S/temp_bodiless.c
// RUN: FileCheck -match-full-lines --input-file %S/temp_bodiless.c %s
// RUN: rm %S/temp_bodiless.c

/***********************************************/
/* Tests that functions without bodies         */
/* are marked wild when there is no interface  */
/***********************************************/

static int *foo1(void) { return (void *)0; }
void test1() {
  int *a = foo1();
}
//CHECK _Ptr<int> a = foo1();

static int *foo2(void);
void test2() {
  int *a = foo2();
}
//CHECK int *a = foo2();

int *foo3(void) { return (void *)0; }
void test3() {
  int *a = foo3();
}
//CHECK: _Ptr<int> a = foo3();

int *foo4(void);
void test4() {
  int *a = foo4();
}
//CHECK: int *a = foo4();

int *foo5(void) : itype(_Ptr<int>);
void test5() {
  int *a = foo5();
}
//CHECK: _Ptr<int> a = foo5();

extern int *foo6(void);
void test6() {
  int *a = foo6();
}
//CHECK: int *a = foo6();

extern int *foo7(void) : itype(_Ptr<int>);
void test7() {
  int *a = foo7();
}
//CHECK: _Ptr<int> a = foo7();


// parameters are not defined and therefore unchecked
extern int *foo8() : itype(_Ptr<int>);
void test8() {
  int *a = foo8();
}
//CHECK: int *a = foo8();

