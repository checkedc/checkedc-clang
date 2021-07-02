// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -itypes-for-extern -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -itypes-for-extern -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -itypes-for-extern -alltypes -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -itypes-for-extern -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -itypes-for-extern -alltypes %t.checked/itypes_for_extern.c -- | diff %t.checked/itypes_for_extern.c -

// Simplest test case: a would normally get a checked type, but is given an
// itype because of the flag.
void foo(int *a) {}
//CHECK: void foo(int *a : itype(_Ptr<int>)) _Checked {}

// Since static function can't be included in other translation units, these
// can stay fully checked.
static void static_foo(int *a) {}
//CHECK: static void static_foo(_Ptr<int> a) _Checked {}

// Don't give a function an itype if it wouldn't normally be checked
void undef_foo(int *a);
//CHECK: void undef_foo(int *a);

int *bar() { return 0; }
//CHECK: int *bar(void) : itype(_Ptr<int>) _Checked { return 0; }

int *baz(int *a, int len, int *b) {
//CHECK_ALL: int *baz(int *a : itype(_Array_ptr<int>) count(len), int len, int *b : itype(_Ptr<int>)) : itype(_Array_ptr<int>) count(len) _Checked {
//CHECK_NOALL: int *baz(int *a : itype(_Ptr<int>), int len, int *b : itype(_Ptr<int>)) : itype(_Ptr<int>) {
  for (int i = 0; i < len; i++)
    a[i];
  return a;
}

// FIXME: The space between after the first star is needed for the idempotence
//        test to pass. If there isn't a space there, 3c will add one when
//        re-run on its original output. This should be fixed ideally in two
//        ways.  First, the space shouldn't be added when not present in the
//        original source, and second, the second conversion should not rewrite
//        the declaration.
void buz(int * (*f)(int *, int *)) {}
//CHECK: void buz(int * ((*f)(int *, int *)) : itype(_Ptr<_Ptr<int> (_Ptr<int>, _Ptr<int>)>)) _Checked {}

typedef int * int_star;
void typedef_test(int_star p) {}
//CHECK: typedef int * int_star;
//CHECK: void typedef_test(int_star p : itype(_Ptr<int>)) _Checked {}

typedef void (*fn)(int *);
void fn_typedef_test(fn f) {}
//CHECK: typedef void (*fn)(int *);
//CHECK: void fn_typedef_test(fn f : itype(_Ptr<void (_Ptr<int>)>)) _Checked {}

struct foo {
  int *a;
  void (*fn)(int *);
};
//CHECK: int *a : itype(_Ptr<int>);
//CHECK: void ((*fn)(int *)) : itype(_Ptr<void (_Ptr<int>)>);

int *glob = 0;
extern int *extern_glob = 0;
static int *static_glob = 0;
//CHECK: int *glob : itype(_Ptr<int>) = 0;
//CHECK: extern int *extern_glob : itype(_Ptr<int>) = 0;
//CHECK: static _Ptr<int> static_glob = 0;

void (*glob_fn)(int *) = 0;
extern void (*extern_glob_fn)(int *) = 0;
static void (*static_glob_fn)(int *) = 0;
//CHECK: void ((*glob_fn)(int *)) : itype(_Ptr<void (_Ptr<int>)>) = 0;
//CHECK: extern void ((*extern_glob_fn)(int *)) : itype(_Ptr<void (_Ptr<int>)>) = 0;
//CHECK: static _Ptr<void (_Ptr<int>)> static_glob_fn = 0;

int_star typedef_glob = 0;
fn typedef_fn_glob = 0;
//CHECK: int_star typedef_glob : itype(_Ptr<int>) = 0;
//CHECK: fn typedef_fn_glob : itype(_Ptr<void (_Ptr<int>)>) = 0;

struct typedef_struct {
  int_star a;
  fn f;
};
//CHECK: int_star a : itype(_Ptr<int>);
//CHECK: fn f : itype(_Ptr<void (_Ptr<int>)>);

// Testing some cases where the itypes already exist. Earlier we made a change
// that lets itypes re-solve to checked types. That shouldn't happen with this
// flag. This is also covered by the idempotence check, but I wanted to make it
// explicit.

void has_itype0(int *a : itype(_Ptr<int>)) { a = 1; }
//CHECK: void has_itype0(int *a : itype(_Ptr<int>)) { a = 1; }

void has_itype1(int *a : itype(_Ptr<int>)) { a = 0; }
//CHECK: void has_itype1(int *a : itype(_Ptr<int>)) _Checked { a = 0; }
