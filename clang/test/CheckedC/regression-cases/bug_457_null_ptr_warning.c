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
  myfunc1(a, 0);  // expected-warning {{cannot prove argument meets declared bounds}} \
                  // expected-note {{expected argument bounds}} \
                  // expected-note {{inferred bounds}}
  myfunc2(0, 0);
  return 0;
}
