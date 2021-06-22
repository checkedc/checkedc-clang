// Test that non-canWrite files are constrained not to change so that the final
// annotations of other files are consistent with the original annotations of
// the non-canWrite files. The currently supported cases are function and
// variable declarations and checked regions.
// (https://github.com/correctcomputation/checkedc-clang/issues/387)

// RUN: rm -rf %t*

// "Lower" case: -base-dir should default to the working directory, so we should
// not allow canwrite_constraints.h to change, and the internal types of q and
// the return should remain wild.
//
// RUN: cd %S
// RUN: 3c -alltypes -addcr -output-dir=%t.checked/base_subdir -warn-all-root-cause %s -- -Xclang -verify
// RUN: FileCheck -match-full-lines -check-prefixes=CHECK_LOWER --input-file %t.checked/base_subdir/canwrite_constraints.c %s
// RUN: test ! -f %t.checked/canwrite_constraints.checked.h

// "Higher" case: When -base-dir is set to the parent directory, we can change
// canwrite_constraints.h, so both q and the return should become checked.
//
// RUN: 3c -alltypes -addcr -base-dir=.. -output-dir=%t.checked2 %s --
// RUN: FileCheck -match-full-lines -check-prefixes=CHECK_HIGHER --input-file %t.checked2/base_subdir/canwrite_constraints.c %s
// RUN: FileCheck -match-full-lines -check-prefixes=CHECK_HIGHER --input-file %t.checked2/canwrite_constraints.h %S/../canwrite_constraints.h

#include "../canwrite_constraints.h"

int *bar(int *q) {
  // CHECK_LOWER: int *bar(int *q : itype(_Ptr<int>)) : itype(_Ptr<int>) {
  // CHECK_HIGHER: _Ptr<int> bar(_Ptr<int> q) _Checked {
  foo(q);
  return foo_var;
}

int gar(intptr a) {
  int *b = a;
  //CHECK_LOWER: int *b = a;
  //CHECK_HIGHER: _Ptr<int> b = a;
  return *b;
}
