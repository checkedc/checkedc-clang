// Tests of inferred bounds for predefined literals like __func__, etc.
//
// RUN: %clang_cc1 -fcheckedc-extension -verify -fdump-inferred-bounds %s | FileCheck %s
// expected-no-diagnostics

void f1() {
// CHECK:      ImplicitCastExpr {{0x[0-9a-f]+}} '_Ptr<const char>' <BitCast>
// CHECK-NEXT: |-Inferred SubExpr Bounds
// CHECK-NEXT: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK-NEXT: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *':'const char *' <ArrayToPointerDecay>
// CHECK-NEXT: |   | `-PredefinedExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue __func__
// CHECK-NEXT: |   |   `-StringLiteral {{0x[0-9a-f]+}} 'const char [3]' lvalue "f1"
// CHECK-NEXT: |   `-BinaryOperator {{0x[0-9a-f]+}} 'const char *':'const char *' '+'
// CHECK-NEXT: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *':'const char *' <ArrayToPointerDecay>
// CHECK-NEXT: |     | `-PredefinedExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue __func__
// CHECK-NEXT: |     |   `-StringLiteral {{0x[0-9a-f]+}} 'const char [3]' lvalue "f1"
// CHECK-NEXT: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK-NEXT: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *' <ArrayToPointerDecay>
// CHECK-NEXT:   `-PredefinedExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue __func__
// CHECK-NEXT:     `-StringLiteral {{0x[0-9a-f]+}} 'const char [3]' lvalue "f1"

  _Ptr<const char> s = __func__;
}
