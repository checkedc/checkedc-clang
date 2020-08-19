// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: cconv-standalone -alltypes %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// XFAIL: *
// Fails dues to issue microsoft#870 but fixed by pull request microsft#899

struct a {
  int *a;
  int *b;
  // CHECK: _Ptr<int> a;
  // CHECK: int *b;
};

struct b {
  int *a;
  int *b;
  // CHECK: _Ptr<int> a;
  // CHECK: int *b;

};

struct c {
  int *a;
  // CHECK: _Array_ptr<int> a;
};

void structs() {
  int c;
  struct a a0 = (struct a){&c, (int*) 1};
  int *e = a0.b;
  // CHECK:  int *e = a0.b;

  int d;
  int *faz = (int*)0;
  // CHECK: _Ptr<int> faz = (_Ptr<int> )0;
  faz = (&(struct b){&d, (int*) 1})->a;
  int *fuz = (int*)0;
  // CHECK: int *fuz = (int*)0;
  fuz = (&(struct b){&d, &d})->b;

  int *f = (int*) 0;
  // CHECK: _Array_ptr<int> f = (_Array_ptr<int> ) 0;
  ((&(struct c){f})->a)++;
}

void lists() {
  int x;

  int a[1] = (int[1]){1};
  // CHECK: int a _Checked[1] = (int _Checked[1]){1};

  int *b[1] = (int*[1]){&x};
  // CHECK: _Ptr<int> b _Checked[1] = (_Ptr<int> _Checked[1]){&x};

  int *c[1] = (int*[1]){&x};
  // CHECK: _Ptr<int> c _Checked[1] = (_Ptr<int> _Checked[1]){&x};
  int *c0 = c[0];
  // CHECK: _Ptr<int> c0 = c[0];

  int *d[2] = (int*[2]){&x, (int*)1};
  // CHECK: int * d _Checked[2] = (int * _Checked[2]){&x, (int*)1};
  int *d0 = d[0];
  // CHECK: int *d0 = d[0];

  int *e = (int*[1]){&x}[0];
  // CHECK _Ptr<int> e = (_Ptr<int> _Checked[1]){&x}[0]

  int *f = (int*[1]){(int*)1}[0];
  // CHECK int *e = (int * _Checked[1]){&x}[0]
}


struct d {
  int *a;
  // CHECK: _Ptr<int> a;
};

struct e {
  int *a;
  // CHECK: int *a;
};

void nested(int* x) {
  struct d a[1] = (struct d[1]){(struct d){x}};
  // CHECK: struct d a _Checked[1] = (struct d _Checked[1]){(struct d){x}};

  struct e b[1] = (struct e[1]){(struct e){(int*)1}};
  // CHECK: struct e b _Checked[1] = (struct e _Checked[1]){(struct e){(int*)1}};
}

void silly(int *x) {
  int *a = (int*){x};
  // CHECK: _Ptr<int> a = (_Ptr<int> ){x};

  int *b = (int*){(int*) 1};
  // CHECK: int *b = (int*){(int*) 1};

  int c = (int){1};
  // CHECK: int c = (int){1};
}
