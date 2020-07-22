// RUN: cconv-standalone %s | FileCheck -match-full-lines %s

void foo(char *a);
void bar(int *a);
void baz(int a[1]);

int *wild();

void test() {
  foo("test");

  int x;
  bar(&x);

  baz((int[1]){1});

  bar(wild());
}
//CHECK: void test(void) {
