// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=int-conversion | %clang -c -Wno-error=int-conversion -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s -- -Wno-error=int-conversion
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/k_and_r.c -- -Wno-error=int-conversion | diff %t.checked/k_and_r.c -

// clang-format doesn't seem to understand K&R function declarations and makes a
// mess.
// clang-format off

void test0(a)
  int *a;
{}
//CHECK: void test0(_Ptr<int> a)
//CHECK: _Checked {}

void test1(a)
  int *a;
{ a = 1; }
//CHECK: void test1(int *a : itype(_Ptr<int>))
//CHECK: { a = 1; }

void test2(a, b)
  int *a;
  char *b;
{}
//CHECK: void test2(_Ptr<int> a, _Ptr<char> b)
//CHECK: _Checked {}

int *test3(a)
  int *a;
{ return a; }
//CHECK: _Ptr<int> test3(_Ptr<int> a)
//CHECK: _Checked { return a; }

void test4(a)
  int *a;
{
  for (int i = 0; i < 10; i++) {
    a[i] = i;
  }
}
//CHECK_NOALL: void test4(int *a : itype(_Ptr<int>))
//CHECK_ALL: void test4(_Array_ptr<int> a : count(10))
//CHECK_ALL: _Checked {

void test5(a, b, c, d, e, f, g, h)
  int *a;
  char *b, c;
  char d, *e;
  int f, *g, h;
{}
//CHECK: void test5(_Ptr<int> a, _Ptr<char> b, char c, char d, _Ptr<char> e, int f, _Ptr<int> g, int h)
//CHECK: _Checked {}

// bc test case

struct yy_buffer_state {};
typedef struct yy_buffer_state *YY_BUFFER_STATE;
#ifdef YY_USE_PROTOS
void yy_init_buffer(YY_BUFFER_STATE b, int *file)
#else
void yy_init_buffer(b, file)
YY_BUFFER_STATE b;
int *file;
//CHECK: void yy_init_buffer(YY_BUFFER_STATE b, _Ptr<int> file)
#endif
{}

// Zlib test case

typedef int deflate_state;
typedef int ct_data;
void compress_block(s, ltree, dtree)
    deflate_state *s;
    const ct_data *ltree;
    const ct_data *dtree;
//CHECK: void compress_block(_Ptr<deflate_state> s, _Ptr<const ct_data> ltree, _Ptr<const ct_data> dtree)
{}

// clang-format on
