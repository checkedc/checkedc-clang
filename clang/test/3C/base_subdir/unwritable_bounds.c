// RUN: rm -rf %t*
// RUN: cd %S
// RUN: 3c -alltypes -addcr -output-dir=%t.checked/base_subdir %s --

// The functions called from this file would normally have bounds inferred if
// they were declared in a writable file. The file is not writable, so the new
// bounds must not be inferred. This possibility of this happening existed
// before 3C started inferring itypes on undefined functions, but it became a
// significant issue and was noticed with the introduction of this feature.

#include "../unwritable_bounds.h"
void test_functions() {
  {
    char s[0];
    unwrite0(s);
  }
  {
    char s[0];
    unwrite1(s);
  }
  {
    char s[0];
    unwrite2(s);
  }
  {
    char s[0];
    unwrite3(s);
  }
  {
    char s[0];
    unwrite4(s);
  }
  {
    char s[0];
    unwrite5(s);
  }
  {
    char s[0];
    unwrite6(s);
  }
}

void test_struct() {
  // There is a code path for variable declarations and a different path for
  // uses of a variable. The initializer is required to test the declaration
  // path.
  struct arr_struct a = {};
  for (int i = 0; i < a.n; i++)
    a.arr[i];

  // I don't quite understand why this was a problem, but it caused an
  // assertion to fail after apply the fix for the first struct test
  // case.
  union e {
    float d
  };
  int i = struct_ret()->c;
  union e f;
  f.d;
}

void test_glob() {
  for (int i = 0; i < 10; i++)
    glob0[i];

  for (int i = 0; i < 10; i++)
    glob1[i];

  for (int i = 0; i < 10; i++)
    glob2[i];

  for (int i = 0; i < 10; i++)
    glob3[i];
}
