// Tests for dumping of ASTS for where-clauses with Checked C extensions.
// This makes sure that additional information appears as
// expected.
// RUN: %clang_cc1 -fcheckedc-extension -fdump-where-clauses \
// RUN: -verify -verify-ignore-unexpected=note -verify-ignore-unexpected=warning \
// RUN: -ast-dump %s \
// RUN: | FileCheck %s --check-prefix=CHECK-AST
// expected-no-diagnostics

// CHECK-AST: FunctionDecl {{.*}} f1
// CHECK-AST-NEXT: ParmVarDecl {{.*}} x 'int'
// CHECK-AST-NEXT:   WhereClause
// CHECK-AST-NEXT:     ComparisonFact
// CHECK-AST-NEXT:       BinaryOperator {{.*}} '>'
// CHECK-AST-NEXT:         ImplicitCastExpr {{.*}}
// CHECK-AST-NEXT:           DeclRefExpr {{.*}} 'x'
// CHECK-AST-NEXT:         IntegerLiteral {{.*}} 'int' 0
// CHECK-AST-NEXT: ParmVarDecl {{.*}} r
// CHECK-AST-NEXT:   WhereClause
// CHECK-AST-NEXT:     BoundsDeclFact: r
// CHECK-AST-NEXT:       CountBoundsExpr {{.*}}
// CHECK-AST-NEXT:         IntegerLiteral {{.*}} 'int' 3
void f1(int x _Where x > 0, _Ptr<int> r _Where r : count(3)) {
  _Nt_array_ptr<char> p : bounds(p, p + 10) = "abcdeabcde";

  int x1 = 1 _Where p : bounds(p, p + 10);
  // CHECK-AST: VarDecl {{.*}} x1
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 1
  // CHECK-AST-NEXT: WhereClause
  // CHECK-AST-NEXT:   BoundsDeclFact: p
  // CHECK-AST-NEXT:     RangeBoundsExpr
  // CHECK-AST-NEXT:       ImplicitCastExpr {{.*}}
  // CHECK-AST-NEXT:         DeclRefExpr {{.*}} 'p' {{.*}}
  // CHECK-AST-NEXT:       BinaryOperator {{.*}} '+'
  // CHECK-AST-NEXT:         ImplicitCastExpr {{.*}}
  // CHECK-AST-NEXT:           DeclRefExpr {{.*}} 'p' {{.*}} 
  // CHECK-AST-NEXT:         IntegerLiteral {{.*}} 'int' 10

  int x2 = 2, y2 = 2 _Where x >= 1;
  // CHECK-AST: VarDecl {{.*}} x2
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 2
  // CHECK-AST-NEXT: VarDecl {{.*}} y2
  // CHECK-AST-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK-AST-NEXT:     WhereClause
  // CHECK-AST-NEXT:       ComparisonFact
  // CHECK-AST-NEXT:         BinaryOperator {{.*}} '>='
  // CHECK-AST-NEXT:           ImplicitCastExpr {{.*}}
  // CHECK-AST-NEXT:             DeclRefExpr {{.*}} 'x'
  // CHECK-AST-NEXT:         IntegerLiteral {{.*}} 'int' 1

  int x3 = 3 _Where x >= 31 _And x < 32;
  // CHECK-AST: VarDecl {{.*}} x3
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 3
  // CHECK-AST-NEXT: WhereClause
  // CHECK-AST-NEXT:   ComparisonFact
  // CHECK-AST-NEXT:     BinaryOperator {{.*}} '>='
  // CHECK-AST-NEXT:       ImplicitCastExpr {{.*}}
  // CHECK-AST-NEXT:         DeclRefExpr {{.*}} 'x'
  // CHECK-AST-NEXT:       IntegerLiteral {{.*}} 'int' 31
  // CHECK-AST-NEXT:   ComparisonFact
  // CHECK-AST-NEXT:     BinaryOperator {{.*}} '<'
  // CHECK-AST-NEXT:       ImplicitCastExpr {{.*}}
  // CHECK-AST-NEXT:         DeclRefExpr {{.*}} 'x'
  // CHECK-AST-NEXT:       IntegerLiteral {{.*}} 'int' 32

  int x4 = 4 _Where x >= 31 _And p : count(x) _And x <= 34;
  // CHECK-AST: VarDecl {{.*}} x4
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 4
  // CHECK-AST-NEXT: WhereClause
  // CHECK-AST-NEXT:   ComparisonFact
  // CHECK-AST-NEXT:     BinaryOperator {{.*}} '>='
  // CHECK-AST-NEXT:       ImplicitCastExpr {{.*}}
  // CHECK-AST-NEXT:         DeclRefExpr {{.*}} 'x'
  // CHECK-AST-NEXT:       IntegerLiteral {{.*}} 'int' 31
  // CHECK-AST-NEXT:   BoundsDeclFact: p
  // CHECK-AST-NEXT:     CountBoundsExpr {{.*}}
  // CHECK-AST-NEXT:       ImplicitCastExpr {{.*}}
  // CHECK-AST-NEXT:         DeclRefExpr {{.*}} 'x'
  // CHECK-AST-NEXT:   ComparisonFact
  // CHECK-AST-NEXT:     BinaryOperator {{.*}} '<='
  // CHECK-AST-NEXT:       ImplicitCastExpr {{.*}}
  // CHECK-AST-NEXT:         DeclRefExpr {{.*}} 'x'
  // CHECK-AST-NEXT:       IntegerLiteral {{.*}} 'int' 34

  int x5 = 2;

  _Where x > 6 _And x < 62;
  // CHECK-AST: NullStmt
  // CHECK-AST-NEXT: WhereClause
  // CHECK-AST-NEXT:   ComparisonFact
  // CHECK-AST-NEXT:     BinaryOperator {{.*}} '>'
  // CHECK-AST-NEXT:       ImplicitCastExpr {{.*}}
  // CHECK-AST-NEXT:         DeclRefExpr {{.*}} 'x'
  // CHECK-AST-NEXT:       IntegerLiteral {{.*}} 'int' 6
  // CHECK-AST-NEXT:   ComparisonFact
  // CHECK-AST-NEXT:     BinaryOperator {{.*}} '<'
  // CHECK-AST-NEXT:       ImplicitCastExpr {{.*}}
  // CHECK-AST-NEXT:         DeclRefExpr {{.*}} 'x'
  // CHECK-AST-NEXT:       IntegerLiteral {{.*}} 'int' 62

  x5 = 42 _Where x5 > 41;
  // CHECK-AST: BinaryOperator {{.*}} '='
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'x5' 'int'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 42
  // CHECK-AST-NEXT: WhereClause
  // CHECK-AST-NEXT:   ComparisonFact
  // CHECK-AST-NEXT:     BinaryOperator {{.*}} '>'
  // CHECK-AST-NEXT:       ImplicitCastExpr {{.*}}
  // CHECK-AST-NEXT:         DeclRefExpr {{.*}} 'x5'
  // CHECK-AST-NEXT:       IntegerLiteral {{.*}} 'int' 41

  ++x5 _Where x5 > 41;
  // CHECK-AST: UnaryOperator {{.*}} '++'
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'x5' 'int'
  // CHECK-AST-NEXT: WhereClause
  // CHECK-AST-NEXT:   ComparisonFact
  // CHECK-AST-NEXT:     BinaryOperator {{.*}} '>'
  // CHECK-AST-NEXT:       ImplicitCastExpr {{.*}}
  // CHECK-AST-NEXT:         DeclRefExpr {{.*}} 'x5'
  // CHECK-AST-NEXT:       IntegerLiteral {{.*}} 'int' 41
}

