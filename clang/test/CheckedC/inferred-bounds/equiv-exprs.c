// Tests for updating equivalent expression information during bounds inference and checking.
// This file tests updating the set of expressions that produces the same value as an expression
// after checking the expression during bounds analysis.
// This file does not test assignments that update the set of sets of equivalent expressions
// (assignments will be tested in a separate test file).
//
// RUN: %clang_cc1 -Wno-unused-value -fdump-checking-state %s | FileCheck %s

#include <stdchecked.h>

extern int a1 [12];
extern void g1(void);

// Note: the expressions tested below include some kinds of
// expressions which bounds checking currently does update
// equivalent expression sets for (such as casts and integer literals).
// The equivalent expression sets after checking an unsupported expression
// kind will be the same as the sets after checking the expression's children.

// DeclRefExpr
void f1(int i, int a checked[5]) {
  // Non-array, non-function type
  i;
  // CHECK: Statement S:
  // CHECK: DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT: `-DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // TODO: update equivalent expression sets for an ImplicitCastExpr
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT: `-DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT: `-DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }

  // Function type
  g1;
  // CHECK: Statement S:
  // CHECK: DeclRefExpr {{.*}} 'g1'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }
  // TODO: update equivalent expression sets for an ImplicitCastExpr
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <FunctionToPointerDecay>
  // CHECK-NEXT: `-DeclRefExpr {{.*}} 'g1'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }

  // Local checked array with known size
  int arr checked[10];
  arr;
  // CHECK: Statement S:
  // CHECK: DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: }
  // TODO: update equivalent expression sets for an ImplicitCastExpr
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT: `-DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: }

  // Extern unchecked array with known size
  a1;
  // CHECK: Statement S:
  // CHECK: DeclRefExpr {{.*}} 'a1'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a1'
  // CHECK-NEXT: }
  // TODO: update equivalent expression sets for an ImplicitCastExpr
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT: `-DeclRefExpr {{.*}} 'a1'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a1'
  // CHECK-NEXT: }

  // Array parameter with _Array_ptr<int> type
  a;
  // CHECK: Statement S:
  // CHECK: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT: `-DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
  // TODO: update equivalent expression sets for an ImplicitCastExpr
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT: `-DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT: `-DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
}

// UnaryOperator: pointer dereferences
void f2(int *p) {
  *p;
  // CHECK: Statement S:
  // CHECK: DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT: `-DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: }
  // TODO: update equivalent expression sets for an ImplicitCastExpr
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT: `-DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT: `-DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} '*'
  // CHECK-NEXT: `-ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   `-DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }
  // TODO: update equivalent expression sets for an ImplicitCastExpr
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT: `-UnaryOperator {{.*}} '*'
  // CHECK-NEXT:  `-ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:  `-DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }
}

// ArraySubscriptExpr
void f3(int a [1]) {
  a[0];
  // CHECK: Statement S:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT: `-DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
  // TODO: update equivalent expression sets for an ImplicitCastExpr
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT: `-DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT: `-DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
  // TODO: update equivalent expression sets for an IntegerLiteral
  // CHECK: Statement S:
  // CHECK-NEXT: IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT: `-DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: ArraySubscriptExpr {{.*}} 'int'
  // CHECK-NEXT: |-ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT: | `-DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: `-IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }
  // TODO: update equivalent expression sets for an ImplicitCastExpr
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT: `-ArraySubscriptExpr {{.*}} lvalue
  // CHECK-NEXT:   |-ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   | `-DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   `-IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }
}
