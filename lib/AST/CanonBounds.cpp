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
#include "llvm/Support/raw_ostream.h"
#include "clang/AST/CanonBounds.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"

using namespace clang;
using Result = Lexicographic::Result;

Lexicographic::Lexicographic(ASTContext &Ctx, EqualityRelation *EV) :
  Context(Ctx), EqualVars(EV), Trace(false) {
}

Result Lexicographic::CompareInteger(signed I1, signed I2) {
  if (I1 < I2)
    return Result::LessThan;
  else if (I1 > I2)
    return Result::GreaterThan;
  else
    return Result::Equal;
}

Result Lexicographic::CompareInteger(unsigned I1, unsigned I2) {
  if (I1 < I2)
    return Result::LessThan;
  else if (I1 > I2)
    return Result::GreaterThan;
  else
    return Result::Equal;
}

static Result CompareAPInt(const llvm::APInt &I1, const llvm::APInt &I2) {
  if (I1.slt(I2))
    return Result::LessThan;
  else if (I1.eq(I2))
    return Result::Equal;
  else
    return Result::GreaterThan;
}

static Result TranslateInt(int i) {
  if (i == 0) 
    return Result::Equal;
  else if  (i > 0)
    return Result::GreaterThan;
  else
    return Result::LessThan;
}

// \brief See if two pointers can be ordered based on nullness or
// pointer equality.   Set the parameter 'ordered' based on whether
// they can be.  If they can be ordered, return the ordering.
template<typename T> 
static Result ComparePointers(T *P1, T *P2, bool &ordered) {
  ordered = true;
  if (P1 == P2)
    return Result::Equal;
  if (P1 == nullptr && P2 != nullptr)
    return Result::LessThan;
  if (P1 != nullptr && P2 == nullptr)
    return Result::GreaterThan;
  ordered = false;
  return Result::LessThan;
}

Result
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

Result
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

  // Compare parameter variables by position and scope depth, not
  // name.
  const ParmVarDecl *Parm1 = dyn_cast<ParmVarDecl>(D1);
  const ParmVarDecl *Parm2 = dyn_cast<ParmVarDecl>(D2);
  if (Parm1 && Parm2) {
    Cmp = CompareInteger(Parm1->getFunctionScopeIndex(),
                         Parm2->getFunctionScopeIndex());
    if (Cmp != Result::Equal)
      return Cmp;

    Cmp = CompareInteger(Parm1->getFunctionScopeDepth(),
                         Parm2->getFunctionScopeDepth());
    if (Cmp != Result::Equal)
      return Cmp;

    Cmp = CompareType(Parm1->getType(), Parm2->getType());

    return Cmp;
  }

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

