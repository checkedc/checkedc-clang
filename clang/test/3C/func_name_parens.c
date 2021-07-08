// Test that 3C doesn't crash on a function call with parentheses around the
// function name
// (https://github.com/correctcomputation/checkedc-clang/issues/543).

// We only care that this doesn't crash.
// RUN: 3c -base-dir=%S %s --

void foo() {}

void bar() {
  (foo)();
}
