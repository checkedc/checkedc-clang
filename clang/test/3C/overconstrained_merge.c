// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/overconstrained_merge.c -- | diff %t.checked/overconstrained_merge.c -

// Test checks for merging a function after the generics code adds a constraint
// The merge succeeds because it happens earlier in the codebase than the addition

// This test only uses -alltypes because of the use of _Array_ptr in the example
// (to distinguish from the _Ptr that will be added).
_Itype_for_any(T) void free(void * : itype(_Array_ptr<T>));
void b(void) {
  char **c;
  free(c);
  // CHECK: _Array_ptr<_Ptr<char>> c = ((void *)0);
  // CHECK: free<_Ptr<char>>(c);
}
void free(void *);
