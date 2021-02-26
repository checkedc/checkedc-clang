; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt < %s -mtriple aarch64-- -consthoist -S | FileCheck %s

; This used to trigger an assertion failure:
;
;    ../lib/Transforms/Scalar/ConstantHoisting.cpp:779: void llvm::ConstantHoistingPass::emitBaseConstants(llvm::Instruction *, llvm::Constant *, llvm::Type *, const llvm::consthoist::ConstantUser &): Assertion `CastInst->isCast() && "Expected an cast instruction!"' failed.

@c.a = external global i32, align 1

define void @c() {
; CHECK-LABEL: @c(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TOBOOL:%.*]] = icmp ne i16 0, 0
; CHECK-NEXT:    br i1 undef, label [[LBL1_US:%.*]], label [[ENTRY_ENTRY_SPLIT_CRIT_EDGE:%.*]]
; CHECK:       entry.entry.split_crit_edge:
; CHECK-NEXT:    [[CONST:%.*]] = bitcast i32 1232131 to i32
; CHECK-NEXT:    br label [[LBL1:%.*]]
; CHECK:       lbl1.us:
; CHECK-NEXT:    [[CONST1:%.*]] = bitcast i32 1232131 to i32
; CHECK-NEXT:    store i32 [[CONST1]], i32* @c.a, align 1
; CHECK-NEXT:    br label [[FOR_COND4:%.*]]
; CHECK:       lbl1:
; CHECK-NEXT:    store i32 [[CONST]], i32* @c.a, align 1
; CHECK-NEXT:    br i1 undef, label [[IF_THEN:%.*]], label [[FOR_END12:%.*]]
; CHECK:       if.then:
; CHECK-NEXT:    br i1 undef, label [[LBL1]], label [[FOR_COND4]]
; CHECK:       for.cond4:
; CHECK-NEXT:    br label [[FOR_COND4]]
; CHECK:       for.body9:
; CHECK-NEXT:    store i32 1232131, i32* undef, align 1
; CHECK-NEXT:    store i32 1232132, i32* undef, align 1
; CHECK-NEXT:    br label [[FOR_BODY9:%.*]]
; CHECK:       for.end12:
; CHECK-NEXT:    ret void
;
entry:
  %tobool = icmp ne i16 0, 0
  br i1 undef, label %lbl1.us, label %entry.entry.split_crit_edge

entry.entry.split_crit_edge:                      ; preds = %entry
  br label %lbl1

lbl1.us:                                          ; preds = %entry
  store i32 1232131, i32* @c.a, align 1
  br label %for.cond4

lbl1:                                             ; preds = %if.then, %entry.entry.split_crit_edge
  store i32 1232131, i32* @c.a, align 1
  br i1 undef, label %if.then, label %for.end12

if.then:                                          ; preds = %lbl1
  br i1 undef, label %lbl1, label %for.cond4

for.cond4:                                        ; preds = %for.cond4, %if.then, %lbl1.us
  br label %for.cond4

for.body9:                                        ; preds = %for.body9
  store i32 1232131, i32* undef, align 1
  store i32 1232132, i32* undef, align 1
  br label %for.body9

for.end12:                                        ; preds = %lbl1
  ret void
}
