//===- CanonBounds.cpp: comparison and canonicalization for bounds exprs -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements methods to canoncialize expressions used in bounds
//  expressions and determine if they should be considered semantically
//  equivalent.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/CanonBounds.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"

using namespace clang;

LexicographicCompare::Result LexicographicCompare::CompareInteger(signed I1, signed I2) {
  if (I1 < I2)
    return Result::LessThan;
  else if (I1 > I2)
    return Result::GreaterThan;
  else
    return Result::Equal;
}

LexicographicCompare::Result LexicographicCompare::CompareInteger(unsigned I1, unsigned I2) {
  if (I1 < I2)
    return Result::LessThan;
  else if (I1 > I2)
    return Result::GreaterThan;
  else
    return Result::Equal;
}

LexicographicCompare::Result LexicographicCompare::CompareExpr(const Expr *E1, const Expr *E2) {
   Stmt::StmtClass E1Kind = E1->getStmtClass();
   Stmt::StmtClass E2Kind = E2->getStmtClass();
   Result Cmp = CompareInteger(E1Kind, E2Kind);
   if (Cmp != Result::Equal)
     return Cmp;

   Cmp = Result::Equal; 
   switch (E1Kind) {
     case Stmt::NoStmtClass:
#define ABSTRACT_STMT(Kind)
#define STMT(Kind, Base) case Expr::Kind##Class:
#define EXPR(Kind, Base)
#include "clang/AST/StmtNodes.inc"
       llvm_unreachable("cannot compare a statement");  
     case Expr::PredefinedExprClass: Cmp = ComparePredefinedExpr(E1, E2); break;
     case Expr::DeclRefExprClass: return CompareDeclRefExpr(E1, E2);
     case Expr::IntegerLiteralClass: return CompareIntegerLiteral(E1, E2);
     case Expr::FloatingLiteralClass: return CompareFloatingLiteral(E1, E2);
     case Expr::ImaginaryLiteralClass: return CompareImaginaryLiteral(E1, E2);
     case Expr::StringLiteralClass: return CompareStringLiteral(E1, E2);
     case Expr::CharacterLiteralClass: return CompareCharacterLiteral(E1, E2);
     case Expr::ParenExprClass: break;
     case Expr::UnaryOperatorClass:Cmp = CompareUnaryOperator(E1, E2); break;
     case Expr::OffsetOfExprClass: Cmp = CompareOffsetOfExpr(E1, E2); break;
     case Expr::ArraySubscriptExprClass: break;
     case Expr::CallExprClass: break;
     case Expr::MemberExprClass: Cmp = CompareMemberExpr(E1, E2); break;
     case Expr::BinaryOperatorClass: Cmp = CompareBinaryOperator(E1, E2); break;
     case Expr::CompoundAssignOperatorClass:
       Cmp = CompareCompoundAssignOperator(E1, E2); break;
     case Expr::BinaryConditionalOperatorClass: break;
     case Expr::ImplicitCastExprClass: Cmp = CompareImplicitCastExpr(E1, E2); break;
     case Expr::CStyleCastExprClass: Cmp = CompareCStyleCastExpr(E1, E2); break;
     case Expr::CompoundLiteralExprClass: Cmp = CompareCompoundLiteralExpr(E1, E2); break;
     // TODO:
     // case: ExtVectorElementExpr
     case Expr::VAArgExprClass: break;
     case Expr::GenericSelectionExprClass: Cmp = CompareGenericSelectionExpr(E1, E2); break;

     // Atomic expressions.
     case Expr::AtomicExprClass: break;

     // GNU Extensions.
     case Expr::AddrLabelExprClass: break;
     case Expr::StmtExprClass: break;
     case Expr::ChooseExprClass: break;
     case Expr::GNUNullExprClass: break;

     // Bounds expressions
     case Expr::NullaryBoundsExprClass: Cmp = CompareNullaryBoundsExpr(E1, E2); break;
     case Expr::CountBoundsExprClass: Cmp = CompareCountBoundsExpr(E1, E2); break;
     case Expr::RangeBoundsExprClass: break;
     case Expr::InteropTypeBoundsAnnotationClass: Cmp = CompareInteropTypeBoundsAnnotation(E1, E2); break;
     case Expr::PositionalParameterExprClass: Cmp = ComparePositionalParameterExpr(E1, E2); break;
     case Expr::BoundsCastExprClass: Cmp = CompareBoundsCastExpr(E1, E2); break;

     // Clang extensions
     case Expr::ShuffleVectorExprClass: break;
     case Expr::ConvertVectorExprClass: break;
     case Expr::BlockExprClass: Cmp = CompareBlockExpr(E1, E2); break;
     case Expr::OpaqueValueExprClass: break;
     case Expr::TypoExprClass: break;
     // TODO:
     // case Expr::MSPropertyRefExprClass:
     // case Expr::MSPropertySubscriptExprClass:

     default:
       llvm_unreachable("unexpected expression kind");         
   }

   if (Cmp != Result::Equal)
     return Cmp;

   // Check children
   auto I1 = E1->child_begin();
   auto I2 = E2->child_begin();
   for ( ;  I1 != E1->child_end() && I2 != E2->child_end(); ++I1, ++I2) {
     const Stmt *E1Child = *I1;
     const Stmt *E2Child = *I2;
     if (E1Child == nullptr && E2Child != nullptr)
       return Result::LessThan;
     if (E1Child != nullptr && E2Child == nullptr)
       return Result::GreaterThan;
     if (E1Child == nullptr && E2Child == nullptr)
       continue;

     const Expr *E1ChildExpr = dyn_cast<Expr>(E1Child);
     const Expr *E2ChildExpr = dyn_cast<Expr>(E2Child);
     assert(E1ChildExpr && E2ChildExpr && "expected subexpression");
     if (E1ChildExpr && E2ChildExpr) {
       Result Cmp = CompareExpr(E1ChildExpr, E2ChildExpr);
       if (Cmp != Result::Equal)
         return Cmp;
     } else
       return Result::LessThan;
   }

   // Make sure the number of children was equal.  If E1 has
   // fewer children than E2, make it less than.  If it has more
   // children, make it greater than.
   if (I1 == E1->child_end() && I2 != E2->child_end())
     return Result::LessThan;

   if (I1 != E1->child_end() && I2 == E2->child_end())
     return Result::GreaterThan;

   return LexicographicCompare::Result::Equal;
}

