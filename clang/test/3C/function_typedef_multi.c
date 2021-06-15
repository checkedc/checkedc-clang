// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checked %S/function_typedef_multi.c %S/function_typedef_multi.h --
// RUN: test ! -f %t.checked/function_typedef_multi.h -a ! -f %t.checked/function_typedef_multi.c

// Test for the two file case in issue #430
// This test caused an assertion to fail prior to PR #436
// This test uses function_typedef_multi.h. The header is deliberately not
// included in this file. Including it prevented the assertion fail even
// without the changes in PR #436.

int foo(int a, int b[1]) { return 0; }
