// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone --addcr --alltypes %s -- | %clang_cc1  -fcheckedc-extension -x c -
//
// RUN: cconv-standalone -alltypes -output-postfix=checked %s
// RUN: cconv-standalone -alltypes %S/ptrtoconstarr.checked.c -- | count 0
// RUN: rm %S/ptrtoconstarr.checked.c

extern _Unchecked unsigned long strlen(const char * restrict src : itype(restrict _Nt_array_ptr<const char>));

int bar(void) { 
  //CHECK_ALL: int bar(void) _Checked {
  //CHECK_NOALL: int bar(void) {
  int local[10];
  //CHECK_ALL: int local _Checked[10];
  //CHECK_NOALL: int local[10];
  int (*coef)[10] = &local;
  //CHECK_ALL: _Ptr<int _Checked[10]> coef = &local;
  //CHECK_NOALL: _Ptr<int *> coef = &local;

  //int two[5][10];
  //DONTCHECK_ALL: int two _Checked[5] _Checked[10];
  //int (*two_ptr)[5][10];

  return (*coef)[1];
}

struct ex {
  int a;
  int *ptr;
  int (*ca)[10];
};
//CHECK: _Ptr<int> ptr;
//CHECK_ALL: _Ptr<int _Checked[10]> ca;
//CHECK_NOALL: _Ptr<int *> ca;

int foo(void) {
  //CHECK_ALL: int foo(void) _Checked {
  //CHECK_NOALL: int foo(void) {
  int local[10];
  //CHECK_ALL: int local _Checked[10];
  //CHECK_NOALL: int local[10];
  int y = 2;
  struct ex e = { 3, &y, &local };
  y += (*e.ca)[3];
  return *e.ptr;
}

int baz(void) {
  //CHECK_ALL: int baz(void) _Checked {
  //CHECK_NOALL: int baz(void) {
  char local[5] = "test";
  //CHECK_ALL: char local _Nt_checked[5] =  "test";
  //CHECK_NOALL: char local[5] = "test";
  char (*ptr)[5] = &local;
  //CHECK_ALL: _Ptr<char _Nt_checked[5]> ptr = &local;
  //CHECK_NOALL: _Ptr<char *> ptr = &local;

  return strlen(*ptr);
}

struct pair {
  int fst;
  int snd;
};

int sum10pairs(struct pair (*pairs)[10]) {
  //CHECK_ALL: int sum10pairs(_Ptr<struct pair _Checked[10]> pairs) _Checked {
  //CHECK_NOALL: int sum10pairs(_Ptr<struct pair *> pairs) {
  int sum = 0;

  for(int i = 0; i < 10; i++) {
    struct pair this = (*pairs)[i];
    sum += this.fst + this.snd;
  }

  return sum;
}

typedef int (*compl)[5];

int example(void) { 
  int local[5] = { 0 };
  //CHECK_ALL: int local _Checked[5] = { 0 };
  //CHECK_NOALL int local[5] = { 0 };
  compl t = &local;
  //CHECK_ALL: _Ptr<int _Checked[5]> t = &local;
  //CHECK_NOALL _Ptr<int *> t = &local;
  return (*t)[2];
}





