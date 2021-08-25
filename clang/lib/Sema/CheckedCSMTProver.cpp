//===== CheckedCSMTProver.cpp - Manager class for available facts ====//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
// This file implements a SMT prover for CheckedC.
//===---------------------------------------------------------------------===//

#include "clang/Sema/CheckedCSMTProver.h"
#include "clang/Sema/FactUtils.h"

// TODO: check the upper bounds checking

using namespace clang;
using namespace llvm;

namespace clang {
  class SMTExprBuilder : public RecursiveASTVisitor<SMTExprBuilder> {
  private:
    CheckedCSMTProver *P;
    Sema &S;
    ASTContext &Ctx;
    llvm::SMTSolverRef Solver;
    llvm::raw_ostream &OS;

    bool IsUnsupported;

  public:
    SMTExprBuilder(CheckedCSMTProver *P) :
      P(P), S(P->S), Ctx(P->Ctx), Solver(P->Solver),
      OS(llvm::outs()), IsUnsupported(false) {}

    void Reset() { IsUnsupported = false; };
    bool IsTraverseOK() const { return !IsUnsupported; }


    bool shouldTraversePostOrder() const { return true; }

    bool TraverseRangeBoundsExpr(RangeBoundsExpr *RB) { return true; }

    bool TraverseBoundsValueExpr(BoundsValueExpr *E) { return true; }

    // TODO: Danger! Should not define this function.
    // The visitor will be broken by unknown reasons.
    // bool TraverseParenExpr(ParenExpr *E) { return true; }

    bool VisitImplicitCastExpr(ImplicitCastExpr *CE) {
      CastKind CK = CE->getCastKind();

      SMTExprRef SubSym = P->GetCachedSym(CE->getSubExpr());
      if (!SubSym) {
        IsUnsupported = true;
        return false;
      }

      switch (CK) {
      case CastKind::CK_LValueToRValue:
        P->SetCachedSym(CE, SubSym);
        break;
      case CastKind::CK_IntegralCast: 
        {
          SMTExprRef FromSym = SubSym;
          QualType FromTy = CE->getSubExpr()->getType();
          QualType ToTy = CE->getType();

          // OS << "\nFromSym:";
          // FromSym->dump();
          // OS << "\nCasting AST:";
          // CE->dump();

          SMTExprRef ToSym = CastSymExprToType(FromSym, FromTy, ToTy);
          P->SetCachedSym(CE, ToSym);
          break;
        }
      default:
        llvm::errs() << CastExpr::getCastKindName(CK) << "\n";
        llvm_unreachable("not support cast");
      }

      return true;
    }

    // TODO: handle _Nt_array_ptr<int> p : byte_count(d) = g(a);
    // RangeBoundsExpr 'NULL TYPE'
    // |-CStyleCastExpr '_Array_ptr<char>' <BitCast>
    // | `-BoundsValueExpr '_Nt_array_ptr<int>' _BoundTemporary  0x5651a7b0fe78
    // `-BinaryOperator '_Array_ptr<char>' '+'
    //   |-CStyleCastExpr '_Array_ptr<char>' <BitCast>
    //   | `-BoundsValueExpr '_Nt_array_ptr<int>' _BoundTemporary  0x5651a7b0fe78
    //   `-ImplicitCastExpr 'int' <LValueToRValue>
    //     `-DeclRefExpr 'int' lvalue ParmVar 0x5651a7aafa60 'a' 'int'
    bool VisitCStyleCastExpr(CStyleCastExpr *CE) {
      IsUnsupported = true;
      return false;
    }

    // Refer to
    //   (SMTConv.h)
    //     fromCast, doTypeConversion
    //   (SemaExpr.cpp)
    //     Sema::handleIntegerConversion, Sema::doIntegralCast
    //   (Sema.cpp)   
    //     Sema::ImpCastExprToType
    SMTExprRef CastSymExprToType(SMTExprRef FromSym, QualType FromTy, QualType ToTy) {
      if (FromTy == ToTy)
        return FromSym;

      bool FromSigned = FromTy->hasSignedIntegerRepresentation();
      // bool ToSigned = ToTy->hasSignedIntegerRepresentation();

      uint64_t FromBitWidth = Ctx.getTypeSize(FromTy);
      uint64_t ToBitWidth = Ctx.getTypeSize(ToTy);

      if (ToBitWidth > FromBitWidth)
        return FromSigned
                  ? Solver->mkBVSignExt(ToBitWidth - FromBitWidth, FromSym)
                  : Solver->mkBVZeroExt(ToBitWidth - FromBitWidth, FromSym);

      // Keeping the lower bits
      if (ToBitWidth < FromBitWidth)
        return Solver->mkBVExtract(ToBitWidth - 1, 0, FromSym);

      return FromSym;
    }

