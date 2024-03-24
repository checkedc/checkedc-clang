// Test that 3C's stderr is empty by default on a successful run to prevent a
// recurrence of an issue like
// https://github.com/correctcomputation/checkedc-clang/issues/478 .

// Apparently `count 0` passes if the input contains a single line without a
// trailing newline character, as it did in the issue linked above. (This
// behavior may be considered a bug in `count`.) So we compare the stderr to a
// manually created empty file instead.

// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S %s -- 2>%t.stderr
// RUN: touch %t.empty
// RUN: diff %t.empty %t.stderr

int *p;
