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
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/APInt.h"
#include "clang/AST/CanonBounds.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"

using namespace clang;

Lexicographic::Result Lexicographic::CompareInteger(signed I1, signed I2) {
  if (I1 < I2)
    return Result::LessThan;
  else if (I1 > I2)
    return Result::GreaterThan;
  else
    return Result::Equal;
}

Lexicographic::Result Lexicographic::CompareInteger(unsigned I1, unsigned I2) {
  if (I1 < I2)
    return Result::LessThan;
  else if (I1 > I2)
    return Result::GreaterThan;
  else
    return Result::Equal;
}

static Lexicographic::Result CompareAPInt(const llvm::APInt &I1, const llvm::APInt &I2) {
  if (I1.slt(I2))
    return Lexicographic::Result::LessThan;
  else if (I1.eq(I2))
    return Lexicographic::Result::Equal;
  else
    return Lexicographic::Result::GreaterThan;
}

static Lexicographic::Result TranslateInt(int i) {
  if (i == 0) 
    return Lexicographic::Result::Equal;
  else if  (i > 0)
    return Lexicographic::Result::GreaterThan;
  else
    return Lexicographic::Result::LessThan;
}

// \brief See if two pointers can be ordered based on nullness or
// pointer equality.   Set the parameter 'ordered' based on whether
// they can be.  If they can be ordered, return the ordering.
template<typename T> 
static Lexicographic::Result ComparePointers(T *P1, T *P2, bool &ordered) {
  ordered = true;
  if (P1 == P2)
    return Lexicographic::Result::Equal;
  if (P1 == nullptr && P2 != nullptr)
    return Lexicographic::Result::LessThan;
  if (P1 != nullptr && P2 == nullptr)
    return Lexicographic::Result::GreaterThan;
  ordered = false;
  return Lexicographic::Result::LessThan;
}

Lexicographic::Result 
Lexicographic::CompareScope(const DeclContext *DC1, const DeclContext *DC2) {
   DC1 = DC1->getPrimaryContext();
   DC2 = DC2->getPrimaryContext();

  if (DC1 == DC2)
    return Result::Equal;

  Result Cmp = CompareInteger(DC1->getDeclKind(), DC2->getDeclKind());
  if (Cmp != Result::Equal)
    return Cmp;

  switch (DC1->getDeclKind()) {
    case Decl::TranslationUnit: return Result::Equal;
    case Decl::Function: 
    case Decl::Enum:
    case Decl::Record: {
      const NamedDecl *ND1 = dyn_cast<NamedDecl>(DC1);
      const NamedDecl *ND2 = dyn_cast<NamedDecl>(DC2);
      return CompareDecl(ND1, ND2);
    }
    default:
      llvm_unreachable("unexpected scope type");
      return Result::LessThan;
  }
}

Lexicographic::Result 
Lexicographic::CompareDecl(const NamedDecl *D1Arg, const NamedDecl *D2Arg) {
  const NamedDecl *D1 = dyn_cast<NamedDecl>(D1Arg->getCanonicalDecl());
  const NamedDecl *D2 = dyn_cast<NamedDecl>(D2Arg->getCanonicalDecl());
  if (D1 == D2) 
    return Result::Equal;

  if (!D1 || !D2) {
    assert("unexpected cast failure");
    return Result::LessThan;
  }

  Result Cmp = CompareInteger(D1->getKind(), D2->getKind());
  if (Cmp != Result::Equal)
    return Cmp;

  const IdentifierInfo *Name1 = D1->getIdentifier();
  const IdentifierInfo *Name2 = D2->getIdentifier();

  bool ordered;
  Cmp = ComparePointers(D1, D2, ordered);
  if (ordered && Cmp != Result::Equal)
    return Cmp;
  assert(Name1 && Name2);

  Cmp = TranslateInt(Name1->getName().compare(Name2->getName()));
  if (Cmp != Result::Equal)
    return Cmp;

  // They are canonical declarations that have the same kind and the same name, but are not 
  // the same declaration.
  // - If they are in the same semantic scope, one declaration must hide the other.  Order the
  //   earlier declaration as less than the later declaration.
  // - Otherwise order them by scope

  const DeclContext *DC1 = D1->getDeclContext();
  const DeclContext *DC2 = D2->getDeclContext();

  if (!(DC1->Equals(DC2)))
    return CompareScope(DC1, DC2);

  // see if D2 occurs after D1 in the context
  
  const Decl *Current = D1->getNextDeclInContext();
  while (Current != nullptr) {
    if (Current == D2)
      return Result::LessThan;
    Current = Current->getNextDeclInContext();
  }

  // make sure D1 occurs after D2 then
  Current = D2->getNextDeclInContext();
  while (Current != nullptr) {
    if (Current == D1)
      return Result::LessThan;
    Current = Current->getNextDeclInContext();
  }
  llvm_unreachable("unable to order declarations in same context");
  return Result::LessThan;
}