Result Lexicographic::CompareExpr(const Expr *E1, const Expr *E2) {
   if (Trace) {
     raw_ostream &OS = llvm::outs();
     OS << "Lexicographic comparing expressions\n";
     OS << "E1:\n";
     E1->dump(OS);
     OS << "E2:\n";
     E2->dump(OS);
   }

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
     case Expr::PredefinedExprClass: Cmp = Compare<PredefinedExpr>(E1, E2); break;
     case Expr::DeclRefExprClass: return Compare<DeclRefExpr>(E1, E2);
     case Expr::IntegerLiteralClass: return Compare<IntegerLiteral>(E1, E2);
     case Expr::FloatingLiteralClass: return Compare<FloatingLiteral>(E1, E2);
     case Expr::ImaginaryLiteralClass: break;
     case Expr::StringLiteralClass: return Compare<StringLiteral>(E1, E2);
     case Expr::CharacterLiteralClass: return Compare<CharacterLiteral>(E1, E2);
     case Expr::ParenExprClass: break;
     case Expr::UnaryOperatorClass: Cmp = Compare<UnaryOperator>(E1, E2); break;
     case Expr::OffsetOfExprClass: Cmp = Compare<OffsetOfExpr>(E1, E2); break;
     case Expr::UnaryExprOrTypeTraitExprClass:
       Cmp = Compare<UnaryExprOrTypeTraitExpr>(E1, E2); break;
     case Expr::ArraySubscriptExprClass: break;
     case Expr::CallExprClass: break;
     case Expr::MemberExprClass: Cmp = Compare<MemberExpr>(E1, E2); break;
     case Expr::BinaryOperatorClass: Cmp = Compare<BinaryOperator>(E1, E2); break;
     case Expr::CompoundAssignOperatorClass:
       Cmp = Compare<CompoundAssignOperator>(E1, E2); break;
     case Expr::BinaryConditionalOperatorClass: break;
     case Expr::ImplicitCastExprClass: Cmp = Compare<ImplicitCastExpr>(E1, E2); break;
     case Expr::CStyleCastExprClass: Cmp = Compare<CStyleCastExpr>(E1, E2); break;
     case Expr::CompoundLiteralExprClass: Cmp = Compare<CompoundLiteralExpr>(E1, E2); break;
     // TODO:
     // case: ExtVectorElementExpr
     case Expr::VAArgExprClass: break;
     case Expr::GenericSelectionExprClass: Cmp = Compare<GenericSelectionExpr>(E1, E2); break;

     // Atomic expressions.
     case Expr::AtomicExprClass: Compare<AtomicExpr>(E1, E2); break;

     // GNU Extensions.
     case Expr::AddrLabelExprClass: break;
     case Expr::StmtExprClass: break;
     case Expr::ChooseExprClass: break;
     case Expr::GNUNullExprClass: break;

     // Bounds expressions
     case Expr::NullaryBoundsExprClass: Cmp = Compare<NullaryBoundsExpr>(E1, E2); break;
     case Expr::CountBoundsExprClass: Cmp = Compare<CountBoundsExpr>(E1, E2); break;
     case Expr::RangeBoundsExprClass:  Cmp = Compare<RangeBoundsExpr>(E1, E2); break; break;
     case Expr::InteropTypeBoundsAnnotationClass: Cmp = Compare<InteropTypeBoundsAnnotation>(E1, E2); break;
     case Expr::PositionalParameterExprClass: Cmp = Compare<PositionalParameterExpr>(E1, E2); break;
     case Expr::BoundsCastExprClass: Cmp = Compare<BoundsCastExpr>(E1, E2); break;

     // Clang extensions
     case Expr::ShuffleVectorExprClass: break;
     case Expr::ConvertVectorExprClass: break;
     case Expr::BlockExprClass: Cmp = Compare<BlockExpr>(E1, E2); break;
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

   return Result::Equal;
}

Result
Lexicographic::CompareType(QualType QT1, QualType QT2) {
  QT1 = QT1.getCanonicalType();
  QT2 = QT2.getCanonicalType();
  if (QT1 == QT2)
    return Result::Equal;
  else
    // TODO: fill this in
    return Result::LessThan;
}

Result
Lexicographic::CompareImpl(const PredefinedExpr *E1, const PredefinedExpr *E2) {
  return CompareInteger(E1->getIdentType(), E2->getIdentType());
}

Result
Lexicographic::CompareImpl(const DeclRefExpr *E1, const DeclRefExpr *E2) {
  return CompareDecl(E1->getDecl(), E2->getDecl());
}

Result
Lexicographic::CompareImpl(const IntegerLiteral *E1, const IntegerLiteral *E2) {
  BuiltinType::Kind Kind1 = E1->getType()->castAs<BuiltinType>()->getKind();
  BuiltinType::Kind Kind2 = E2->getType()->castAs<BuiltinType>()->getKind();
  Result Cmp = CompareInteger(Kind1, Kind2);
  if (Cmp != Result::Equal)
    return Cmp;
  return CompareAPInt(E1->getValue(), E2->getValue());
}