    // {c
    //   int order = S.Context.getIntegerTypeOrder(FromTy, ToTy);
    //   if (!order)
    //     return FromSym;
    // }

    bool VisitBinaryOperator(BinaryOperator *BO) {

      Expr *LHS = BO->getLHS()->IgnoreParens();
      Expr *RHS = BO->getRHS()->IgnoreParens();

      SMTExprRef LHSSym = P->GetCachedSym(LHS);
      if (!LHSSym) {
        IsUnsupported = true;
        return false;
      }

      SMTExprRef RHSSym = P->GetCachedSym(RHS);
      if (!RHSSym) {
        IsUnsupported = true;
        return false;
      }

      // OS << "VisitBinaryOperator" << "\n";
      // BO->dump();
      // OS << "LHS (after P): " << LHS << "\n"
      //    << "RHS (after P): " << RHS << "\n";
      // OS << "\nLHS Sym: ";
      // LHSSym->dump();
      // OS << "\nRHS Sym: ";
      // RHSSym->dump();

      BinaryOperator::Opcode op = BO->getOpcode();

      SMTExprRef SymBinop;

      if (BO->isLogicalOp()) {
        switch (op) {
        case BinaryOperatorKind::BO_LAnd: 
          SymBinop = Solver->mkAnd(LHSSym, RHSSym);
          break;
        default:
          llvm_unreachable("impossible logic binop");
        }
      }
      else if (BO->isComparisonOp() || BO->isMultiplicativeOp() || BO->isAdditiveOp()) {
        switch (op) {
        // TODO: check signed or not.
        // ComparisonOp
        case BinaryOperatorKind::BO_LT: 
          SymBinop = Solver->mkBVSlt(LHSSym, RHSSym);
          break;
        case BinaryOperatorKind::BO_GT: 
          SymBinop = Solver->mkBVSgt(LHSSym, RHSSym);
          break;
        case BinaryOperatorKind::BO_LE: 
          SymBinop = Solver->mkBVSle(LHSSym, RHSSym);
          break;
        case BinaryOperatorKind::BO_GE: 
          SymBinop = Solver->mkBVSge(LHSSym, RHSSym);
          break;
        case BinaryOperatorKind::BO_EQ: 
          SymBinop = Solver->mkEqual(LHSSym, RHSSym);
          break;
        case BinaryOperatorKind::BO_NE: 
          SymBinop = Solver->mkNot(Solver->mkEqual(LHSSym, RHSSym));
          break;
        // Arithmatic (MultiplicativeOp, AdditiveOp)
        case BinaryOperatorKind::BO_Mul:
          SymBinop = Solver->mkBVMul(LHSSym, RHSSym);
          break;          
        case BinaryOperatorKind::BO_Div:
          SymBinop = Solver->mkBVSDiv(LHSSym, RHSSym);
          break;
        // TOOD: check the signedness
        case BinaryOperatorKind::BO_Rem:
          SymBinop = Solver->mkBVSRem(LHSSym, RHSSym);
          break;

        case BinaryOperatorKind::BO_Add:
          [[fallthrough]]
        case BinaryOperatorKind::BO_Sub:
          // TODO: check the types rather than the sorts.
          if (Solver->getSort(LHSSym) == Solver->getSort(RHSSym))
            SymBinop = Solver->mkBVAdd(LHSSym, RHSSym);
          else if (LHS->getType()->isAnyPointerType() && RHS->getType()->isIntegerType()) {
            // See SimpleSValBuilder::evalBinOpLN in SimpleSValBuilder.cpp
            uint64_t PointerWidth = Ctx.getTypeSize(LHS->getType());

            SMTExprRef RHSExtSym = CastSymExprToType(RHSSym, RHS->getType(), LHS->getType());
          
            llvm::APSInt Scale(PointerWidth, /* isUnsigned */ true);
            QualType pointeeTy = LHS->getType()->getPointeeType();
            Scale = Ctx.getTypeSizeInChars(pointeeTy).getQuantity();
            SMTExprRef ScaleSym = Solver->mkBitvector(Scale, PointerWidth);

            SMTExprRef OffsetSym = Solver->mkBVMul(RHSExtSym, ScaleSym);

            SMTExprRef Result = (op == BinaryOperatorKind::BO_Add)
                              ? Solver->mkBVAdd(LHSSym, OffsetSym)
                              : Solver->mkBVSub(LHSSym, OffsetSym);

            SymBinop = Result;
          } else {
            llvm_unreachable("not yet handle mixed type addition");
          }
          break;
        // else
        // case BinaryOperatorKind::BO_Cmp:
        default:
          llvm_unreachable("impossible binop");
        }
      }
      else
        llvm_unreachable("non support binop");

      P->SetCachedSym(BO, SymBinop);
      
      return true;
    }

