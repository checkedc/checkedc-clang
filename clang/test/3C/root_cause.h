// Test that root cause errors are reported correctly in include header files.
// Included by root_cause.c

void outside_of_main() {
  // expected-warning@+3 {{1 unchecked pointer: Cast from int to int *}}
  // unwritable-expected-warning@+2 {{0 unchecked pointers: Source code in non-writable file}}
  // unwritable-expected-warning@+1 {{0 unchecked pointers: Source code in non-writable file}}
  int *c = 1;
}
