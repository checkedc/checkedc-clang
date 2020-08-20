// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/compound_literal.checked.c -- | diff -w %S/compound_literal.checked.c -
// RUN: rm %S/compound_literal.checked.c

struct a {
  int *a;
	//CHECK: _Ptr<int> a;
  int *b;
	//CHECK: int *b;
};

struct b {
  int *a;
	//CHECK: _Ptr<int> a;
  int *b;
	//CHECK: int *b;

};

struct c {
  int *a;
	//CHECK_NOALL: int *a;
	//CHECK_ALL:   _Array_ptr<int> a;
};

void structs() {
  int c;
  struct a a0 = (struct a){&c, (int*) 1};
	//CHECK: struct a a0 = (struct a){&c, (int*) 1};
  int *e = a0.b;
	//CHECK: int *e = a0.b;

  int d;
  int *faz = (int*)0;
	//CHECK: _Ptr<int> faz =  (_Ptr<int> )0;
  faz = (&(struct b){&d, (int*) 1})->a;
	//CHECK: faz = (&(struct b){&d, (int*) 1})->a;
  int *fuz = (int*)0;
	//CHECK: int *fuz = (int*)0;
  fuz = (&(struct b){&d, &d})->b;

  int *f = (int*) 0;
	//CHECK_NOALL: int *f = (int*) 0;
	//CHECK_ALL:   _Array_ptr<int> f =  (_Array_ptr<int> ) 0;
  ((&(struct c){f})->a)++;
}

void lists() {
  int x;

  int a[1] = (int[1]){1};
	//CHECK_NOALL: int a[1] = (int[1]){1};
	//CHECK_ALL:   int a _Checked[1] =  (int  _Checked[1]){1};

  int *b[1] = (int*[1]){&x};
	//CHECK_NOALL: int *b[1] = (int*[1]){&x};
	//CHECK_ALL:   _Ptr<int> b _Checked[1] =  (_Ptr<int>  _Checked[1]){&x};

  int *c[1] = (int*[1]){&x};
	//CHECK_NOALL: int *c[1] = (int*[1]){&x};
	//CHECK_ALL:   _Ptr<int> c _Checked[1] =  (_Ptr<int>  _Checked[1]){&x};
  int *c0 = c[0];
	//CHECK_NOALL: int *c0 = c[0];
	//CHECK_ALL:   _Ptr<int> c0 =  c[0];

  int *d[2] = (int*[2]){&x, (int*)1};
	//CHECK_NOALL: int *d[2] = (int*[2]){&x, (int*)1};
	//CHECK_ALL:   int * d _Checked[2] =  (int *  _Checked[2]){&x, (int*)1};
  int *d0 = d[0];
	//CHECK: int *d0 = d[0];

  int *e = (int*[1]){&x}[0];
	//CHECK_NOALL: int *e = (int*[1]){&x}[0];
	//CHECK_ALL:   _Ptr<int> e =  (_Ptr<int>  _Checked[1]){&x}[0];

  int *f = (int*[1]){(int*)1}[0];
	//CHECK_NOALL: int *f = (int*[1]){(int*)1}[0];
	//CHECK_ALL:   int *f = (int *  _Checked[1]){(int*)1}[0];
}


struct d {
  int *a;
	//CHECK: _Ptr<int> a;
};

struct e {
  int *a;
	//CHECK: int *a;
};

void nested(int* x) {
	//CHECK: void nested(_Ptr<int> x) {
  struct d a[1] = (struct d[1]){(struct d){x}};
	//CHECK_NOALL: struct d a[1] = (struct d[1]){(struct d){x}};
	//CHECK_ALL:   struct d a _Checked[1] =  (struct d  _Checked[1]){(struct d){x}};

  struct e b[1] = (struct e[1]){(struct e){(int*)1}};
	//CHECK_NOALL: struct e b[1] = (struct e[1]){(struct e){(int*)1}};
	//CHECK_ALL:   struct e b _Checked[1] =  (struct e  _Checked[1]){(struct e){(int*)1}};
}

void silly(int *x) {
	//CHECK: void silly(_Ptr<int> x) {
  int *a = (int*){x};
	//CHECK: _Ptr<int> a =  (_Ptr<int> ){x};

  int *b = (int*){(int*) 1};
	//CHECK: int *b = (int*){(int*) 1};

  int c = (int){1};
}