void f2(_Array_ptr<char> buf : count(len), int len) {
  _Array_ptr<int> r : count(6) = _Dynamic_bounds_cast<_Array_ptr<int>>(buf, bounds(r, r + 6));
  // CHECK-AST: VarDecl {{.*}} r
  // check the RHS, which has four parts
  // CHECK-AST:    BoundsCastExpr {{.*}} '_Array_ptr<int>' <DynamicPtrBounds>
  //
  // check part1
  // CHECK-AST-NEXT: Normalized Bounds
  // CHECK-AST-NEXT:   RangeBoundsExpr
  // CHECK-AST-NEXT:     ImplicitCastExpr
  // CHECK-AST-NEXT:        DeclRefExpr {{.*}} 'r'
  // CHECK-AST-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-AST-NEXT:       ImplicitCastExpr
  // CHECK-AST-NEXT:         DeclRefExpr {{.*}} 'r'
  // CHECK-AST-NEXT:       IntegerLiteral {{.*}} 'int' 6
  //
  // check part2
  // CHECK-AST-NEXT: Inferred SubExpr Bounds
  // check bounds       (buf, buf+len)
  //
  // check part3
  // CHECK-AST:      CHKCBindTemporaryExpr {{.*}} '_Array_ptr<char>'
  // CHECK-AST-NEXT:   ImplicitCastExpr {{.*}} '_Array_ptr<char>'
  // CHECK-AST-NEXT:     DeclRefExpr {{.*}} 'buf'
  //
  // check part4
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT:   ImplicitCastExpr
  // CHECK-AST-NEXT:      DeclRefExpr {{.*}} 'r'
  // CHECK-AST-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-AST-NEXT:     ImplicitCastExpr
  // CHECK-AST-NEXT:       DeclRefExpr {{.*}} 'r'
  // CHECK-AST-NEXT:     IntegerLiteral {{.*}} 'int' 6

  // incorrect syntax
  // _Where r : _Dynamic_bounds_cast<_Array_ptr<int>>(buf, bounds(r, r + 6));
  // _Where r : count(6) = _Dynamic_bounds_cast<_Array_ptr<int>>(buf, bounds(r, r + 6));
 
  _Array_ptr<char> ga : count(2) = "ab";
  // CHECK-AST:    DeclStmt

  _Array_ptr<char> a
           : bounds(
              _Assume_bounds_cast<_Array_ptr<int>>(a, count(1)),
              _Assume_bounds_cast<_Array_ptr<int>>(ga, count(2)) + 3) = "foobar";
  // CHECK-AST:    DeclStmt
  // CHECK-AST-NEXT: VarDecl {{.*}} a
  // CHECK-AST-NEXT: RangeBoundsExpr
  // check for the lower bound
  // CHECK-AST-NEXT:   BoundsCastExpr {{.*}} '_Array_ptr<int>' <AssumePtrBounds>
  // CHECK-AST-NEXT:     CHKCBindTemporaryExpr {{.*}} '_Array_ptr<char>'
  // CHECK-AST-NEXT:       ImplicitCastExpr
  // CHECK-AST-NEXT:         DeclRefExpr {{.*}} 'a' '_Array_ptr<char>'
  // CHECK-AST-NEXT:     CountBoundsExpr
  // CHECK-AST-NEXT:       IntegerLiteral {{.*}} 'int' 1
  // check for the upper bound (omit)

  _Where a : bounds(
              _Assume_bounds_cast<_Array_ptr<int>>(a, count(1)),
              _Assume_bounds_cast<_Array_ptr<int>>(ga, count(2)) + 3);
  // CHECK-AST: NullStmt
  // CHECK-AST-NEXT: WhereClause
  // CHECK-AST-NEXT:   BoundsDeclFact: a
  // CHECK-AST-NEXT:     RangeBoundsExpr
  // check for the lower bound
  // CHECK-AST-NEXT:   BoundsCastExpr {{.*}} '_Array_ptr<int>' <AssumePtrBounds>
  // CHECK-AST-NEXT:     CHKCBindTemporaryExpr {{.*}} '_Array_ptr<char>'
  // CHECK-AST-NEXT:       ImplicitCastExpr
  // CHECK-AST-NEXT:         DeclRefExpr {{.*}} 'a' '_Array_ptr<char>'
  // CHECK-AST-NEXT:     CountBoundsExpr
  // CHECK-AST-NEXT:       IntegerLiteral {{.*}} 'int' 1
  // check for the upper bound (omit)
}
