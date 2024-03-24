// RUN: rm -rf %t*

// Turn off color in the AST dump so that we can more easily match it
// (https://stackoverflow.com/questions/32447542/how-do-i-get-clang-to-dump-the-ast-without-color).
// RUN: 3c -base-dir=%S %s -- -Xclang -ast-dump -fno-color-diagnostics | FileCheck -match-full-lines %s

// RUN: 3c -base-dir=%S %s -- -Xclang -ast-list 2>%t.stderr
// RUN: grep 'The requested ProgramAction is not implemented by 3C' %t.stderr

int *p;
// CHECK: `-VarDecl {{.*}} p 'int *'
