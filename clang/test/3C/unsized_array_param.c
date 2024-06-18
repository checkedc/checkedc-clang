// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL" %s
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/unsized_array_param.c -- | diff %t.checked/unsized_array_param.c -

void test1(int *x[]) {}
//CHECK_ALL: void test1(_Ptr<_Ptr<int>> x) _Checked {}

void (*test2)(int *[]);
//CHECK_ALL: _Ptr<void (_Ptr<_Ptr<int>>)> test2 = ((void *)0);

void (*test3[2])(int *[]);
//CHECK_ALL: _Ptr<void (_Ptr<_Ptr<int>>)> test3 _Checked[2] = {((void *)0)};
