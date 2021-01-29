// Used by base_subdir/canwrite_constraints_function_and_variable.c .

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
