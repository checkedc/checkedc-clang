// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- -Wno-error=implicit-int | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=implicit-int | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=implicit-int | %clang -c -Wno-error=implicit-int -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s -- -Wno-error=implicit-int
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/macroConcat.c -- -Wno-error=implicit-int | diff %t.checked/macroConcat.c -

// I suspect the original source locations might have been an important aspect
// of this test, so we shouldn't let clang-format change them. ~ Matt 2021-03-10
// clang-format off

#define c(g) void FOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO##g ()
c(BAR0); c(BAR1); c(BAR2); c(BAR3); c(BAR4); c(BAR5); c(BAR6); c(BAR7);
c(BAR8); c(BAR9); c(BAR10); c(BAR11); c(BAR12); c(BAR13); c(BAR14); c(BAR15);
c(BAR16); c(BAR17); c(BAR18); c(BAR19); c(BAR20); c(BAR21); c(BAR22); c(BAR23);
c(BAR24); c(BAR25); c(BAR26); c(BAR27); c(BAR28); c(BAR29); c(BAR30); c(BAR31);
c(BAR32); c(BAR33); c(BAR34); c(BAR35); c(BAR36); c(BAR37); c(BAR38);

#define u(d) void add##d(g) {}
// CHECK: #define u(d) void add##d(g) {}
u(2);
// CHECK: u(2);

int *a = 0;
// CHECK: _Ptr<int> a = 0;

// clang-format on
