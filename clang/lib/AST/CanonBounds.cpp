//===- CanonBounds.cpp: comparison and canonicalization for bounds exprs -===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
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
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/PreorderAST.h"
#include "clang/AST/Stmt.h"

using namespace clang;
using Result = Lexicographic::Result;

namespace {
  // Return true if this cast preserve the bits of the value,
  // false otherwise.
  static bool IsValuePreserving(CastKind CK) {
    switch (CK) {
      case CK_BitCast:
      case CK_LValueBitCast:
      case CK_NoOp:
      case CK_ArrayToPointerDecay:
      case CK_FunctionToPointerDecay:
      case CK_NullToPointer:
        return true;
      default:
        return false;
    }
  }

  static bool isPointerToArrayType(QualType Ty) {
    if (const PointerType *T = Ty->getAs<PointerType>())
      return T->getPointeeType()->isArrayType();
    else
      return false;
  }
}

Lexicographic::Lexicographic(ASTContext &Ctx, EquivExprSets *EquivExprs) :
  Context(Ctx), EquivExprs(EquivExprs), Trace(false) {
}

Result Lexicographic::CompareInteger(signed I1, signed I2) const {
  if (I1 < I2)
    return Result::LessThan;
  else if (I1 > I2)
    return Result::GreaterThan;
  else
    return Result::Equal;
}

Result Lexicographic::CompareInteger(unsigned I1, unsigned I2) const {
  if (I1 < I2)
    return Result::LessThan;
  else if (I1 > I2)
    return Result::GreaterThan;
  else
    return Result::Equal;
}

