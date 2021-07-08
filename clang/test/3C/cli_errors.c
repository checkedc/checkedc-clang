// Test that when given invalid command-line arguments, `3c` prints an
// appropriate error message without raising SIGABRT. (By default, `not` tests
// that the given command exits without raising a signal.)
//
// This C file itself is not used.

// RUN: rm -rf %t*
// RUN: not 3c --no-such-option 2>%t.unknown-option
// RUN: grep -q 'Unknown.*no-such-option' %t.unknown-option
// RUN: not 3c 2>%t.no-sources
// RUN: grep -q 'No source files specified.' %t.no-sources
