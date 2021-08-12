// RUN: cd %S
// RUN: 3c -use-malloc=my_malloc -alltypes -addcr -output-dir=%t.checked/base_subdir -warn-all-root-cause %s -- -Xclang -verify=unwritable-expected -Wno-everything

#include "../root_cause.c"
