// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- -Wno-error=int-conversion | %clang -c -Wno-error=int-conversion -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s -- -Wno-error=int-conversion
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/defn_then_decl.c -- -Wno-error=int-conversion | diff %t.checked/defn_then_decl.c -

// Tests behavior when a function declaration appears after the definition for
// that function. A specific issue that existed caused constraints applied to
// the declaration (for example, when the declaration is in a macro) to not
// be correctly applied to the original definition.

void test0(int *p) {}
#define MYDECL void test0(int *p);
MYDECL

void test1(int *p) {}
//CHECK: void test1(_Ptr<int> p) _Checked {}
void test1(int *p);
//CHECK: void test1(_Ptr<int> p);

void test2(int *p);
//CHECK: void test2(_Ptr<int> p);
void test2(int *p) {}
//CHECK: void test2(_Ptr<int> p) _Checked {}
void test2(int *p);
//CHECK: void test2(_Ptr<int> p);

void test3(int *p) { p = 1; }
//CHECK: void test3(int *p : itype(_Ptr<int>)) { p = 1; }
void test3(int *p);
//CHECK: void test3(int *p : itype(_Ptr<int>));

void test4(int *p) {}
//CHECK: void test4(_Ptr<int> p) _Checked {}
void test4();
//CHECK: void test4(_Ptr<int> p);
