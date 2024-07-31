// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- -Wno-error=int-conversion | %clang -c -Wno-error=int-conversion -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s -- -Wno-error=int-conversion
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/liberal_itypes_ptrptr.c -- -Wno-error=int-conversion | diff %t.checked/liberal_itypes_ptrptr.c -

void ptrptr(int **g) {}
//CHECK: void ptrptr(_Ptr<int *> g) {}
void ptrptr_caller() {
  int **a = 1;
  ptrptr(a);
  //CHECK: int **a = 1;
  //CHECK: ptrptr(_Assume_bounds_cast<_Ptr<int *>>(a));

  int **b = 0;
  *b = 1;
  ptrptr(b);
  //CHECK: _Ptr<int *> b = 0;
  //CHECK: ptrptr(b);

  int **c = 0;
  ptrptr(c);
  //CHECK: _Ptr<int *> c = 0;
  //CHECK: ptrptr(c);

  int *d;
  ptrptr(&d);
  //CHECK: int *d;
  //CHECK: ptrptr(&d);

  int ***e;
  ptrptr(*e);
  //CHECK: _Ptr<_Ptr<int *>> e = ((void *)0);
  //CHECK: ptrptr(*e);

  int ***f = 1;
  ptrptr(*f);
  //CHECK: int ***f = 1;
  //CHECK: ptrptr(_Assume_bounds_cast<_Ptr<int *>>(*f));

  int ***g;
  *g = 1;
  ptrptr(*g);
  //CHECK: _Ptr<int **> g = ((void *)0);
  //CHECK: ptrptr(_Assume_bounds_cast<_Ptr<int *>>(*g));
}

void ptrptr_wild(int **y);
//CHECK: void ptrptr_wild(int **y);
int **ptrptr_ret() { return 0; }
//CHECK: _Ptr<int *> ptrptr_ret(void) { return 0; }
void ptrptr_wild_caller() {
  ptrptr_wild(ptrptr_ret());
  //CHECK: ptrptr_wild(((int **)ptrptr_ret()));
}

void ptrptr_itype(int **a) {
  //CHECK: void ptrptr_itype(int **a : itype(_Ptr<int *>)) {
  a = 1;

  int **b = a;
  //CHECK: int **b = a;

  int *c = *a;
  //CHECK: int *c = *a;
}

void ptrptr_itype_caller() {
  int **a;
  *a = 1;
  ptrptr_itype(a);
  //CHECK: _Ptr<int *> a = ((void *)0);
  //CHECK: ptrptr_itype(a);

  int **b = 1;
  ptrptr_itype(b);
  //CHECK: int **b = 1;
  //CHECK: ptrptr_itype(b);

  int **c;
  ptrptr_itype(c);
  //CHECK: _Ptr<int *> c = ((void *)0);
  //CHECK: ptrptr_itype(c);

  int ***d;
  ptrptr_itype(*d);
  //CHECK: _Ptr<_Ptr<int *>> d = ((void *)0);
  //CHECK: ptrptr_itype(*d);

  int *e;
  ptrptr_itype(&e);
  //CHECK: int *e;
  //CHECK: ptrptr_itype(&e);
}

int **ptrptr_ret_bad() { return 1; }
//CHECK: int **ptrptr_ret_bad(void) : itype(_Ptr<int *>) { return 1; }
void ptrptr_other() {
  int **a = ptrptr_ret_bad();
  //CHECK: int **a = ptrptr_ret_bad();
  a = 1;

  int **b = ptrptr_ret_bad();
  //CHECK: _Ptr<int *> b = ptrptr_ret_bad();
  *b = 1;

  int **c = ptrptr_ret_bad();
  //CHECK: _Ptr<int *> c = ptrptr_ret_bad();

  int *d = *ptrptr_ret_bad();
  //CHECK: int *d = *ptrptr_ret_bad();
}

int *nested_callee(char *k) { return 0; }
//CHECK: _Ptr<int> nested_callee(_Ptr<char> k) _Checked { return 0; }
int **nested_callee_ptrptr(char **k) { return 0; }
//CHECK: _Ptr<int *> nested_callee_ptrptr(_Ptr<char *> k) { return 0; }
void nested_caller(void) {
  //CHECK: void nested_caller(void) {
  char *a = 1;
  nested_callee(nested_callee(a));
  //CHECK: nested_callee(_Assume_bounds_cast<_Ptr<char>>(nested_callee(_Assume_bounds_cast<_Ptr<char>>(a))));

  nested_callee_ptrptr(nested_callee_ptrptr(1));
  //CHECK: nested_callee_ptrptr(_Assume_bounds_cast<_Ptr<char *>>(nested_callee_ptrptr(_Assume_bounds_cast<_Ptr<char *>>(1))));
}

void itype_defined_ptrptr(int **p : itype(_Ptr<_Ptr<int>>));
//CHECK: void itype_defined_ptrptr(int **p : itype(_Ptr<_Ptr<int>>));

void itype_defined_caller() {
  int **c = 1;
  itype_defined_ptrptr(c);
  //CHECK: int **c = 1;
  //CHECK: itype_defined_ptrptr(c);

  int **d;
  itype_defined_ptrptr(d);
  //CHECK: _Ptr<_Ptr<int>> d = ((void *)0);
  //CHECK: itype_defined_ptrptr(d);

  int **e;
  *e = 1;
  itype_defined_ptrptr(e);
  //CHECK: _Ptr<int *> e = ((void *)0);
  //CHECK: itype_defined_ptrptr(((int **)e));
}
