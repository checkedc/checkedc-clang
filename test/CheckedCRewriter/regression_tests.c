// Tests for Checked C rewriter tool.
//
// Tests checked-c-convert tool for any regressions.
//
// RUN: checked-c-convert %s -- | FileCheck -match-full-lines %s
//

#include <stdlib_checked.h>

int main() {

  char *ptr1 = NULL;

  ptr1 = (char *) calloc(1, sizeof(char));

  return 0;
}
//CHECK: _Ptr<char> ptr1 =  NULL;