    bool VisitDeclRefExpr(DeclRefExpr *RE) {
      if (const VarDecl *VD = dyn_cast<VarDecl>(RE->getDecl())) {
        SMTExprRef DeclSym = P->VarDeclToSym(VD);
        P->SetCachedSym(RE, DeclSym);
        return true;
      }

      return false;
    }

    bool VisitIntegerLiteral(IntegerLiteral *IL) {
      const APSInt Num(IL->getValue());
      uint64_t BitWidth = Ctx.getTypeSize(IL->getType());

      SMTExprRef SymNum = Solver->mkBitvector(Num, BitWidth);

      P->SetCachedSym(IL, SymNum);

      return true;
    }

    // The below functions works as public.
    // If the input contains the unsupport expressions,
    //   the result will be nullptr
    SMTExprRef ExprToSym(Expr *E) {
      assert(!IsUnsupported);
      SMTExprRef Result = P->GetCachedSym(E);

      if (!Result) {
        TraverseStmt(E);
        // TraverseStmt has the side-effect to save the cache if E is valid.

        if (!IsUnsupported)
          Result = P->GetCachedSym(E);
      }
      
      return Result;
    }

    SMTExprRef BoundsToSym(VarDecl *Var, BoundsExpr *B) {
      RangeBoundsExpr *RBE = dyn_cast_or_null<RangeBoundsExpr>(B);

      Expr *Lower = RBE->getLowerExpr();
      Expr *Upper = RBE->getUpperExpr();
      
      // OS << "Lower: \n";
      // Lower->dump();
      // OS << "Upper: \n";
      // Upper->dump();

      TraverseStmt(Lower);
      if (IsUnsupported)
        return nullptr;

      TraverseStmt(Upper);
      if (IsUnsupported)
        return nullptr;

      SMTExprRef SymLower = P->GetCachedSym(Lower);
      assert(SymLower);

      SMTExprRef SymUpper = P->GetCachedSym(Upper);
      assert(SymUpper);

      SMTExprRef SymV = P->VarDeclToSym(Var);

      // OS << "SL: " << SymLower << "\n";
      // SymLower->dump();

      // OS << "SU: " << SymUpper << "\n";
      // SymUpper->dump();

      // SymV->dump();

      auto C = Solver->mkAnd(
        Solver->mkBVSle(SymLower, SymV),
        Solver->mkBVSle(SymV, SymUpper)
      );

      return C;
    }
  };
}

CheckedCSMTProver::~CheckedCSMTProver() {}

CheckedCSMTProver::ProofResult CheckedCSMTProver::ProveBounds(
                                       const BoundsExpr *DeclaredBounds,
                                       const BoundsExpr *SrcBounds,
                                       EquivExprSets *EquivExprs,
                                       AbstractFactListTy &Facts) {

  // Step 1 - Get the range bounds, return if fails.
  // TODO: it's OK that SrcRBE is in other form because we can also prove the declared bounds.
  OS << "Prove (target raw): \n";
  // SrcBounds->dump();
  // OS << " => ";
  // DeclaredBounds->dump();
  
  const RangeBoundsExpr *DeclaredRBE = nullptr;
  const RangeBoundsExpr *SrcRBE = nullptr;

  if (!GetValidRangeBounds(DeclaredBounds, SrcBounds, &DeclaredRBE, &SrcRBE))
    return ProofResult::Maybe;

  // Step 2 - Build the constraints for bounds safety.
  Solver->reset();
  
  SMTExprRef SymSafeBounds = BuildSafeBoundsConstraints(DeclaredRBE, SrcRBE);

  if (!SymSafeBounds)
    return ProofResult::Maybe;

  // Step 3 - Build the constraints from EquivExprs.
  OS << "\n\nEquivExprs: \n";

  // It won't fail. By default it will be a harmless truth.
  SMTExprRef SymEquivExprs = BuildEquivExprsConstraints(EquivExprs);

  // Step 4 - Build the constraints from the available facts.
  OS << "\n\nFacts: \n";

  SMTExprRef SymFacts = BuildAvailableFactConstraints(Facts);

  // Step 5 - Build the (negation of) target checking constraint.

  SMTExprRef UnsatTarget = BuildNegationOfTarget(SymSafeBounds, SymEquivExprs, SymFacts);
  
  // Step 6 - Handle the checking result

  ProofResult Result = CheckSafety(UnsatTarget);

  return Result;
}

