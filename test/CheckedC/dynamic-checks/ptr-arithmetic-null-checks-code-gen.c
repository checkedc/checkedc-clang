// RUN: %clang_cc1 %s -emit-llvm -o - | FileCheck %s
// RUN: %clang_cc1 %s -emit-llvm -fno-checkedc-null-ptr-checks -o - | FileCheck %s --check-prefix NOCHECK

#include <stdchecked.h>
#include <stddef.h>

void f1() {
// CHECK-LABEL: f1
  array_ptr<char> p = "abc";

// NOCHECK-NOT: _Dynamic_check

// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** %p, align 8
// CHECK-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i8*, i8** %p, align 8
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG1]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  p++;

// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** %p, align 8
// CHECK-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i8*, i8** %p, align 8
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG1]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  ++p;

// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** %p, align 8
// CHECK-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i8*, i8** %p, align 8
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG1]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  p--;

// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** %p, align 8
// CHECK-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i8*, i8** %p, align 8
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG1]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  --p;

// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** %p, align 8
// CHECK-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i8*, i8** %p, align 8
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG1]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  p += 5;

// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** %p, align 8
// CHECK-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i8*, i8** %p, align 8
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG1]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  p -= 10;

// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** %p, align 8
// CHECK-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i8*, i8** %p, align 8
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG1]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  p = 2 + p;

// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** %p, align 8
// CHECK-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i8*, i8** %p, align 8
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG1]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  p = p - 100;
}