Lexicographic::Result Lexicographic::CompareExpr(const Expr *E1, const Expr *E2) {
   if (E1 == E2)
     return Result::Equal;

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
     case Expr::ImaginaryLiteralClass: break;
     case Expr::StringLiteralClass: return CompareStringLiteral(E1, E2);
     case Expr::CharacterLiteralClass: return CompareCharacterLiteral(E1, E2);
     case Expr::ParenExprClass: break;
     case Expr::UnaryOperatorClass: Cmp = CompareUnaryOperator(E1, E2); break;
     case Expr::OffsetOfExprClass: Cmp = CompareOffsetOfExpr(E1, E2); break;
     case Expr::UnaryExprOrTypeTraitExprClass:
       Cmp = CompareUnaryExprOrTypeTraitExpr(E1, E2); break;
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
     case Expr::AtomicExprClass: CompareAtomicExpr(E1, E2); break;

     // GNU Extensions.
     case Expr::AddrLabelExprClass: break;
     case Expr::StmtExprClass: break;
     case Expr::ChooseExprClass: break;
     case Expr::GNUNullExprClass: break;

     // Bounds expressions
     case Expr::NullaryBoundsExprClass: Cmp = CompareNullaryBoundsExpr(E1, E2); break;
     case Expr::CountBoundsExprClass: Cmp = CompareCountBoundsExpr(E1, E2); break;
     case Expr::RangeBoundsExprClass:  Cmp = CompareRangeBoundsExpr(E1, E2); break; break;
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
     bool ordered = false;
     Cmp = ComparePointers(E1Child, E2Child, ordered);
     if (ordered) {
       if (Cmp == Result::Equal)
         continue;
       return Cmp;
      }
     assert(E1Child && E2Child);
     const Expr *E1ChildExpr = dyn_cast<Expr>(E1Child);
     const Expr *E2ChildExpr = dyn_cast<Expr>(E2Child);
     assert(E1ChildExpr && E2ChildExpr && "dyn_cast failed");
     if (E1ChildExpr && E2ChildExpr) {
       Cmp = CompareExpr(E1ChildExpr, E2ChildExpr);
       if (Cmp != Result::Equal)
         return Cmp;
     } else
       // assert fired - return something
       return Result::LessThan;
   }

   // Make sure the number of children was equal.  If E1 has
   // fewer children than E2, make it less than E2.  If it has more
   // children, make it greater than E2.
   if (I1 == E1->child_end() && I2 != E2->child_end())
     return Result::LessThan;

   if (I1 != E1->child_end() && I2 == E2->child_end())
     return Result::GreaterThan;

   return Lexicographic::Result::Equal;
}

// Lexicogrpahic comparion of properties specific to each expression type. 
// Does not check children of expressions.
#define CHECK_WRAPPER(N) \
Lexicographic::Result \
Lexicographic::Compare##N(const Expr *Raw1, const Expr *Raw2) { \
  const N *E1 = dyn_cast<N>(Raw1); \
  const N *E2 = dyn_cast<N>(Raw2); \
  if (!E1 || !E2)  { \
    llvm_unreachable("dyn_cast failed"); \
    return Result::LessThan; \
  } 

