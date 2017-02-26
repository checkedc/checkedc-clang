// Tests for Code Generation with Checked C Extension
// This makes sure we're generating something sensible for _Dynamic_check
// invocations.
//
// RUN: %clang_cc1 -fcheckedc-extension -emit-llvm -verify %s -o - | FileCheck %s


// Function with single dynamic check
void f1(void) {
  // CHECK: void @f1()
  _Dynamic_check(0); // expected-warning {{dynamic check will always fail}}

  // Branch
  // CHECK: br i1
  // CHECK-SAME: label %_Dynamic_check.succeeded
  // CHECK-SAME: label %_Dynamic_check.failed

  // Ordering of Basic Blocks
  // CHECK: {{^}}_Dynamic_check.succeeded
  // CHECK: ret void

  // Contents of Fail Basic Block
  // CHECK: {{^}}_Dynamic_check.failed
  // CHECK-NEXT: call void @llvm.trap()
  // CHECK-NEXT: unreachable
}

// Function with two dynamic checks
void f2(int i) {
  // CHECK: void @f2(i32 %i)

  _Dynamic_check(i != 3);
  // CHECK: icmp ne
  // CHECK: br i1
  // CHECK: {{^}}_Dynamic_check.succeeded

  _Dynamic_check(i < 50);
  // CHECK: icmp slt
  // CHECK: br i1
  // CHECK: {{^}}_Dynamic_check.succeeded
  // CHECK: ret void

  // CHECK: {{^}}_Dynamic_check.failed
  // CHECK: call void @llvm.trap()
  // CHECK: {{^}}_Dynamic_check.failed
  // CHECK: call void @llvm.trap()
}

void f3(void) {
  // CHECK: void @f3()
  _Dynamic_check(0 == 0);
  // CHECK-NOT: br i1
  // CHECK-NOT: _Dynamic_check
  // CHECK: ret void
  // CHECK-NOT: call void @llvm.trap()
}