bool CheckedCSMTProver::GetValidRangeBounds(
                              const BoundsExpr *DeclaredBounds,
                              const BoundsExpr *SrcBounds,
                              const RangeBoundsExpr ** DeclaredRBEPtr,
                              const RangeBoundsExpr ** SrcRBEPtr) {
  // DeclaredBounds->dump();
  // SrcBounds->dump();

  *DeclaredRBEPtr = cast<RangeBoundsExpr>(DeclaredBounds);
  *SrcRBEPtr = cast<RangeBoundsExpr>(SrcBounds);

  // DeclaredRBE->dump();
  // SrcRBE->dump();

  return ((*DeclaredRBEPtr) && (*SrcRBEPtr));
}

SMTExprRef CheckedCSMTProver::BuildSafeBoundsConstraints(
                                    const RangeBoundsExpr *DeclaredRBE, 
                                    const RangeBoundsExpr *SrcRBE) {
  SMTExprBuilder Builder(this);
  Expr *DeclaredLower = DeclaredRBE->getLowerExpr();
  Expr *DeclaredUpper = DeclaredRBE->getUpperExpr();

  Expr *SrcLower = SrcRBE->getLowerExpr();
  Expr *SrcUpper = SrcRBE->getUpperExpr();

  OS << "Prove (target): \n";
  SrcRBE->dumpPretty(Ctx);
  OS << " => ";
  DeclaredRBE->dumpPretty(Ctx);

  // TODO: use Option<SMTExprRef> to simplify the error handling code
  SMTExprRef SymSrcLower = Builder.ExprToSym(SrcLower);
  if (!Builder.IsTraverseOK())
    return nullptr;

  SMTExprRef SymSrcUpper = Builder.ExprToSym(SrcUpper);
  if (!Builder.IsTraverseOK())
    return nullptr;

  SMTExprRef SymDeclaredLower = Builder.ExprToSym(DeclaredLower);
  if (!Builder.IsTraverseOK())
    return nullptr;

  SMTExprRef SymDeclaredUpper = Builder.ExprToSym(DeclaredUpper);
  if (!Builder.IsTraverseOK())
    return nullptr;

  SMTExprRef IsLowerSafe = Solver->mkBVSle(SymSrcLower, SymDeclaredLower);
  SMTExprRef IsUpperSafe = Solver->mkBVSle(SymDeclaredUpper, SymSrcUpper);
  SMTExprRef SymSafeBounds = Solver->mkAnd(IsLowerSafe, IsUpperSafe);

  OS << "\n\nProve (symbolic): \n";
  SymSafeBounds->dump();

  return SymSafeBounds;
}

SMTExprRef CheckedCSMTProver::BuildEquivExprsConstraints(EquivExprSets *EquivExprs) {
  SMTExprBuilder Builder(this);
  SMTExprRef SymEquivExprs = Solver->mkBoolean(true);

  for (ExprSetTy &EqExprs : (*EquivExprs)) {
    if (EqExprs.size() <= 1)
      continue;

    SMTExprRef PrevExprSym = nullptr;

    for (Expr *EqExpr : EqExprs) {
      // This function is very tolerant on the builder's failure,
      //    just ignoring the unsurpported expressions.
      Builder.Reset();

      // EqExpr->dumpPretty(Ctx);
      // OS << ", ";

      if (PrevExprSym) {
        SMTExprRef ThisExprSym = Builder.ExprToSym(EqExpr);
        if (!ThisExprSym)
          continue;

        SMTExprRef SymEq = Solver->mkEqual(PrevExprSym, ThisExprSym);
        SymEquivExprs = Solver->mkAnd(SymEquivExprs, SymEq);

        PrevExprSym = ThisExprSym;

      } else {
        PrevExprSym = Builder.ExprToSym(EqExpr);
        if (!PrevExprSym)
          continue;
      }
    }
  }

  return SymEquivExprs;
}

SMTExprRef CheckedCSMTProver::BuildAvailableFactConstraints(AbstractFactListTy &Facts) {
  SMTExprRef SymFacts = Solver->mkBoolean(true);

  for (AbstractFact *AFact : Facts) {
    SMTExprRef SymExpr = FactToSym(AFact);
    if (!SymExpr)
      continue;
    SymFacts = Solver->mkAnd(SymFacts, SymExpr);
  }

  return SymFacts;
}

