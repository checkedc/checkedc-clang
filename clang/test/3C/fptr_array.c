// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- -Wno-error=int-conversion -w | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=int-conversion -w | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=int-conversion -w | %clang -c -Wno-error=int-conversion -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s -- -Wno-error=int-conversion -w
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/fptr_array.c -- -Wno-error=int-conversion -w | diff %t.checked/fptr_array.c -

// Test function pointers as elements of fixed size arrays.

void (*fs[2])(int *);
void (*f)(int *);
//CHECK_NOALL: void (*fs[2])(_Ptr<int>) = {((void *)0)};
//CHECK_NOALL: void (*f)(_Ptr<int>) = ((void *)0);
//CHECK_ALL: _Ptr<void (_Ptr<int>)> fs _Checked[2] = {((void *)0)};
//CHECK_ALL: _Ptr<void (_Ptr<int>)> f = ((void *)0);

void (*gs[2])(int *);
void g_impl(int *x) { x = 1; }
void (*g)(int *) = g_impl;
//CHECK_NOALL: void (*gs[2])(int * : itype(_Ptr<int>)) = {((void *)0)};
//CHECK_NOALL: void g_impl(int *x : itype(_Ptr<int>)) { x = 1; }
//CHECK_NOALL: void (*g)(int * : itype(_Ptr<int>)) = g_impl;

void (*hs[2])(void *);
void (*h)(void *);
//CHECK_NOALL: void (*hs[2])(void *);
//CHECK_NOALL: void (*h)(void *);
//CHECK_ALL: _Ptr<void (void *)> hs _Checked[2] = {((void *)0)};
//CHECK_ALL: _Ptr<void (void *)> h = ((void *)0);

int *(*is[2])(void);
int *(*i)(void);
//CHECK_NOALL: _Ptr<int> (*is[2])(void) = {((void *)0)};
//CHECK_NOALL: _Ptr<int> (*i)(void) = ((void *)0);
//CHECK_ALL: _Ptr<_Ptr<int> (void)> is _Checked[2] = {((void *)0)};
//CHECK_ALL: _Ptr<_Ptr<int> (void)> i = ((void *)0);

int *(**js[2])(void);
int *(**j)(void);
//CHECK_NOALL: _Ptr<int> (**js[2])(void) = {((void *)0)};
//CHECK_NOALL: _Ptr<int> (**j)(void) = ((void *)0);
//CHECK_ALL: _Ptr<_Ptr<_Ptr<int> (void)>> js _Checked[2] = {((void *)0)};
//CHECK_ALL: _Ptr<_Ptr<_Ptr<int> (void)>> j = ((void *)0);

int *(*ks[2][2])(void);
int *(*k)(void);
//CHECK_NOALL: _Ptr<int> (*ks[2][2])(void) = {((void *)0)};
//CHECK_NOALL: _Ptr<int> (*k)(void) = ((void *)0);
//CHECK_ALL: _Ptr<_Ptr<int> (void)> ks _Checked[2] _Checked[2] = {((void *)0)};
//CHECK_ALL: _Ptr<_Ptr<int> (void)> k = ((void *)0);

void (* const l)(int *);
//CHECK_NOALL: void (*const l)(_Ptr<int>) = ((void *)0);
//CHECK_ALL: const _Ptr<void (_Ptr<int>)> l = ((void *)0);

#define UNWRITABLE_MS_DECL void (*ms[2])(int *)
UNWRITABLE_MS_DECL;

void test(void){
  fs[1] = f;
  gs[1] = g;
  hs[1] = h;
  is[1] = i;
  js[1] = j;
  ks[1][1] = k;
  void (* const ls[1])(int *) = {l};
  //CHECK_NOALL: void (*const ls[1])(_Ptr<int>) = {l};
  //CHECK_ALL: const _Ptr<void (_Ptr<int>)> ls _Checked[1] = {l};

  void (*(*pms)[2])(int *) = &ms;
  //CHECK: _Ptr<void (*[2])(int *)> pms = &ms;

  void (*(*apms[1])[2])(int *) = {&ms};
  //CHECK_NOALL: void (*(*apms[1])[2])(int *) = {&ms};
  //CHECK_ALL: _Ptr<void (*[2])(int *)> apms _Checked[1] = {&ms};
}