Result
Lexicographic::CompareImpl(const FloatingLiteral *E1,
                           const FloatingLiteral *E2) {
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

Result
Lexicographic::CompareImpl(const StringLiteral *E1,
                           const StringLiteral *E2) {
  Result Cmp = CompareInteger(E1->getKind(), E2->getKind());
  if (Cmp != Result::Equal)
    return Cmp;
  return TranslateInt(E1->getBytes().compare(E2->getBytes()));
}

Result
Lexicographic::CompareImpl(const CharacterLiteral *E1,
                           const CharacterLiteral *E2) {
  Result Cmp = CompareInteger(E1->getKind(), E2->getKind());
  if (Cmp != Result::Equal)
    return Cmp;
  return CompareInteger(E1->getValue(), E2->getValue());
}

Result
Lexicographic::CompareImpl(const UnaryOperator *E1, const UnaryOperator *E2) {
  return CompareInteger(E1->getOpcode(), E2->getOpcode());
}

Result
Lexicographic::CompareImpl(const UnaryExprOrTypeTraitExpr *E1,
                           const UnaryExprOrTypeTraitExpr *E2) {
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

Result
Lexicographic::CompareImpl(const OffsetOfExpr *E1, const OffsetOfExpr *E2) {
  // TODO: fill this in 
  return Result::Equal;
}

Result
Lexicographic::CompareImpl(const MemberExpr *E1, const MemberExpr *E2) {
  Result Cmp = CompareInteger(E1->isArrow(), E2->isArrow());
  if (Cmp != Result::Equal)
    return Cmp;
  return CompareDecl(E1->getMemberDecl(), E2->getMemberDecl());
}

Result
Lexicographic::CompareImpl(const BinaryOperator *E1,
                           const BinaryOperator *E2) {
  return CompareInteger(E1->getOpcode(), E2->getOpcode());
}

Result
Lexicographic::CompareImpl(const CompoundAssignOperator *E1,
                           const CompoundAssignOperator *E2) {
  return CompareInteger(E1->getOpcode(), E2->getOpcode());
}

Result
Lexicographic::CompareImpl(const ImplicitCastExpr *E1,
                           const ImplicitCastExpr *E2) {
  return CompareInteger(E1->getValueKind(), E2->getValueKind());
}

Result
Lexicographic::CompareImpl(const CStyleCastExpr *E1,
                           const CStyleCastExpr *E2) {
  return CompareType(E1->getType(), E2->getType());
}

Result
Lexicographic::CompareImpl(const CompoundLiteralExpr *E1,
                           const CompoundLiteralExpr *E2) {
  return CompareInteger(E1->isFileScope(), E2->isFileScope());
}

Result
Lexicographic::CompareImpl(const GenericSelectionExpr *E1,
                           const GenericSelectionExpr *E2) {
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

Result
Lexicographic::CompareImpl(const AtomicExpr *E1, const AtomicExpr *E2) {
  return CompareInteger(E1->getOp(), E2->getOp());
}

Result
Lexicographic::CompareImpl(const NullaryBoundsExpr *E1,
                           const NullaryBoundsExpr *E2) {
  return CompareInteger(E1->getKind(), E2->getKind());
}

Result
Lexicographic::CompareImpl(const CountBoundsExpr *E1,
                           const CountBoundsExpr *E2) {
  return CompareInteger(E1->getKind(), E2->getKind());
}

Result
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

Result
Lexicographic::CompareImpl(const RangeBoundsExpr *E1,
                           const RangeBoundsExpr *E2) {
  Result Cmp = CompareInteger(E1->getKind(), E2->getKind());
  if (Cmp != Result::Equal)
    return Cmp;
  RelativeBoundsClause *RB1 = E1->getRelativeBoundsClause();
  RelativeBoundsClause *RB2 = E2->getRelativeBoundsClause();

  return CompareRelativeBoundsClause(RB1, RB2);
}

Result
Lexicographic::CompareImpl(const InteropTypeBoundsAnnotation *E1,
                           const InteropTypeBoundsAnnotation *E2) {
  Result Cmp = CompareInteger(E1->getKind(), E2->getKind());
  if (Cmp != Result::Equal)
    return Cmp;
  return CompareType(E1->getType(), E2->getType());
}

Result
Lexicographic::CompareImpl(const PositionalParameterExpr *E1,
                           const PositionalParameterExpr *E2) {
  Result Cmp = CompareInteger(E1->getIndex(), E2->getIndex());
  if (Cmp != Result::Equal)
    return Cmp;
  return CompareType(E1->getType(), E2->getType());
}

Result
Lexicographic::CompareImpl(const BoundsCastExpr *E1,
                           const BoundsCastExpr *E2) {
  Result Cmp = CompareExpr(E1->getBoundsExpr(), E2->getBoundsExpr());
  if (Cmp != Result::Equal)
    return Cmp;
  return CompareType(E1->getType(), E2->getType());
}

Result
Lexicographic::CompareImpl(const BlockExpr *E1, const BlockExpr *E2) {
  return Result::Equal;
}
