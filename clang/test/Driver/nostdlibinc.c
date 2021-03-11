// RUN: %clang -target x86_64-unknown-unknown -nostdlibinc -ffreestanding -MM -MG %s | FileCheck -check-prefix=CHECK %s

#if !__has_include("stddef.h")
#error "expected to be able to find compiler builtin headers!"
#endif

#include<stdlib.h>
// CHECK: stdlib.h
