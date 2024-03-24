// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/struct_macro.c -- | diff %t.checked/struct_macro.c -

// 3C was failing to create constraint variables for fields of structure
// defined by macros.
// https://github.com/correctcomputation/checkedc-clang/issues/297

// Fields is not in macro, so can be checked

#define b struct c
b { int *a; };
//CHECK: b { _Ptr<int> a; };
void e(struct c x) {
  int *y = x.a;
  //CHECK: _Ptr<int> y = x.a;
}

// clang-format messes up this part of the file because it mistakes macro
// references for other syntactic constructs.
// clang-format off

// Fields is in macro, so it can't be checked.
// A constraint variable still must be created.
#define foo                                                                    \
  struct bar {                                                                 \
    int *x                                                                     \
  };
foo

void baz(struct bar buz) {
  int *x = buz.x;
  //CHECK: int *x = buz.x;
}

// clang-format on
