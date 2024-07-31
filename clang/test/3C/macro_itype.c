// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- -Wno-error=int-conversion | %clang -c -Wno-error=int-conversion -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s -- -Wno-error=int-conversion
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/macro_itype.c -- -Wno-error=int-conversion | diff %t.checked/macro_itype.c -

// Example encountered while converting libjpeg. This triggered an assertion
// fail because the ItypeStr extracted from the source was empty.
#define macro0 int *a : itype(_Ptr<int>);
struct s { macro0 };
//CHECK: struct s { macro0 };

// Example from issue correctcomputation/checkedc-clang#594.
#define PARAM_DECL_WITH_ITYPE int *p : itype(_Ptr<int>)
void foo(PARAM_DECL_WITH_ITYPE);
//CHECK: void foo(PARAM_DECL_WITH_ITYPE);

// Just removing the assertion that failed on the above example caused this to
// rewrite incorrectly. The ItypeStr would be left empty, so first parameter
// would be rewritten to `int *b` even though the rewriter intended to give it
// an itype. If the parameter was then passed a checked pointer, there would be
// a Checked C compiler error. Ideally, 3C wouldn't need to change the
// declaration of `b` at all (see issue correctcomputation/checkedc-clang#694).
#define macro1 : itype(_Ptr<int>)
void fn(int *b macro1, int *c) {
//CHECK: void fn(int *b : itype(_Ptr<int>), _Ptr<int> c) {
  b = 1;
}
void caller() {
  int *e;
  //CHECK: _Ptr<int> e = ((void *)0);
  fn(e, 0);
}
