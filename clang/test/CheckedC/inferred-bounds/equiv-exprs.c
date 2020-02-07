// Tests for updating equivalent expression information during bounds inference and checking.
// This file tests updating the set of expressions that produces the same value as an expression
// after checking the expression during bounds analysis.
// This file does not test assignments that update the set of sets of equivalent expressions
// (assignments will be tested in a separate test file).
//
// RUN: %clang_cc1 -Wno-unused-value -fdump-checking-state %s | FileCheck %s --dump-input=always

#include <stdchecked.h>

extern int a1 [12];
extern void g1(void);

// DeclRefExpr
// Note: these expressions also include implicit LValueToRValue and
// ArrayToPointerDecay casts.  These casts are not currently handled by
// bounds checking methods.
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
  // CHECK-NEXT: DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }

  // Function type
  g1;
  // CHECK: Statement S:
  // CHECK: DeclRefExpr {{.*}} 'g1'
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

  // Array parameter with _Array_ptr<int> type
  a;
  // CHECK: Statement S:
  // CHECK: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
}
