// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL" %s
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/keep_array_size_expr.c -- | diff %t.checked/keep_array_size_expr.c -

int a[1];
int b[1 + 1];
int c[1 /*test*/];
#define FOO 10
int d[FOO];
int e[FOO + FOO];
int f[sizeof(int)];
//CHECK_ALL: int a _Checked[1];
//CHECK_ALL: int b _Checked[1 + 1];
//CHECK_ALL: int c _Checked[1 /*test*/];
//CHECK_ALL: #define FOO 10
//CHECK_ALL: int d _Checked[FOO];
//CHECK_ALL: int e _Checked[FOO + FOO];
//CHECK_ALL: int f _Checked[sizeof(int)];

#define BAR(x) (x + 10)
int g[BAR(10)];
//CHECK_ALL: #define BAR(x) (x + 10)
//CHECK_ALL: int g _Checked[BAR(10)];

#define BAZ(x) x##1
int h[BAZ(1)];
//CHECK_ALL: #define BAZ(x) x##1
//CHECK_ALL: int h _Checked[BAZ(1)];

int i[];
//CHECK_ALL: int i _Checked[];

int j[10][10];
int k[1 + 1][FOO + FOO];
int l[FOO][BAR(1)];
int m[/*test*/10][BAR(1)][1+1][sizeof(int)][FOO];
//CHECK_ALL: int j _Checked[10] _Checked[10];
//CHECK_ALL: int k _Checked[1 + 1] _Checked[FOO + FOO];
//CHECK_ALL: int l _Checked[FOO] _Checked[BAR(1)];
//CHECK_ALL: int m _Checked[/*test*/10] _Checked[BAR(1)] _Checked[1+1] _Checked[sizeof(int)] _Checked[FOO];


int *n[FOO];
int **o[BAR(1+1)];
//CHECK_ALL: _Ptr<int> n _Checked[FOO] = {((void *)0)};
//CHECK_ALL: _Ptr<_Ptr<int>> o _Checked[BAR(1+1)] = {((void *)0)};


int (*p)[FOO];
int (*q[/*test*/10/*test*/])[FOO];
int (*r[/*test*/10/*test*/][BAR(1)])[FOO][BAZ(1)];
//CHECK_ALL: _Ptr<int _Checked[FOO]> p = ((void *)0);
//CHECK_ALL: _Ptr<int _Checked[FOO]> q _Checked[/*test*/10/*test*/] = {((void *)0)};
//CHECK_ALL: _Ptr<int _Checked[FOO] _Checked[BAZ(1)]> r _Checked[/*test*/10/*test*/] _Checked[BAR(1)] = {((void *)0)};

int s <: 10 :>;
//CHECK_ALL: int s _Checked <: 10 :>;
