// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -output-dir=%t.checked %s %S/merge_fp2.c --
// RUN: %clang -working-directory=%t.checked -c merge_fp1_xfail.c merge_fp2.c
// RUN: FileCheck -match-full-lines --input-file %t.checked/merge_fp1_xfail.c %s
//
// XFAIL: *

// When this file is first it fails.

// possible assert failure - nothing should merge into undeclared params
int *primary_merge(int *(*secondary_merge)());
// CHECK: _Ptr<int> primary_merge(_Ptr<_Ptr<int> (int *)> secondary_merge);
