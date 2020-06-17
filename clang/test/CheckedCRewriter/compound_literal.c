// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: cconv-standalone -alltypes %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

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
  // CHECK: _Ptr<int> faz = (int*)0;
  faz = (&(struct b){&d, (int*) 1})->a;
  int *fuz = (int*)0;
  // CHECK: int *fuz = (int*)0;
  fuz = (&(struct b){&d, &d})->b;

  int *f = (int*) 0;
  // CHECK: _Array_ptr<int> f = (int*) 0;
  ((&(struct c){f})->a)++;
}
