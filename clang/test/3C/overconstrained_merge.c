// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/overconstrained_merge.c -- | diff %t.checked/overconstrained_merge.c -

// Test checks for merging a function after the generics code adds a constraint.
// The merge succeeds because it happens earlier in the codebase than the
// addition.

// This test only uses -alltypes because of the use of _Array_ptr in the example
// (to distinguish from the _Ptr that will be added).

// This test uses a sequence of declarations of this function that differs in an
// important way from the one for the system `free` (the fully unchecked
// declaration comes second), so we use a different name for this function to
// avoid any confusion with the system `free`.
_Itype_for_any(T) void my_free(void * : itype(_Array_ptr<T>));
void b(void) {
  char **c;
  my_free(c);
  // CHECK: _Array_ptr<_Ptr<char>> c = ((void *)0);
  // CHECK: my_free<_Ptr<char>>(c);
}
void my_free(void *);
