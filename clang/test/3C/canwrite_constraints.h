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

// Make sure we don't allow the types of casts in non-writable files to be
// changed. This problem was seen with the inline definition of `atol(p)` as
// `strtol(p, (char **) NULL, 10)` in the system headers.
void strtol_like(int *p : itype(_Ptr<int>));
void atol_like() {
  // expected-warning@+1 {{Expression in non-writable file}}
  strtol_like((int *)0);
}

// Make sure we do not add checked regions in non-writable files. This happened
// incidentally in system headers in some other regression tests, but this is a
// dedicated test for it.
inline void no_op() {}
// CHECK_HIGHER: inline void no_op() _Checked {}

// In the lower case, this should stay wild
// In the higher case, this should solve to checked
// expected-warning@+1 {{Declaration in non-writable file}}
typedef int *intptr;
// CHECK_HIGHER: typedef _Ptr<int> intptr;
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

// Make sure that FVComponentVariable::equateWithItype prevents both of these
// from being changed.
//
// TODO: Add the correct expected root cause warnings once diagnostic
// verification is re-enabled
// (https://github.com/correctcomputation/checkedc-clang/issues/503).
void unwritable_func_with_itype(int *p : itype(_Array_ptr<int>)) {}

void unwritable_func_with_itype_and_bounds(int *p
                                           : itype(_Array_ptr<int>) count(12)) {
}
