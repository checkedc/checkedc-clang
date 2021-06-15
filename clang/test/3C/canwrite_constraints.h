// Used by base_subdir/canwrite_constraints.c .

// Note: Only the test in which this file is non-writable verifies diagnostics
// and will use the expected warning comments below. The test in which this file
// is writable does not verify diagnostics and ignores those comments.

// "@+1" means "on the next line". If we put the comment on the same line, it
// breaks the CHECK_HIGHER.
// expected-warning@+1 {{Declaration in non-writable file}}
inline void foo(int *p) {}
// CHECK_HIGHER: inline void foo(_Ptr<int> p) _Checked {}

// expected-warning@+1 {{Declaration in non-writable file}}
int *foo_var = ((void *)0);
// CHECK_HIGHER: _Ptr<int> foo_var = ((void *)0);

// Make sure we do not add checked regions in non-writable files. This happened
// incidentally in system headers in some other regression tests, but this is a
// dedicated test for it.
inline void no_op() {}
// CHECK_HIGHER: inline void no_op() _Checked {}

// Test the unwritable cast internal warning
// (https://github.com/correctcomputation/checkedc-clang/issues/454) using the
// known bug with itypes and function pointers
// (https://github.com/correctcomputation/checkedc-clang/issues/423) as an
// example.
void unwritable_cast(void((*g)(int *q)) : itype(_Ptr<void(_Ptr<int>)>)) {
  // expected-warning@+1 {{Declaration in non-writable file}}
  int *p = 0;
  // Now 3C thinks it needs to insert _Assume_bounds_cast<_Ptr<int>> around `p`
  // because it forgets that it is allowed to use the original type of `g`.
  // expected-warning@+1 {{3C internal error: tried to insert a cast into an unwritable file}}
  (*g)(p);
}
