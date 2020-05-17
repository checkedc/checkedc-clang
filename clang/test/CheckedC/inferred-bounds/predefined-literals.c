// Tests of inferred bounds for predefined literals like __func__, etc.
//
// RUN: %clang_cc1 -fdump-inferred-bounds %s | FileCheck %s
// expected-no-diagnostics

void f1() {
// CHECK:      ImplicitCastExpr {{0x[0-9a-f]+}} '_Ptr<const char>' <BitCast>
// CHECK-NEXT: |-Inferred SubExpr Bounds
// CHECK-NEXT: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK-NEXT: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *':'const char *' <ArrayToPointerDecay>
// CHECK-NEXT: |   | `-BoundsValueExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue _BoundTemporary {{0x[0-9a-f]+}}
// CHECK-NEXT: |   `-BinaryOperator {{0x[0-9a-f]+}} 'const char *':'const char *' '+'
// CHECK-NEXT: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *':'const char *' <ArrayToPointerDecay>
// CHECK-NEXT: |     | `-BoundsValueExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue _BoundTemporary {{0x[0-9a-f]+}}
// CHECK-NEXT: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK-NEXT: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *' <ArrayToPointerDecay>
// CHECK-NEXT:   `-CHKCBindTemporaryExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue
// CHECK-NEXT:     `-PredefinedExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue __func__
// CHECK-NEXT:       `-StringLiteral {{0x[0-9a-f]+}} 'const char [3]' lvalue "f1"
  _Ptr<const char> s1 = __func__;
}

void f2(_Ptr<const char> s2);
void f3() {
// CHECK:      ImplicitCastExpr {{0x[0-9a-f]+}} '_Ptr<const char>' <BitCast>
// CHECK-NEXT: |-Inferred SubExpr Bounds
// CHECK-NEXT: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK-NEXT: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *':'const char *' <ArrayToPointerDecay>
// CHECK-NEXT: |   | `-BoundsValueExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue _BoundTemporary {{0x[0-9a-f]+}}
// CHECK-NEXT: |   `-BinaryOperator {{0x[0-9a-f]+}} 'const char *':'const char *' '+'
// CHECK-NEXT: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *':'const char *' <ArrayToPointerDecay>
// CHECK-NEXT: |     | `-BoundsValueExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue _BoundTemporary {{0x[0-9a-f]+}}
// CHECK-NEXT: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK-NEXT: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *' <ArrayToPointerDecay>
// CHECK-NEXT:   `-CHKCBindTemporaryExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue
// CHECK-NEXT:     `-PredefinedExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue __func__
// CHECK-NEXT:       `-StringLiteral {{0x[0-9a-f]+}} 'const char [3]' lvalue "f3"
  f2(__func__);
}

void f4() {
// CHECK:      ImplicitCastExpr {{0x[0-9a-f]+}} '_Ptr<const char>' <BitCast>
// CHECK-NEXT: |-Inferred SubExpr Bounds
// CHECK-NEXT: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK-NEXT: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *':'const char *' <ArrayToPointerDecay>
// CHECK-NEXT: |   | `-BoundsValueExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue _BoundTemporary {{0x[0-9a-f]+}}
// CHECK-NEXT: |   `-BinaryOperator {{0x[0-9a-f]+}} 'const char *':'const char *' '+'
// CHECK-NEXT: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *':'const char *' <ArrayToPointerDecay>
// CHECK-NEXT: |     | `-BoundsValueExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue _BoundTemporary {{0x[0-9a-f]+}}
// CHECK-NEXT: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK-NEXT: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'const char *' <ArrayToPointerDecay>
// CHECK-NEXT:   `-CHKCBindTemporaryExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue
// CHECK-NEXT:     `-PredefinedExpr {{0x[0-9a-f]+}} 'const char [3]' lvalue __FUNCTION__
// CHECK-NEXT:       `-StringLiteral {{0x[0-9a-f]+}} 'const char [3]' lvalue "f4"
  _Ptr<const char> s4 = __FUNCTION__;
}

void f5() {
// CHECK:      |-RangeBoundsExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}, col:{{[0-9]+}}> 'NULL TYPE'
// CHECK-NEXT: | |-ImplicitCastExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> '_Array_ptr<const char>' <LValueToRValue>
// CHECK-NEXT: | | `-DeclRefExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> '_Array_ptr<const char>' lvalue Var {{0x[0-9a-f]+}} 's5' '_Array_ptr<const char>'
// CHECK-NEXT: | `-BinaryOperator {{0x[0-9a-f]+}} <col:{{[0-9]+}}, col:{{[0-9]+}}> '_Array_ptr<const char>' '+'
// CHECK-NEXT: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> '_Array_ptr<const char>' <LValueToRValue>
// CHECK-NEXT: |   | `-DeclRefExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> '_Array_ptr<const char>' lvalue Var {{0x[0-9a-f]+}} 's5' '_Array_ptr<const char>'
// CHECK-NEXT: |   `-IntegerLiteral {{0x[0-9a-f]+}} <col:{{[0-9]+}}> 'int' 2
// CHECK-NEXT: `-ImplicitCastExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> '_Array_ptr<const char>' <BitCast>
// CHECK-NEXT:   `-ImplicitCastExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> 'const char *' <ArrayToPointerDecay>
// CHECK-NEXT:     `-CHKCBindTemporaryExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> 'const char [3]' lvalue
// CHECK-NEXT:       `-PredefinedExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> 'const char [3]' lvalue __func__
// CHECK-NEXT:         `-StringLiteral {{0x[0-9a-f]+}} <col:{{[0-9]+}}> 'const char [3]' lvalue "f5"
  _Array_ptr<const char> s5 : bounds(s5, s5+ 2) = __func__;
}

char f6() {
// CHECK:      ImplicitCastExpr {{0x[0-9a-f]+}} 'char' <LValueToRValue>
// CHECK-NEXT: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'const char' lvalue
// CHECK-NEXT:   |-Bounds Null-terminated read
// CHECK-NEXT:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK-NEXT:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Nt_array_ptr<const char>':'_Nt_array_ptr<const char>' <ArrayToPointerDecay>
// CHECK-NEXT:   |   | `-BoundsValueExpr {{0x[0-9a-f]+}} 'const char _Nt_checked[3]' lvalue _BoundTemporary {{0x[0-9a-f]+}}
// CHECK-NEXT:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Nt_array_ptr<const char>':'_Nt_array_ptr<const char>' '+'
// CHECK-NEXT:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Nt_array_ptr<const char>':'_Nt_array_ptr<const char>' <ArrayToPointerDecay>
// CHECK-NEXT:   |     | `-BoundsValueExpr {{0x[0-9a-f]+}} 'const char _Nt_checked[3]' lvalue _BoundTemporary {{0x[0-9a-f]+}}
// CHECK-NEXT:   |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK-NEXT:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Nt_array_ptr<const char>' <ArrayToPointerDecay>
// CHECK-NEXT:   | `-CHKCBindTemporaryExpr {{0x[0-9a-f]+}} 'const char _Nt_checked[3]' lvalue
// CHECK-NEXT:   |   `-PredefinedExpr {{0x[0-9a-f]+}} 'const char _Nt_checked[3]' lvalue __func__
// CHECK-NEXT:   |     `-StringLiteral {{0x[0-9a-f]+}} 'const char _Nt_checked[3]' lvalue "f6"
// CHECK-NEXT:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
#pragma CHECKED_SCOPE ON
  return __func__[1];
#pragma CHECKED_SCOPE OFF
}