Result Lexicographic::CompareAPInt(const llvm::APInt &I1,
                                   const llvm::APInt &I2) const {
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
Lexicographic::CompareScope(const DeclContext *DC1, const DeclContext *DC2) const {
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
Lexicographic::CompareDecl(const NamedDecl *D1Arg, const NamedDecl *D2Arg) const {
  const NamedDecl *D1 = dyn_cast<NamedDecl>(D1Arg->getCanonicalDecl());
  const NamedDecl *D2 = dyn_cast<NamedDecl>(D2Arg->getCanonicalDecl());
  if (D1 == D2) 
    return Result::Equal;

  if (!D1 || !D2) {
    assert(false && "unexpected cast failure");
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
  Cmp = ComparePointers(Name1, Name2, ordered);
  if (ordered && Cmp != Result::Equal)
    return Cmp;
  assert((Name1 != nullptr) == (Name2 != nullptr));

  if (Name1) {
    Cmp = TranslateInt(Name1->getName().compare(Name2->getName()));
    if (Cmp != Result::Equal)
      return Cmp;
  }

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

bool Lexicographic::CompareExprSemantically(const Expr *Arg1,
                                            const Expr *Arg2) {
   // Compare Arg1 and Arg2 semantically. If we hit an error during comparison
   // simply fallback to CompareExpr which compares two expressions
   // structurally.

   Expr *E1 = const_cast<Expr *>(Arg1);
   Expr *E2 = const_cast<Expr *>(Arg2);

   PreorderAST P1(Context, E1);
   P1.Normalize();
   if (P1.GetError()) {
     P1.Cleanup();
     return CompareExpr(Arg1, Arg2) == Result::Equal;
   }

   PreorderAST P2(Context, E2);
   P2.Normalize();
   if (P2.GetError()) {
     P2.Cleanup();
     return CompareExpr(Arg1, Arg2) == Result::Equal;
   }

  bool Res = P1.IsEqual(P2);
  P1.Cleanup();
  P2.Cleanup();
  return Res;
}

Result Lexicographic::CompareExpr(const Expr *Arg1, const Expr *Arg2) {
   if (Trace) {
     raw_ostream &OS = llvm::outs();
     OS << "Lexicographic comparing expressions\n";
     OS << "E1:\n";
     Arg1->dump(OS);
     OS << "E2:\n";
     Arg2->dump(OS);
   }

   Expr *E1 = const_cast<Expr *>(Arg1);
   Expr *E2 = const_cast<Expr *>(Arg2);

  E1 = IgnoreValuePreservingOperations(Context, E1);
  E2 = IgnoreValuePreservingOperations(Context, E2);

  if (E1 == E2)
    return Result::Equal;

  // The use of an expression temporary is equal to the
  // value of the binding expression.
  if (BoundsValueExpr *BV1 = dyn_cast<BoundsValueExpr>(E1)) {
    CHKCBindTemporaryExpr *Binding = BV1->getTemporaryBinding();
    if (Binding == E2)
      return Result::Equal;

    if (Binding)
      if (CompareExpr(Binding->getSubExpr(), E2) == Result::Equal)
        return Result::Equal;
  }
  
  if (BoundsValueExpr *BV2 = dyn_cast<BoundsValueExpr>(E2)) {
    CHKCBindTemporaryExpr *Binding = BV2->getTemporaryBinding();
    if (Binding == E1)
      return Result::Equal;

    if (Binding)
      if (CompareExpr(Binding->getSubExpr(), E1) == Result::Equal)
        return Result::Equal;
  }

   // Compare expressions structurally, recursively invoking
   // comparison for subcomponents.  If that fails, consult
   // EquivExprs to see if the expressions are considered
   // equivalent.
   // - Treat different kinds of casts (implicit/explicit) as
   //   equivalent if the operation kinds are the same.
   Stmt::StmtClass E1Kind = E1->getStmtClass();
   Stmt::StmtClass E2Kind = E2->getStmtClass();
   Result Cmp = CompareInteger(E1Kind, E2Kind);
   if (Cmp != Result::Equal && !(isa<CastExpr>(E1) && isa<CastExpr>(E2)))
     return CheckEquivExprs(Cmp, E1, E2);

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
     case Expr::ConditionalOperatorClass: break;
     case Expr::ImplicitCastExprClass: Cmp = Compare<CastExpr>(E1, E2); break;
     case Expr::CStyleCastExprClass: Cmp = Compare<CastExpr>(E1, E2); break;
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
     case Expr::InteropTypeExprClass: Cmp = Compare<InteropTypeExpr>(E1, E2); break;
     case Expr::PositionalParameterExprClass: Cmp = Compare<PositionalParameterExpr>(E1, E2); break;
     case Expr::BoundsCastExprClass: Cmp = Compare<BoundsCastExpr>(E1, E2); break;
     case Expr::BoundsValueExprClass: Cmp = Compare<BoundsValueExpr>(E1, E2); break;
     // Binding of a temporary to the result of an expression.  These are
     // equal if their child expressions are equal.
     case Expr::CHKCBindTemporaryExprClass: break;

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
     return CheckEquivExprs(Cmp, E1, E2);

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
       return CheckEquivExprs(Cmp, E1, E2);
      }
     assert(E1Child && E2Child);
     const Expr *E1ChildExpr = dyn_cast<Expr>(E1Child);
     const Expr *E2ChildExpr = dyn_cast<Expr>(E2Child);
     assert(E1ChildExpr && E2ChildExpr && "dyn_cast failed");
     if (E1ChildExpr && E2ChildExpr) {
       Cmp = CompareExpr(E1ChildExpr, E2ChildExpr);
       if (Cmp != Result::Equal)
         return CheckEquivExprs(Cmp, E1, E2);
       // TODO: Github issue #475.  We need to sort out typing rules
       // for uses of variables with bounds-safe interfaces in bounds
       // expressions.  Then we can likely replace this with CompareType.
       // TODO: consider treating operations whose types differ
       // but that still produce the same value as being the
       // same.  For example:
       // - Pointer arithmetic where the pointer referent types are the same
       //   size, checkedness is the same, and the integer types are the
       //    same size/signedness.
       
       // Bounds expressions don't have types.
       if (!isa<BoundsExpr>(E1ChildExpr))
         Cmp = CompareTypeIgnoreCheckedness(E1ChildExpr->getType(), E2ChildExpr->getType());

       if (Cmp != Result::Equal)
         return CheckEquivExprs(Cmp, E1, E2);
     } else
       // assert fired - return something
       return Result::LessThan;
   }

   // Make sure the number of children was equal.  If E1 has
   // fewer children than E2, make it less than E2.  If it has more
   // children, make it greater than E2.
   if (I1 == E1->child_end() && I2 != E2->child_end())
     return CheckEquivExprs(Result::LessThan, E1, E2);

   if (I1 != E1->child_end() && I2 == E2->child_end())
     return CheckEquivExprs(Result::GreaterThan, E1, E2);

   return Result::Equal;
}

// See if the expressions are considered equivalent using the list of lists
// of equivalent expressions.
Result Lexicographic::CheckEquivExprs(Result Current, const Expr *E1, const Expr *E2) {
  if (!EquivExprs)
    return Current;

  // Important: compare expressions for equivalence without using equality facts.
  // This keep the asymptotic complexity of this method linear in the number of AST nodes
  // for E1, E2, and EquivExprs.  It also avoid the complexities of having to avoid
  // infinite recursions.
  Lexicographic SimpleComparer = Lexicographic(Context, nullptr);
  // Iterate over the list of sets.
  for (auto OuterList = EquivExprs->begin(); OuterList != EquivExprs->end();
       ++OuterList) {
    bool LHSAppears = false;
    bool RHSAppears = false;
    // See if the LHS expression appears in the set.
    SmallVector<Expr *, 4> ExprList = *OuterList;
    for (auto InnerList = ExprList.begin(); InnerList != ExprList.end(); ++InnerList) {
      if (SimpleComparer.CompareExpr(E1, *InnerList)  == Result::Equal) {
        LHSAppears = true;
        break;
      }
    }
    if (!LHSAppears)
      continue;

    // See if the RHS expression appears in the set.
    for (auto InnerList = ExprList.begin(); InnerList != ExprList.end(); ++InnerList) {
      if (SimpleComparer.CompareExpr(E2, *InnerList)  == Result::Equal) {
        RHSAppears = true;
        break;
      }
    }

    // If both appear, consider them equivalent.
    if (RHSAppears)
      return Result::Equal;
  }

  return Current;
 }


Result
Lexicographic::CompareTypeLexicographically(QualType QT1, QualType QT2) const {
  assert(QT1 != QT2);

  std::string S1 = QT1.getAsString();
  std::string S2 = QT2.getAsString();
  if (S1.compare(S2) < 0)
    return Result::LessThan;
  return Result::GreaterThan;
}

Result
Lexicographic::CompareType(QualType QT1, QualType QT2) const {
  QT1 = QT1.getCanonicalType();
  QT2 = QT2.getCanonicalType();
  if (QT1 == QT2)
    return Result::Equal;
  return CompareTypeLexicographically(QT1, QT2);
}

Result
Lexicographic::CompareTypeIgnoreCheckedness(QualType QT1, QualType QT2) const {
  QT1 = QT1.getCanonicalType();
  QT2 = QT2.getCanonicalType();
  if (Context.isEqualIgnoringChecked(QT1, QT2))
    return Result::Equal;
  return CompareTypeLexicographically(QT1, QT2);
}

Result
Lexicographic::CompareImpl(const PredefinedExpr *E1, const PredefinedExpr *E2) {
  return CompareInteger(E1->getIdentKind(), E2->getIdentKind());
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
Lexicographic::CompareImpl(const CastExpr *E1,
                           const CastExpr *E2) {
  Result Cmp = CompareInteger(E1->getCastKind(), E2->getCastKind());
  if (Cmp != Result::Equal)
    return Cmp;
  // TODO: Github issue #475.  We need to sort out typing rules
  // for uses of variables with bounds-safe interfaces in bounds
  // expressions.  Then we can likely replace this with CompareType.
  return CompareTypeIgnoreCheckedness(E1->getType(), E2->getType());
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

  std::vector<QualType> E1AssocTypes;
  std::vector<const Expr *> E1AssocExprs;
  for (const auto &E1Assoc : E1->associations()) {
    E1AssocTypes.push_back(E1Assoc.getType());
    E1AssocExprs.push_back(E1Assoc.getAssociationExpr());
  }

  unsigned i = 0;
  for (const auto &E2Assoc : E2->associations()) {
    Cmp = CompareType(E1AssocTypes[i], E2Assoc.getType());
    if (Cmp != Result::Equal)
      return Cmp;
    Cmp = CompareExpr(E1AssocExprs[i], E2Assoc.getAssociationExpr());
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
Lexicographic::CompareImpl(const InteropTypeExpr *E1,
                           const InteropTypeExpr *E2) {
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
  Result Cmp = CompareInteger(E1->getCastKind(), E2->getCastKind());
  if (Cmp != Result::Equal)
    return Cmp;

  Cmp = CompareExpr(E1->getBoundsExpr(), E2->getBoundsExpr());
  if (Cmp != Result::Equal)
    return Cmp;

  return CompareType(E1->getType(), E2->getType());
}

Result
Lexicographic::CompareImpl(const CHKCBindTemporaryExpr *E1,
                           const CHKCBindTemporaryExpr *E2) {
  bool ordered;
  Result Cmp = ComparePointers(E1, E2, ordered);
  if (!ordered) {
    // Order binding expressions by the source location of
    // the source-level expression.
    if (E1->getSubExpr()->getBeginLoc() <
        E2->getSubExpr()->getBeginLoc())
      Cmp = Result::LessThan;
    else
      Cmp = Result::GreaterThan;
   }
   return Cmp;
}

Result
Lexicographic::CompareImpl(const BoundsValueExpr *E1,
                           const BoundsValueExpr *E2) {
  Result Cmp = CompareInteger(E1->getKind(), E2->getKind());
  if (Cmp != Result::Equal)
    return Cmp;

  // The type doesn't matter - the value is the same no matter what the type.

  // If these are uses of an expression temporary, check that the binding
  // expression is the same.
  if (E1->getKind() == BoundsValueExpr::Kind::Temporary)
     Cmp = CompareImpl(E1->getTemporaryBinding(), E2->getTemporaryBinding());
  return Cmp;
}

Result
Lexicographic::CompareImpl(const BlockExpr *E1, const BlockExpr *E2) {
  return Result::Equal;
}

// Ignore operations that don't change runtime values: parens, some cast operations,
// array/function address-of and dereference operators, and address-of/dereference
// operators that cancel (&* and *&).
//
// The code for casts is adapted from Expr::IgnoreNoopCasts, which seems like doesn't
// do enough filtering (it'll ignore LValueToRValue casts for example).
// TODO: reconcile with CheckValuePreservingCast
Expr *Lexicographic::IgnoreValuePreservingOperations(ASTContext &Ctx,
                                                     Expr *E) {
  while (true) {
    E = E->IgnoreParens();

    if (CastExpr *P = dyn_cast<CastExpr>(E)) {
      CastKind CK = P->getCastKind();
      Expr *SE = P->getSubExpr();
      if (IsValuePreserving(CK)) {
        E = SE;
        continue;
      }

      // Ignore integer <-> casts that are of the same width, ptr<->ptr
      // and ptr<->int casts of the same width.
      if (CK == CK_IntegralToPointer || CK == CK_PointerToIntegral ||
          CK == CK_IntegralCast) {
        if (Ctx.hasSameUnqualifiedType(E->getType(), SE->getType())) {
          E = SE;
          continue;
        }

        if ((E->getType()->isPointerType() ||
              E->getType()->isIntegralType(Ctx)) &&
              (SE->getType()->isPointerType() ||
              SE->getType()->isIntegralType(Ctx)) &&
            Ctx.getTypeSize(E->getType()) == Ctx.getTypeSize(SE->getType())) {
          E = SE;
          continue;
        }
      }
    } else if (const UnaryOperator *UO = dyn_cast<UnaryOperator>(E)) {
      QualType ETy = UO->getType();
      Expr *SE = UO->getSubExpr();
      QualType SETy = SE->getType();

      UnaryOperator::Opcode Op = UO->getOpcode();
      if (Op == UO_Deref) {
        // This may be more conservative than necessary.
        bool between_functions = ETy->isFunctionType() && SETy->isFunctionPointerType();
        bool between_arrays = ETy->isArrayType() && isPointerToArrayType(SETy);
        if (between_functions || between_arrays) {
          E = SE;
          continue;
        }

        // handle *&e, which reduces to e.
        if (const UnaryOperator *Child =
            dyn_cast<UnaryOperator>(SE->IgnoreParens())) {
          if (Child->getOpcode() == UO_AddrOf) {
            E = Child->getSubExpr();
            continue;
          }
        }

      } else if (Op == UO_AddrOf) {
        // This may be more conservative than necessary.
        bool between_functions = ETy->isFunctionPointerType() && SETy->isFunctionType();
        bool between_arrays = isPointerToArrayType(ETy) && SETy->isArrayType();
        if (between_functions || between_arrays) {
          E = SE;
          continue;
        }

        // handle &*e, which reduces to e.
        if (const UnaryOperator *Child =
            dyn_cast<UnaryOperator>(SE->IgnoreParens())) {
          if (Child->getOpcode() == UO_Deref) {
            E = Child->getSubExpr();
            continue;
          }
        }
      }
    }

    return E;
  }
}
