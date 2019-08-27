// Tests of inferred bounds for predefined literals like __func__, etc.
//
// RUN: %clang_cc1 -fdump-inferred-bounds %s | FileCheck %s
// expected-no-diagnostics

void f1() {
// CHECK:      ImplicitCastExpr {{0x[0-9a-f]+}} '_Ptr<const char>' <BitCast>
// CHECK-NEXT: |-Inferred SubExpr Bounds
// CHECK-NEXT: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK-NEXT: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *':'const char *' <ArrayToPointerDecay>
// CHECK-NEXT: |   | `-BoundsValueExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue _BoundTemporary  {{0x[0-9a-f]+}}
// CHECK-NEXT: |   `-BinaryOperator {{0x[0-9a-f]+}} 'const char *':'const char *' '+'
// CHECK-NEXT: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *':'const char *' <ArrayToPointerDecay>
// CHECK-NEXT: |     | `-BoundsValueExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue _BoundTemporary  {{0x[0-9a-f]+}}
// CHECK-NEXT: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK-NEXT: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *' <ArrayToPointerDecay>
// CHECK-NEXT:   `-PredefinedExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue __func__
// CHECK-NEXT:     `-StringLiteral {{0x[0-9a-f]+}} 'const char [3]' lvalue "f1"
  _Ptr<const char> s1 = __func__;
}

void f2(_Ptr<const char> s2);
void f3() {
// CHECK:      ImplicitCastExpr {{0x[0-9a-f]+}} '_Ptr<const char>' <BitCast>
// CHECK-NEXT: |-Inferred SubExpr Bounds
// CHECK-NEXT: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK-NEXT: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *':'const char *' <ArrayToPointerDecay>
// CHECK-NEXT: |   | `-BoundsValueExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue _BoundTemporary  {{0x[0-9a-f]+}}
// CHECK-NEXT: |   `-BinaryOperator {{0x[0-9a-f]+}} 'const char *':'const char *' '+'
// CHECK-NEXT: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *':'const char *' <ArrayToPointerDecay>
// CHECK-NEXT: |     | `-BoundsValueExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue _BoundTemporary  {{0x[0-9a-f]+}}
// CHECK-NEXT: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK-NEXT: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *' <ArrayToPointerDecay>
// CHECK-NEXT:   `-PredefinedExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue __func__
// CHECK-NEXT:     `-StringLiteral {{0x[0-9a-f]+}} 'const char [3]' lvalue "f3"
  f2(__func__);
}

void f4() {
// CHECK:      ImplicitCastExpr {{0x[0-9a-f]+}} '_Ptr<const char>' <BitCast>
// CHECK-NEXT: |-Inferred SubExpr Bounds
// CHECK-NEXT: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK-NEXT: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *':'const char *' <ArrayToPointerDecay>
// CHECK-NEXT: |   | `-BoundsValueExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue _BoundTemporary  {{0x[0-9a-f]+}}
// CHECK-NEXT: |   `-BinaryOperator {{0x[0-9a-f]+}} 'const char *':'const char *' '+'
// CHECK-NEXT: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *':'const char *' <ArrayToPointerDecay>
// CHECK-NEXT: |     | `-BoundsValueExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue _BoundTemporary  {{0x[0-9a-f]+}}
// CHECK-NEXT: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK-NEXT: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *' <ArrayToPointerDecay>
// CHECK-NEXT:   `-PredefinedExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue __FUNCTION__
// CHECK-NEXT:     `-StringLiteral {{0x[0-9a-f]+}} 'const char [3]' lvalue "f4"
  _Ptr<const char> s4 = __FUNCTION__;
}