Lexicographic::Result
Lexicographic::CompareType(QualType QT1, QualType QT2) {
  if (QT1 == QT2)
    return Result::Equal;
  else
    // TODO: fill this in
    return Result::LessThan;
}

CHECK_WRAPPER(PredefinedExpr)
  return CompareInteger(E1->getIdentType(), E2->getIdentType());
}

CHECK_WRAPPER(DeclRefExpr)
  return CompareDecl(E1->getDecl(), E2->getDecl());
}

CHECK_WRAPPER(IntegerLiteral)
  BuiltinType::Kind Kind1 = E1->getType()->castAs<BuiltinType>()->getKind();
  BuiltinType::Kind Kind2 = E2->getType()->castAs<BuiltinType>()->getKind();
  Result Cmp = CompareInteger(Kind1, Kind2);
  if (Cmp != Result::Equal)
    return Cmp;
  return CompareAPInt(E1->getValue(), E2->getValue());
}

CHECK_WRAPPER(FloatingLiteral)
  BuiltinType::Kind Kind1 = E1->getType()->castAs<BuiltinType>()->getKind();
  BuiltinType::Kind Kind2 = E2->getType()->castAs<BuiltinType>()->getKind();
  Result Cmp = CompareInteger(Kind1, Kind2);
  if (Cmp != Result::Equal)
    return Cmp;
  Cmp = CompareInteger(E1->isExact(), E2->isExact());
  if (Cmp != Result::Equal)
    return Cmp;
  llvm::APInt E1BitPattern = E1->getValue().bitcastToAPInt();
  llvm::APInt E2BitPattern = E2->getValue().bitcastToAPInt();
  return CompareAPInt(E1BitPattern, E2BitPattern);
}

CHECK_WRAPPER(StringLiteral)
  Result Cmp = CompareInteger(E1->getKind(), E2->getKind());
  if (Cmp != Result::Equal)
    return Cmp;
  return TranslateInt(E1->getBytes().compare(E2->getBytes()));
}

CHECK_WRAPPER(CharacterLiteral)
  Result Cmp = CompareInteger(E1->getKind(), E2->getKind());
  if (Cmp != Result::Equal)
    return Cmp;
  return CompareInteger(E1->getValue(), E2->getValue());
}

CHECK_WRAPPER(UnaryOperator)
  return CompareInteger(E1->getOpcode(), E2->getOpcode());
}

CHECK_WRAPPER(UnaryExprOrTypeTraitExpr)
  Result Cmp = CompareInteger(E1->getKind(), E2->getKind());
  if (Cmp != Result::Equal)
    return Cmp;

  Cmp = CompareInteger(E1->isArgumentType(), E2->isArgumentType());
  if (Cmp != Result::Equal)
    return Cmp;

  if (E1->isArgumentType())
    return CompareType(E1->getArgumentType(), E2->getArgumentType());
  else
    return CompareExpr(E1->getArgumentExpr(), E2->getArgumentExpr());
}

CHECK_WRAPPER(OffsetOfExpr)
  // TODO: fill this in 
  return Result::Equal;
}

CHECK_WRAPPER(MemberExpr)
  Result Cmp = CompareInteger(E1->isArrow(), E2->isArrow());
  if (Cmp != Result::Equal)
    return Cmp;
  return CompareDecl(E1->getMemberDecl(), E2->getMemberDecl());
}

CHECK_WRAPPER(BinaryOperator)
  return CompareInteger(E1->getOpcode(), E2->getOpcode());
}

CHECK_WRAPPER(CompoundAssignOperator)
  return CompareInteger(E1->getOpcode(), E2->getOpcode());
}

CHECK_WRAPPER(ImplicitCastExpr)
  return CompareInteger(E1->getValueKind(), E2->getValueKind());
}

CHECK_WRAPPER(CStyleCastExpr)
  return CompareType(E1->getType(), E2->getType());
}

