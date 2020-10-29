// RUN: cconv-standalone -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -alltypes -output-postfix=checked %s
// RUN: cconv-standalone -alltypes %S/partial_checked_arr.checked.c -- | count 0
// RUN: rm %S/partial_checked_arr.checked.c

int strcmp(const char *src1 : itype(_Nt_array_ptr<const char>),
           const char *src2 : itype(_Nt_array_ptr<const char>));

void test0() {
  _Ptr<int *> a = 0;
  // CHECK_ALL: _Ptr<_Array_ptr<int>> a = 0;
  // CHECK_NOALL: _Ptr<int *> a = 0;
  (*a)[0] = 1;
  (*a)[1] = 1;

  _Array_ptr<int *> b = 0;
  // CHECK: _Array_ptr<_Ptr<int>> b = 0;

  _Array_ptr<int *> c = 0;
  // CHECK_ALL: _Array_ptr<_Ptr<int>> c : count(10) = 0;
  // CHECK_NOALL: _Array_ptr<_Ptr<int>> c = 0;
  for (int i = 0; i < 10; i++) {
    c[i] = 0;
  }

  _Ptr<_Array_ptr<char **>> d = 0;
  // CHECK_ALL: _Ptr<_Array_ptr<_Array_ptr<_Ptr<char>>>> d = 0;
  // CHECK_NOALL: _Ptr<_Array_ptr<char **>> d = 0;
  (**d)[0] = 0;

  _Nt_array_ptr<char **> e;
  // CHECK: _Nt_array_ptr<_Ptr<_Ptr<char>>> e = ((void *)0);
  
  
  _Ptr<char *> f;
  _Ptr<char *> g;
  // CHECK_ALL: _Ptr<_Nt_array_ptr<char>> f = ((void *)0);
  // CHECK_ALL: _Ptr<_Nt_array_ptr<char>> g = ((void *)0);
  // CHECK_NOALL: _Ptr<char *> f;
  // CHECK_NOALL: _Ptr<char *> g;

  strcmp(*f, *g);
}

_Ptr<char *> test1(_Ptr<char *> d) {
// CHECK_ALL: _Ptr<_Array_ptr<char>> test1(_Ptr<_Array_ptr<char>> d) _Checked {
// CHECK_NOALL: _Ptr<char *> test1(_Ptr<char *> d) {
  (*d)[0] = 0;
  return d;
}
