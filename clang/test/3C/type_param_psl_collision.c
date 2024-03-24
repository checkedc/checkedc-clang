// Apparently, two function calls expanded from the same macro call can have
// colliding PersistentSourceLocs in the TypeParamBindings map
// (https://github.com/correctcomputation/checkedc-clang/issues/568). One
// symptom of the collision was that 3C would crash if one of the function names
// was in parentheses. Test that we've fixed this crash. The underlying problem
// described in the issue is not yet fixed.

// We only care that this doesn't crash.
// RUN: 3c -base-dir=%S %s --

_Itype_for_any(T) void *fiddle(void *p : itype(_Ptr<T>)) : itype(_Ptr<T>) {
  return p;
}
void foo() {}

void bar() {
  char y;
  #define TWO_CALLS fiddle<char>(&y); (foo)();
  TWO_CALLS
}