SMTExprRef CheckedCSMTProver::BuildNegationOfTarget(SMTExprRef SymSafeBounds,
                                                    SMTExprRef SymEquivExprs,
                                                    SMTExprRef SymFacts) {
  // NOT (Facts && EquivExprs => BounsSafe)
  //  == Facts && EquivExprs && (NOT BounsSafe)
  SMTExprRef UnsatTarget = 
    Solver->mkAnd(
      Solver->mkNot(SymSafeBounds),
      Solver->mkAnd(
        SymEquivExprs, 
        SymFacts
      )
    );
  
  return UnsatTarget;
}

CheckedCSMTProver::ProofResult CheckedCSMTProver::CheckSafety(SMTExprRef UnsatTarget) {
  Solver->addConstraint(UnsatTarget);

  OS << "\nConstraints in solver: \n";
  Solver->print(OS);

  Optional<bool> result = Solver->check();

  if (result.hasValue()) {
    if (result.getValue()) {
      // APSInt res_int(Ctx.getTargetInfo().getIntWidth(), true);
      // const bool res_bool = Solver->getInterpretation(x, res_int);
      // OS << "Model: x = " << res_int << "\n";

      // const bool IsSat = true;
      OS << "Unsafe (bad may happen)\n";
      return ProofResult::False;
    } else {
      OS << "Safe (bad won't happen)\n";
      return ProofResult::True;
    }
  } else {
    llvm_unreachable("The solver result has no value");
  }

  return ProofResult::Maybe;
}

SMTExprRef CheckedCSMTProver::FactToSym(AbstractFact *AFact) {
#if LLVM_WITH_Z3
  OS << "\n"; 
  FactPrinter::PrintPretty(S, AFact);

  SMTExprRef ResultExpr = FactSymMap[AFact];
  if (ResultExpr)
    return ResultExpr;

  SMTExprBuilder Builder(this);  

  if (auto *Fact = dyn_cast<WhereClauseFact>(AFact)) {
    if (auto *BF = dyn_cast<BoundsDeclFact>(Fact)) {
      ResultExpr = Builder.BoundsToSym(BF->getVarDecl(), BF->getNormalizedBounds());
    } else if (auto *EF = dyn_cast<EqualityOpFact>(Fact)) {
      ResultExpr = Builder.ExprToSym(EF->EqualityOp);
    }
  } else if (auto *IF = dyn_cast<InferredFact>(AFact)) {
    ResultExpr = Builder.ExprToSym(IF->EqualityOp);
  } else
    llvm_unreachable("invalid fact");

  OS << "ResultExpr: " << ResultExpr << "\n";

  if (ResultExpr) {
    ResultExpr->dump();
    FactSymMap[AFact] = ResultExpr;
  }

  return ResultExpr;

#endif

  return nullptr;
}

std::string CheckedCSMTProver::GetSymName(Sema &S, const VarDecl *VD) {
  std::string Str;
  llvm::raw_string_ostream SS(Str);

  SourceManager &SrcManager = S.getSourceManager();
  // TODO: change to get id location IdLoc?
  SourceLocation Loc = VD->getInitializerStartLoc();
  // TODO: not work for ParamVarDecl
  unsigned LineNumber = SrcManager.getSpellingLineNumber(Loc);

  SS << VD->getQualifiedNameAsString() << "@" << LineNumber;

  return Str;
}

SMTExprRef CheckedCSMTProver::VarDeclToSym(const VarDecl *VD) {
  SMTExprRef Result = VarDeclSymMap[VD];
  if (Result)
    return Result;

  const std::string SynName = GetSymName(S, VD);
  uint64_t BitWidth = Ctx.getTypeSize(VD->getType());
  Result = Solver->mkSymbol(SynName.c_str(), Solver->getBitvectorSort(BitWidth));
  VarDeclSymMap[VD] = Result;

  return Result;
}

SMTExprRef CheckedCSMTProver::GetCachedSym(const Expr *E) {
  // User should not call this with nullptr
  assert(E);
  SMTExprRef Result = ExprSymMap[E];
  return Result;
}

void CheckedCSMTProver::SetCachedSym(const Expr *E, SMTExprRef Sym) {
  ExprSymMap[E] = Sym;
}

void CheckedCSMTProver::SetVarDeclSymBimapping(const VarDecl *VD, llvm::SMTExprRef Sym) {
  VarDeclSymMap[VD] = Sym;
  SymVarDeclMap[Sym] = VD;
}
