// Test that rewriting of a typedef involving a struct is blocked only if the
// full struct declaration is inside the typedef, not later in the file.

// RUN: 3c -base-dir=%S %s -- | FileCheck -match-full-lines %s

typedef struct sfoo *pfoo;
// CHECK: typedef _Ptr<struct sfoo> pfoo;

typedef struct sfoo {
  int x;
} foo;
