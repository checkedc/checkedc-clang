//
// These is a regression test case for
// https://github.com/Microsoft/checkedc-clang/issues/457
//
// This test checks that bounds declaration checking does not
// introduce a spurious warning about pointer arithmetic on a null
// value.
// RUN: %clang -cc1 -verify -Wextra -fcheckedc-extension %s

#pragma CHECKED_SCOPE ON

static void myfunc1(_Array_ptr<void> data : bounds(other_data, other_data + 5),
                    _Array_ptr<char> other_data) {
  (void)data;
}

static void myfunc2(_Array_ptr<int> data : count(len), int len) {
  (void)data;
}

int main(void) {
  int a _Checked[5];
  myfunc1(a, 0);  // expected-error {{it is not possible to prove argument meets declared bounds for 1st parameter}} \
                  // expected-note {{the inferred bounds use the value of the variable 'a', and there is no relational information between 'a' and any of the variables used by the expected argument bounds}} \
                  // expected-note {{(expanded) expected argument bounds are 'bounds((_Array_ptr<char>)0, (_Array_ptr<char>)0 + 5)'}} \
                  // expected-note {{(expanded) inferred bounds are 'bounds(a, a + 5)'}}
  myfunc2(0, 0);
  return 0;
}
