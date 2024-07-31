// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- -Wno-error=int-conversion | %clang -c -Wno-error=int-conversion -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s -- -Wno-error=int-conversion
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/add_struct_init.c -- -Wno-error=int-conversion | diff %t.checked/add_struct_init.c -

struct foo {};
struct bar {
  int *a;
  //CHECK: _Ptr<int> a;
};
struct fiz {
  int *a;
};
struct buz {
  struct bar a;
};
struct baz {
  struct buz a;
};
struct fuz {
  struct baz a;
  struct fiz b;
};
struct biz {
  struct fiz b;
};

void test() {
  struct foo a;
  struct bar b;
  struct fiz c;
  struct buz d;
  struct baz e;
  struct fuz f;
  struct biz g;
  //CHECK: struct foo a;
  //CHECK: struct bar b = {};
  //CHECK: struct fiz c;
  //CHECK: struct buz d = {};
  //CHECK: struct baz e = {};
  //CHECK: struct fuz f = {};
  //CHECK: struct biz g;

  c.a = 1;
}
