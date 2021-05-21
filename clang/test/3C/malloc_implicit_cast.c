// Regression test for failure to detect the `sizeof` in `malloc(x * sizeof(y))`
// if there is an implicit cast around the `sizeof` because `x` is of an integer
// type bigger than `size_t`
// (https://github.com/correctcomputation/checkedc-clang/issues/345).

// Of course, in order to trigger the bug (if it exists), we need an integer
// type bigger than `size_t`. We make a best effort to find one (in our main
// Linux x86_64 environment with the GCC system headers, `size_t` should be 64
// bits and `unsigned __int128` should be available); if we don't find one, the
// test may falsely pass.

// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines %s

#include <stdlib.h>

#include <stdint.h>
#ifdef __SIZEOF_INT128__
typedef unsigned __int128 uintrealmax_t;
#else
typedef uintmax_t uintrealmax_t;
#endif

void foo() {
  uintrealmax_t n = 5;
  // If the bug triggers, we'll get an "Unsafe call to allocator function" root
  // cause here (not tested) and `p` will remain wild.
  int *p = malloc(n * sizeof(int));
  // CHECK: _Array_ptr<int> p : count(n) = malloc<int>(n * sizeof(int));
  p[0] = 42;
}
