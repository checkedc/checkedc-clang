// Tests for 3C.
//
// Checks array heuristics: issue:
// https://github.com/correctcomputation/checkedc-clang/issues/533
//
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -alltypes %s -- | %clang -c -f3c-tool -fcheckedc-extension -x c -o %t1.unused -

extern int baz;
void foo(int *a) {
  for (int i = 0; i < baz; i++) {
    (void) a[i];
  }
}
//CHECK_ALL: void foo(_Array_ptr<int> a : count(baz)) {
