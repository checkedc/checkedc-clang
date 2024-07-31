// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- -Wno-error=int-conversion | %clang -c -Wno-error=int-conversion -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s -- -Wno-error=int-conversion
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/dont_add_prototype.c -- -Wno-error=int-conversion | diff %t.checked/dont_add_prototype.c -

// Don't add a void prototype on functions if we don't need to. This can
// potentially cause rewriting errors when attempting to rewrite functions
// defined outside the basedir or defined by a macro.

// This function does not need to be rewritten. Its return type is unchecked
// after solving. It should not be rewritten to use a void prototype in order
// to avoid any potential rewritting issues.
void *test0() { return 1; }
//CHECK: void *test0() { return 1; }

// Trying to add a prototype in these examples caused a rewriting error
// because the functions are defined by macros.

#define test_macro0 int *test_macro0()
test_macro0 {
//CHECK: test_macro0 {
  return 0;
}

#define test_macro1 test_macro1()
int *test_macro1 {
//CHECK: int *test_macro1 {
  return 0;
}

// Force conversion output.
int *a;
