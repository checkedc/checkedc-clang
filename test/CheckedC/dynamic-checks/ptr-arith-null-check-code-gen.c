// RUN: %clang_cc1 %s -emit-llvm -o - | FileCheck %s

#include <stdchecked.h>
#include <stddef.h>

array_ptr<char> p : count(3) = "abc";
array_ptr<char> q : count(3) = "abc";
// NOCHECK-NOT: _Dynamic_check.non_null

void f1() {
// CHECK-LABEL: f1
// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** @p
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG0]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  p++;
}

void f2() {
// CHECK-LABEL: f2
// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** @p
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG0]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  ++p;
}

void f3() {
// CHECK-LABEL: f3
// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** @p
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG0]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  p--;
}

void f4() {
// CHECK-LABEL: f4
// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** @p
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG0]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  --p;
}

void f5() {
// CHECK-LABEL: f5
// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** @p
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG0]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  p += 5;
}

void f6() {
// CHECK-LABEL: f6
// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** @p
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG0]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  p -= 10;
}

void f7() {
// CHECK-LABEL: f7
// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** @p
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG0]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  p = 2 + p;
}

void f8() {
// CHECK-LABEL: f8
// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** @p
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG0]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  p = p - 100;
}

void f9() {
// CHECK-LABEL: f9
// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** @p
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG0]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  p[1]++;
}

void f10() {
// CHECK-LABEL: f10
// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** @p
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG0]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]

 // CHECK:      [[REG1:%[a-zA-Z0-9.]*]] = load i8*, i8** @p
// CHECK:      [[REG2:%[a-zA-Z0-9.]*]] = load i8*, i8** @p
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG2]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  p[1] = p[2] + (*p)++;
}

void f11(array_ptr<char> a);
void f12() {
// CHECK-LABEL: f12
// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** @p
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG0]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  f11(++p);
}

void f13() {
// CHECK-LABEL: f13
// CHECK:      [[REG0:%[a-zA-Z0-9.]*]] = load i8*, i8** @p
// CHECK:      [[REG1:%[a-zA-Z0-9.]*]] = load i8*, i8** @q
// CHECK-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[REG1]], null
// CHECK-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %[[LAB_DYFAIL:_Dynamic_check.failed[a-zA-Z0-9.]*]]
  p[q[1]++]++;
}