LexicographicCompare::Result 
LexicographicCompare::CompareDecl(const ValueDecl *D1, const ValueDecl *D2) {
  return Result::Equal;
}

#define CHECK_WRAPPER(N) \
LexicographicCompare::Result \
LexicographicCompare::Compare##N(const Expr *Raw1, const Expr *Raw2) { \
  const N *E1 = dyn_cast<N>(Raw1); \
  const N *E2 = dyn_cast<N>(Raw2); \
  if (!E1 || !E2)  { \
    llvm_unreachable("dyn_cast failed"); \
    return Result::LessThan; \
  } 

CHECK_WRAPPER(PredefinedExpr)
  return CompareInteger(E1->getIdentType(), E2->getIdentType());
}

CHECK_WRAPPER(DeclRefExpr)
  return Result::Equal;
}

CHECK_WRAPPER(IntegerLiteral)
  return Result::Equal;
}

CHECK_WRAPPER(FloatingLiteral)
  return Result::Equal;
}

CHECK_WRAPPER(ImaginaryLiteral)
  return Result::Equal;
}

CHECK_WRAPPER(StringLiteral)
  return Result::Equal;
}

CHECK_WRAPPER(CharacterLiteral)
  return Result::Equal;
}

CHECK_WRAPPER(UnaryOperator)
  return Result::Equal;
}

CHECK_WRAPPER(OffsetOfExpr)
  return Result::Equal;
}

CHECK_WRAPPER(MemberExpr)
  return Result::Equal;
}

CHECK_WRAPPER(BinaryOperator)
  return Result::Equal;
}


CHECK_WRAPPER(CompoundAssignOperator)
  return Result::Equal;
}


CHECK_WRAPPER(ImplicitCastExpr)
  return Result::Equal;
}

CHECK_WRAPPER(CompoundLiteralExpr)
  return Result::Equal;
}

CHECK_WRAPPER(GenericSelectionExpr)
  return Result::Equal;
}

CHECK_WRAPPER(NullaryBoundsExpr)
  return Result::Equal;
}

CHECK_WRAPPER(CountBoundsExpr)
  return Result::Equal;
}

CHECK_WRAPPER(InteropTypeBoundsAnnotation)
  return Result::Equal;
}

CHECK_WRAPPER(PositionalParameterExpr)
  return Result::Equal;
}

CHECK_WRAPPER(BoundsCastExpr)
  return Result::Equal;
}

CHECK_WRAPPER(BlockExpr)
  return Result::Equal;
}

