// RUN: %clang_cc1 -fcheckedc-extension %s -emit-llvm -o - | FileCheck %s

_Array_ptr<int> ga1 : count(1) = 0;
_Array_ptr<int> ga3 : count(3) = 0;

int gca1 _Checked[1] = { 1 };
int gca3 _Checked[3] = { 0, 1, 2 };

// CHECK: define void @func(
void func(_Array_ptr<int> lp1 : count(1), _Array_ptr<int> lp3 : count(3)) {
  
  int x1 = ga1[0];
  int x2 = ga3[0];
  int x3 = ga3[2];
  int x4 = *ga1;
  int x5 = *ga3;

  int y1 = lp1[0];
  int y2 = lp3[0];
  int y3 = lp3[0];
  int y4 = *lp1;
  int y5 = *lp3;

  // CHECK: ret void

  // We should have 10 bounds checks
  // CHECK: call void @llvm.trap
  // CHECK: call void @llvm.trap
  // CHECK: call void @llvm.trap
  // CHECK: call void @llvm.trap
  // CHECK: call void @llvm.trap
  // CHECK: call void @llvm.trap
  // CHECK: call void @llvm.trap
  // CHECK: call void @llvm.trap
  // CHECK: call void @llvm.trap
  // CHECK: call void @llvm.trap
}