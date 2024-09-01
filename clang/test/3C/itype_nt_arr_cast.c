// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/itype_nt_arr_cast.c -- | diff %t.checked/itype_nt_arr_cast.c -

char *fn1() {
//CHECK_ALL: char *fn1(void) : itype(_Nt_array_ptr<char>) {
//CHECK_NOALL: char *fn1(void) : itype(_Ptr<char>) {
  if (0)
    return "";
  char *x = 1;
  return x;
}
char *caller1() {
  char *a = fn1();
//CHECK_ALL: _Nt_array_ptr<char> caller1(void) _Checked {
//CHECK_ALL:   _Nt_array_ptr<char> a = ((_Nt_array_ptr<char> )fn1());
//CHECK_NOALL: _Ptr<char> caller1(void) _Checked {
//CHECK_NOALL:   _Ptr<char> a = fn1();
  return a;
}


char *fn2(void) : itype(_Nt_array_ptr<char>);
char *caller2() {
  char *a = fn2();
//CHECK_ALL: _Nt_array_ptr<char> caller2(void) _Checked {
//CHECK_ALL:   _Nt_array_ptr<char> a = ((_Nt_array_ptr<char> )fn2());
//CHECK_NOALL: char *caller2(void) : itype(_Ptr<char>) {
  return a;
}

char *(*fn3)() = fn1;
char *caller3() {
  char *a = fn3();
//CHECK_ALL: _Ptr<char *(void) : itype(_Nt_array_ptr<char>)> fn3 = fn1;
//CHECK_ALL: _Nt_array_ptr<char> caller3(void) _Checked {
//CHECK_ALL:   _Nt_array_ptr<char> a = ((_Nt_array_ptr<char> )fn3());
//CHECK_NOALL: _Ptr<char *(void) : itype(_Ptr<char>)> fn3 = fn1;
//CHECK_NOALL: _Ptr<char> caller3(void) _Checked {
//CHECK_NOALL:   _Ptr<char> a = fn3();
  return a;
}

char *fn4(_Nt_array_ptr<char>);
char *caller4(char *c) {
//CHECK_ALL: char *caller4(char *c : itype(_Nt_array_ptr<char>)) : itype(_Nt_array_ptr<char>) {
//CHECK_NOALL: char *caller4(char *c : itype(_Ptr<char>)) : itype(_Ptr<char>) {
  if (0) {
    char *d = 1;
    return d;
  }
  // A bounds cast is used here because the cast insertion code sees `c` as a
  // fully unchecked pointer. It only sees the internal (unchecked) constraint
  // variable, so it doesn't know that the external component is checked. It'd
  // be nice if this used the same casting style as the other cases, but it
  // works as is, and I don't want to waste time when this is just a workaround
  // for a CheckedC clang bug.
  fn4(c);
  //CHECK: fn4(_Assume_bounds_cast<_Nt_array_ptr<char>>(c, bounds(unknown)));
  return c;
}
