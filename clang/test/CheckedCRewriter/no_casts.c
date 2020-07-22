// RUN: cconv-standalone %s | count 0

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
