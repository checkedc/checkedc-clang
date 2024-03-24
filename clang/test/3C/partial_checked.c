// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/partial_checked.c -- | diff %t.checked/partial_checked.c -

void test0(_Ptr<int *> a) {}
// CHECK: void test0(_Ptr<_Ptr<int>> a) _Checked {}

void test1(_Ptr<int **> a) {}
// CHECK: void test1(_Ptr<_Ptr<_Ptr<int>>> a) _Checked {}

void test2(_Ptr<int **> a) {
  // CHECK: void test2(_Ptr<int **> a : itype(_Ptr<_Ptr<_Ptr<int>>>)) {
  **a = 1;
}

void test3(_Ptr<int *> a) {
  // CHECK: void test3(_Ptr<int *> a : itype(_Ptr<_Ptr<int>>)) {
  *a = 1;
}

void test4() {
  _Ptr<int *> a;
  // CHECK: _Ptr<_Ptr<int>> a = ((void *)0);

  _Ptr<int *> b = 0;
  // CHECK: _Ptr<int *> b = 0;
  *b = 1;

  _Ptr<_Ptr<int *>> c;
  // CHECK: _Ptr<_Ptr<_Ptr<int>>> c = ((void *)0);

  int *d _Checked[4];
  // CHECK: _Ptr<int> d _Checked[4] = {((void *)0)};

  _Ptr<int *> e _Checked[4];
  // CHECK: _Ptr<_Ptr<int>> e _Checked[4] = {((void *)0)};

  _Ptr<int *> f _Checked[4] = {0};
  // CHECK: _Ptr<int *> f _Checked[4] = {0};
  (*f[0]) = 1;

  _Ptr<int **> g, h, i _Checked[1];
  // CHECK: _Ptr<_Ptr<_Ptr<int>>> g = ((void *)0);
  // CHECK: _Ptr<_Ptr<int *>> h = ((void *)0);
  // CHECK: _Ptr<_Ptr<_Ptr<int>>> i _Checked[1] = {((void *)0)};
  **h = 1;

  _Ptr<void(int *)> j = 0;
  // CHECK: _Ptr<void (_Ptr<int>)> j = 0;

  _Ptr<int *(void)> k = 0;
  // CHECK: _Ptr<_Ptr<int> (void)> k = 0;

  _Ptr<int> (*l)(void) = 0;
  // CHECK: _Ptr<_Ptr<int> (void)> l = 0;

  _Ptr<int *(void)> m = 0, n = 0;
  // CHECK: _Ptr<_Ptr<int> (void)> m = 0;
  // CHECK: _Ptr<_Ptr<int> (void)> n = 0;
}

// Test the partial workaround in getDeclSourceRangeWithAnnotations for a
// compiler bug where DeclaratorDecl::getSourceRange gives the wrong answer for
// certain checked pointer types. Previously, if the variable had an
// initializer, 3C used the start of the initializer as the end location of the
// rewrite, which had the side effect of working around the bug for all
// variables with an initializer (such as `m` above). Any variable with an
// affected type and no initializer would trigger the bug; apparently we never
// noticed because 3C unnecessarily adds initializers to global variables
// (https://github.com/correctcomputation/checkedc-clang/issues/741). Now, for
// uniformity, 3C always uses DeclaratorDecl::getSourceRange to get the range
// excluding any initializer, so it needs a workaround specifically for the bug.
// See getDeclSourceRangeWithAnnotations for more information.
_Ptr<int *(void)> gm;
// CHECK: _Ptr<_Ptr<int> (void)> gm = ((void *)0);

void test5(_Ptr<int *> a, _Ptr<int *> b, _Ptr<_Ptr<int>> c, int **d) {
  // CHECK: void test5(_Ptr<_Ptr<int>> a, _Ptr<int *> b : itype(_Ptr<_Ptr<int>>), _Ptr<_Ptr<int>> c, _Ptr<_Ptr<int>> d) {
  *b = 1;
}

struct s0 {
  _Ptr<int *> a, b;
  _Ptr<_Ptr<int *>> c;
  _Ptr<_Ptr<int>> d;
  int *e _Checked[1];
  // CHECK: _Ptr<_Ptr<int>> a;
  // CHECK: _Ptr<_Ptr<int>> b;
  // CHECK: _Ptr<_Ptr<_Ptr<int>>> c;
  // CHECK: _Ptr<_Ptr<int>> d;
  // CHECK: _Ptr<int> e _Checked[1];
};

extern void thing(_Ptr<int *> a);
// CHECK: extern void thing(_Ptr<int *> a);

void test6() {
  _Ptr<int *> a = 0;
  // CHECK: _Ptr<int *> a = 0;
  thing(a);

  int **b = 0;
  // CHECK: _Ptr<int *> b = 0;
  thing(b);

  _Ptr<int **> c = 0;
  // CHECK: _Ptr<_Ptr<int *>> c = 0;
  thing(*c);
}
