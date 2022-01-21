// RUN: cd %S
// RUN: 3c -use-malloc=my_malloc -alltypes -addcr -output-dir=%t.checked/base_subdir -warn-all-root-cause %s -- -Xclang -verify=unwritable-expected -Wno-everything
// RUN: 3c -use-malloc=my_malloc -alltypes -addcr -output-dir=%t.checked/base_subdir -warn-root-cause %s -- -Xclang -verify=not-all-unwritable-expected -Wno-everything
// not-all-unwritable-expected-no-diagnostics

#include "../root_cause.c"
