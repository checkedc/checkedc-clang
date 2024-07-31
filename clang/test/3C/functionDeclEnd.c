// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- -Wno-error=int-conversion | %clang -c -Wno-error=int-conversion -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s -- -Wno-error=int-conversion
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/functionDeclEnd.c -- -Wno-error=int-conversion | diff %t.checked/functionDeclEnd.c -

// Tests for issue 392. When rewriting function prototypes sometimes code
// falling between the start of the definition and the end of the prototype
// could be deleted.
// NOTE: Tests in this file assume that code in the branch of the preprocessor
// directive that is not taken will not be rewritten. It might be desirable to
// eventually rewrite in both branches. See issue 374.

#define FOO

#ifdef FOO
void test0(int *a)
// CHECK: void test0(_Ptr<int> a)
#else
void test0(int *a)
#endif
// CHECK: #else
// CHECK: void test0(int *a)
// CHECK: #endif
{
  // CHECK: _Checked {
  return;
}

#ifdef FOO
int *test1()
// CHECK: _Ptr<int> test1(void)
#else
int *test1()
#endif
// CHECK: #else
// CHECK: int *test1()
// CHECK: #endif
{
  // CHECK: _Checked {
  return 0;
}

#ifdef FOO
int *test2()
// CHECK: int *test2(void) : itype(_Ptr<int>)
#else
int *test2()
#endif
// CHECK: #else
// CHECK: int *test2()
// CHECK: #endif
{
  // CHECK: {
  int *a = 1;
  return a;
}

// These test for rewriting with existing itype and bounds expression are
// particularly important because they break the simplest fix where
// getRParenLoc is always used.

#ifdef FOO
int *test3(int *a, int l)
    : itype(_Array_ptr<int>) count(l)
// CHECK: int *test3(_Ptr<int> a, int l) : itype(_Array_ptr<int>) count(l)
#else
int *test3(int *a, int l)
    : itype(_Array_ptr<int>) count(l)
#endif
// CHECK: #else
// CHECK: int *test3(int *a, int l)
// CHECK:     : itype(_Array_ptr<int>) count(l)
// CHECK: #endif
{
  // CHECK: {
  int *b = 1;
  return b;
}

#ifdef FOO
int *test4(int *a)
    : itype(_Ptr<int>)
// CHECK: int *test4(_Ptr<int> a) : itype(_Ptr<int>)
#else
int *test4(int *a)
    : itype(_Ptr<int>)
#endif
// CHECK: #else
// CHECK: int *test4(int *a)
// CHECK:     : itype(_Ptr<int>)
// CHECK: #endif
{
  // CHECK: {
  int *b = 1;
  return b;
}

#ifdef FOO
_Array_ptr<int> test5(int *a, int l)
    : count(l)
// CHECK: _Array_ptr<int> test5(_Ptr<int> a, int l) : count(l)
#else
_Array_ptr<int> test5(int *a, int l)
    : count(l)
#endif
// CHECK: #else
// CHECK: _Array_ptr<int> test5(int *a, int l)
// CHECK:     : count(l)
// CHECK: #endif
{
  // CHECK: _Checked {
  return 0;
}

void test6(int *a)
// A comment ( with parentheses ) that shouldn't be deleted
// CHECK: void test6(_Ptr<int> a)
// CHECK: // A comment ( with parentheses ) that shouldn't be deleted
{
  *a = 1;
}

#ifdef FOO
int *test7(int *a)
    : count(10)
//CHECK_NOALL: int *test7(int *a : itype(_Ptr<int>)) : count(10)
//CHECK_ALL: _Array_ptr<int> test7(_Array_ptr<int> a : count(10)) : count(10)
#else
int *test7(int *a)
    : count(10)
#endif
          ;
//CHECK: #else
//CHECK: int *test7(int *a)
//CHECK:     : count(10)
//CHECK: #endif
//CHECK: ;

int *test7(int *a) : count(10) {
  //CHECK_ALL: _Array_ptr<int> test7(_Array_ptr<int> a : count(10)) : count(10) _Checked {
  //CHECK_NOALL: int *test7(int *a : itype(_Ptr<int>)) : count(10) {
  for (int i = 0; i < 10; i++)
    a[i];
  return a;
}

// This test cast checks that the itype is overwritten even when it appears
// after the count. Unfortunately, the count and itype are reversed on
// rewriting. This isn't great since it's an unnecessary change to the code,
// but it is still valid.
#ifdef FOO
int *test8(int *a, int l)
    : count(l) itype(_Array_ptr<int>)
// CHECK: int *test8(_Ptr<int> a, int l) : itype(_Array_ptr<int>) count(l)
#else
int *test8(int *a, int l)
    : count(l) itype(_Array_ptr<int>)
#endif
// CHECK: #else
// CHECK: int *test8(int *a, int l)
// CHECK:     : count(l) itype(_Array_ptr<int>)
// CHECK: #endif
{
  // CHECK: {
  int *b = 1;
  return b;
}
