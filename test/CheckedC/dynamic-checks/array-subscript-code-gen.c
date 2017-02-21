// RUN: %clang_cc1 -fcheckedc-extension %s -emit-llvm -o - | FileCheck %s

int ga1 _Checked[1] = { 0 };
int ga3 _Checked[3] = { 0, 1, 2 };

_Array_ptr<int> gp1 : count(1) = ga1;
_Array_ptr<int> gp2 : bounds(&ga1, &ga1 + 1) = ga1;

_Array_ptr<int> gp3 : count(3) = ga3;
_Array_ptr<int> gp4 : bounds(&ga3[0], &ga3[2] + 1) = ga1;

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

  // CHECK: call void @llvm.trap
  // CHECK: call void @llvm.trap
  // CHECK: call void @llvm.trap
  // CHECK: call void @llvm.trap

}