CHECK_WRAPPER(CompoundLiteralExpr)
  return CompareInteger(E1->isFileScope(), E2->isFileScope());
}

CHECK_WRAPPER(GenericSelectionExpr)
  unsigned E1AssocCount = E1->getNumAssocs();
  Result Cmp = CompareInteger(E1AssocCount, E2->getNumAssocs());
  if (Cmp != Result::Equal)
    return Cmp;
  for (unsigned i = 0; i != E1AssocCount; ++i) {
    Cmp = CompareType(E1->getAssocType(i), E2->getAssocType(i));
    if (Cmp != Result::Equal)
      return Cmp;
    Cmp = CompareExpr(E1->getAssocExpr(i), E2->getAssocExpr(i));
    if (Cmp != Result::Equal)
      return Cmp;
    return Cmp;
  }
  return Result::Equal;
}

CHECK_WRAPPER(AtomicExpr)
  return CompareInteger(E1->getOp(), E2->getOp());
}

CHECK_WRAPPER(NullaryBoundsExpr)
  return CompareInteger(E1->getKind(), E2->getKind());
}

CHECK_WRAPPER(CountBoundsExpr)
  return CompareInteger(E1->getKind(), E2->getKind());
}

Lexicographic::Result
Lexicographic::CompareRelativeBoundsClause(const RelativeBoundsClause *RC1,
                                           const RelativeBoundsClause *RC2) {
  bool ordered;
  Result Cmp = ComparePointers(RC1, RC2, ordered);
  if (ordered)
    return Cmp;
  assert(RC1 && RC2);

  RelativeBoundsClause::Kind RC1Kind = RC1->getClauseKind();
  Cmp = CompareInteger(RC1Kind, RC2->getClauseKind());
  if (Cmp != Result::Equal)
    return Cmp;

  switch (RC1Kind) {
    case RelativeBoundsClause::Type: {
      const RelativeTypeBoundsClause *RT1 =
        dyn_cast<RelativeTypeBoundsClause>(RC1);
      const RelativeTypeBoundsClause *RT2 =
        dyn_cast<RelativeTypeBoundsClause>(RC2);
      return CompareType(RT1->getType(), RT2->getType());
    }
    case RelativeBoundsClause::Const: {
      const RelativeConstExprBoundsClause *CC1 = 
        dyn_cast<RelativeConstExprBoundsClause>(RC1);
      const RelativeConstExprBoundsClause *CC2 = 
        dyn_cast<RelativeConstExprBoundsClause>(RC2);
      return CompareExpr(CC1->getConstExpr(), CC2->getConstExpr());
    }
    default:
      llvm_unreachable("unexpected relative bounds clause kind");
      return Result::LessThan;
  }
}

CHECK_WRAPPER(RangeBoundsExpr)
  Result Cmp = CompareInteger(E1->getKind(), E2->getKind());
  if (Cmp != Result::Equal)
    return Cmp;
  RelativeBoundsClause *RB1 = E1->getRelativeBoundsClause();
  RelativeBoundsClause *RB2 = E2->getRelativeBoundsClause();

  return CompareRelativeBoundsClause(RB1, RB2);
}

CHECK_WRAPPER(InteropTypeBoundsAnnotation)
  Result Cmp = CompareInteger(E1->getKind(), E2->getKind());
  if (Cmp != Result::Equal)
    return Cmp;
  return CompareType(E1->getType(), E2->getType());
}

CHECK_WRAPPER(PositionalParameterExpr)
  Result Cmp = CompareInteger(E1->getIndex(), E2->getIndex());
  if (Cmp != Result::Equal)
    return Cmp;
  return Result::Equal;
  // return CompareType(E1->getType(), E2->getType());
}

CHECK_WRAPPER(BoundsCastExpr)
  Result Cmp = CompareExpr(E1->getBoundsExpr(), E2->getBoundsExpr());
  if (Cmp != Result::Equal)
    return Cmp;
  return CompareType(E1->getType(), E2->getType());
}

CHECK_WRAPPER(BlockExpr)
  return Result::Equal;
}
