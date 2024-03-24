// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -output-dir=%t.checked %s %S/merge_fp1_xfail.c --
// RUN: %clang -working-directory=%t.checked -c merge_fp1_xfail.c merge_fp2.c
// RUN: FileCheck -match-full-lines --input-file %t.checked/merge_fp2.c %s

// When this file is first it succeeds, when second it fails.

int *primary_merge(int *(*secondary_merge)(int *));
// CHECK: _Ptr<int> primary_merge(_Ptr<_Ptr<int> (int *)> secondary_merge);

// need this to produce changes (0 is safe, and clang merged the declarations)
int *primary_merge(int *(*secondary_merge)()) {
  // CHECK: _Ptr<int> primary_merge(_Ptr<_Ptr<int> (int *)> secondary_merge) {

  return ((*secondary_merge)(0));
}
