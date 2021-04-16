// Test that "%s" arguments to printf-like functions get constrained to
// _Nt_array_ptr
// (https://github.com/correctcomputation/checkedc-clang/issues/549).

// RUN: 3c -alltypes -base-dir=%S %s -- | FileCheck -match-full-lines %s

void my_printf_like_function(int foo, const char *format
                             : itype(_Nt_array_ptr<const char>), int bar, ...)
    __attribute__((format(printf, 2, 4)));

// `%s` and `%p` are both valid conversion specifiers for a `char *`: `%s`
// prints it as a string while `%p` prints the pointer value. We keep everything
// else the same to test that the conversion specifier causes the difference in
// pointer type between s1 and s2. For good measure, test that `const char *`
// also works and we don't fail on non-strings.

void test(int foo, int bar, char *s1, char *s2, int i3, const char *s4) {
  // CHECK: void test(int foo, int bar, _Nt_array_ptr<char> s1, _Ptr<char> s2, int i3, _Nt_array_ptr<const char> s4) {
  my_printf_like_function(foo, "%s %p %d %s", bar, s1, s2, i3, s4);
}
