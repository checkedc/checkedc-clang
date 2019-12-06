//== SimpleBoundsChecker.cpp ------------------------------*- C++ -*--==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines SimpleBoundsChecker, which is a path-sensitive checker
// which looks for an out-of-bound accesses.
// 
// The two main differences compared to other bounds checkers are:
// 1. This checker is Checked C Bounds aware. It reads bounds-safe information
//    on the function declaration and check the index against the bounds
// 2. It uses Z3 for handling complex non-concrete bounds constraint. Clang
//    should be compiled with the Z3 solver enabled.
//
// Assumptions:
// - The bounds are valid (LB < UB). In other words, the function is not called
//   with values that constitute empty or invalid ranges for any of the
//   pointers
//
// Debug Preprocessor Flags:
// - DEBUG_DUMP: dumps memory regions, bounds and index expressions,
//               generated SMT formulas, and some return on failure causes
//===----------------------------------------------------------------------===//

#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ExprEngine.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/SMTConv.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/SMTSolver.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/SValVisitor.h"

#include <string>

#define DEBUG_DUMP 0

using namespace clang;
using namespace ento;

namespace {
  class SimpleBoundsChecker : public Checker<check::Location> {
    mutable std::unique_ptr<BuiltinBug> BT;

    // Replaces the symbol 'from' with the symbol 'to'
    // in the symbolic expression 'E'
    SVal replaceSVal(ProgramStateRef State, SVal E, SVal From, SVal To) const;

    // Generates a symbolic expression out of the given bounds expression.
    //  The terms (variables) should have symbolic values already
    const SymExpr *getSymExpr(ProgramStateRef State,
                              const BoundsExpr *Bounds,
                              const LocationContext *LCtx,
                              SValBuilder &SVB) const;

    void reportOutofBoundsAccess(ProgramStateRef OutBound, const Stmt *LoadS,
                                 CheckerContext &C) const;


    public:
    void checkLocation(SVal l, bool isLoad, const Stmt *S,
                       CheckerContext &C) const;
  };
}

void SimpleBoundsChecker::checkLocation(SVal l, bool isLoad, const Stmt *LoadS,
                                        CheckerContext &C) const {

  const MemRegion *R = l.getAsRegion();
  if (!R)
    return;

  const ElementRegion *ER = dyn_cast<ElementRegion>(R);
  if (!ER)
    return;

#if DEBUG_DUMP
  llvm::errs() << "Element's Region:\n";
  ER->dump();
  llvm::errs() << "\n";
  llvm::errs() << "Element's Base Region:\n";
  ER->getBaseRegion()->dump();
  llvm::errs() << "\n";
#endif

  // Getting pointers to manager objects
  ProgramStateRef State = C.getState();
  ProgramStateManager &SM = State->getStateManager();
  SValBuilder &SvalBuilder = SM.getSValBuilder();
  ASTContext &Ctx = SvalBuilder.getContext();

  // Get the index of the accessed element.
  DefinedOrUnknownSVal Idx = ER->getIndex().castAs<DefinedOrUnknownSVal>();

  // Get the size of the array.
  DefinedOrUnknownSVal NumElements =
      C.getStoreManager().getSizeInElements(State,
                                            ER->getSuperRegion(),
                                            ER->getValueType());

  ProgramStateRef StInBound = State->assumeInBound(Idx, NumElements, true);
  ProgramStateRef StOutBound = State->assumeInBound(Idx, NumElements, false);

  bool BugFound = (!StInBound && StOutBound);

  if (BugFound) {
    // We already know there is an out-of-bounds access
    // report and exit
    reportOutofBoundsAccess(StOutBound, LoadS, C);
    return;
  }


  // For handling complex expressions over indices:

  // 1. Create a Z3 instance
  SMTSolverRef Solver = CreateZ3Solver();

  // 2. Get the Symbolic Expr of the index and bounds expressions
  //

  const LocationContext *LCtx = C.getLocationContext();
  const FunctionDecl *FD = dyn_cast_or_null<FunctionDecl>(LCtx->getDecl());
  if (!FD) {
#if DEBUG_DUMP
    llvm::errs() <<
      "ERR: checkLocation: Cannot get the FunctionDecl to read the bounds!\n";
#endif
    return;
  }

  // Match the deref base pointer to the corresponding function argument
  const BoundsExpr *BE = nullptr;
  for(unsigned int i=0; i<FD->getNumParams(); ++i) {
    const ParmVarDecl *Arg = FD->getParamDecl(i);
    if (!Arg->hasBoundsDeclaration(Ctx) && !Arg->hasBoundsSafeInterface(Ctx))
      continue;
    if (State->getSVal(State->getRegion(Arg, LCtx)).getAsRegion()
        == ER->getBaseRegion()) {
      BE = Arg->getBoundsExpr();
      break;
    }
  }

  if (!BE) {
#if DEBUG_DUMP
    llvm::errs() <<
      "ERR: No Bounds Expression has been found "
      "on the pointer that is being derefed\n";
#endif
    return;
  }

  SymbolRef SymBE = getSymExpr(State, BE, LCtx, SvalBuilder);
  if (!SymBE) {
#if DEBUG_DUMP
    llvm::errs() <<
      "ERR: Failed to generate a Symbolic Expression "
      "out of the Bounds Expression.\n";
#endif
    return;
  }

#if DEBUG_DUMP
  llvm::errs() << "Symbolic Bounds Expression: ";
  SymBE->dump();
  llvm::errs() << "\n";
#endif

  const SymExpr *SymIdx = Idx.getAsSymbol();
  if (!SymIdx) {
    // symIdx is NULL: Index might be concrete, fall back to normal check!
    if (StOutBound && !StInBound) {
      reportOutofBoundsAccess(StOutBound, LoadS, C);
    }
    return;
  }

#if DEBUG_DUMP
  llvm::errs() << "Symbolic Index Expression: ";
  SymIdx->dump();
  llvm::errs() << "\n";
#endif

  // 3. Encode the expression as a SMT formula
  //    it should be of the form: (idx < lower_bound) v (idx >= upper_bound)
  //
  // TODO: currently only expressions of count(n) is handled;
  //       generalize for bounds(LB, UB)
  //
  // SMT expression of the bounds expression
  SMTExprRef SmtBE = SMTConv::getExpr(Solver, Ctx, SymBE);
  // SMT expression of the index
  SMTExprRef SmtIdx = SMTConv::getExpr(Solver, Ctx, SymIdx);
  // SMT expression for (idx >= UpperBound)
  SMTExprRef OverUB = Solver->mkBVSge(SmtIdx, SmtBE);
  // SMT expression for (idx < LowerBound)
  SMTExprRef UnderLB =
    Solver->mkBVSlt(SmtIdx, Solver->mkBitvector(llvm::APSInt(32), 32));

  SMTExprRef SmtOOBounds = Solver->mkOr(UnderLB, OverUB);

  // Forcing the expression in the 'count' bounds to be positive '> 0'
  SMTExprRef PositiveBE =
    Solver->mkBVSgt(SmtBE, Solver->mkBitvector(llvm::APSInt(32), 32));

  // the final SMT expression
  SMTExprRef Constraint = Solver->mkAnd(PositiveBE, SmtOOBounds);

#if DEBUG_DUMP
  llvm::errs() << "SMT constraints for (LB <= Idx < UB) expression:\n";
  Constraint->print(llvm::errs());
  llvm::errs() << "\n";
#endif

  Solver->addConstraint(Constraint);


  // 4. Solve the SMT formula for a bad input using Z3
  Optional<bool> IsSat = Solver->check();
  if (IsSat.hasValue()) {
    if (!IsSat.getValue())
      return;
      // If the formula is UNSAT, there is no input value
      // that makes the index go out-of-bounds

    BugFound = true;
  }
  // 5. [Optional] TODO: Read the model. The model represents a possible input
  //               value that makes the index go out of bounds.
  //               Only useful for bug reports and debugging!
  // ----


  if (BugFound) {
    reportOutofBoundsAccess(StOutBound, LoadS, C);
    return;
  }

  // Array bound check succeeded. From this point forward this array bound
  // should be seen as in-bound in the program states.
  C.addTransition(StInBound);
}

void SimpleBoundsChecker::reportOutofBoundsAccess(ProgramStateRef OutBound,
                                                  const Stmt *LoadS,
                                                  CheckerContext &C) const {
  ExplodedNode *N = C.generateErrorNode(OutBound);
  if (!N)
    return;

  if (!BT)
    BT.reset(new BuiltinBug(
          this, "Out-of-bound array access",
          "Access out-of-bound array element (buffer overflow)"));

  // Generate a report for this bug.
  auto Report = llvm::make_unique<BugReport>(*BT, BT->getDescription(), N);

  Report->addRange(LoadS->getSourceRange());
  C.emitReport(std::move(Report));
  return;
}


const SymExpr *SimpleBoundsChecker::getSymExpr(ProgramStateRef State,
                                               const BoundsExpr *BE,
                                               const LocationContext *LCtx,
                                               SValBuilder &SVB) const {
  class Generator {
    ProgramStateRef State;
    const LocationContext *LCtx;
    SValBuilder &SVB;

    public:
    Generator(ProgramStateRef _State,
              const LocationContext *_LCtx,
              SValBuilder &_SVB)
      : State(_State), LCtx(_LCtx), SVB(_SVB)
    {
    }

    const SymExpr *VisitBoundsExpr(const BoundsExpr *BE) {
      if (const CountBoundsExpr *CBE = dyn_cast<CountBoundsExpr>(BE)) {
        return VisitExpr(CBE->getCountExpr());
      }
      return nullptr;
    }

    const SymExpr *VisitExpr(Expr *E) {
      E = E->IgnoreCasts();

      if (const BinaryOperator *BO = dyn_cast<BinaryOperator>(E)) {
        BinaryOperator::Opcode op = BO->getOpcode();
        Expr *LeftExpr = BO->getLHS();
        Expr *RightExpr = BO->getRHS();

        const IntegerLiteral *LeftIL = dyn_cast<IntegerLiteral>(LeftExpr);
        const IntegerLiteral *RightIL = dyn_cast<IntegerLiteral>(RightExpr);

        if (!LeftIL && !RightIL) {
          const SymExpr *Left = VisitExpr(LeftExpr);
          const SymExpr *Right = VisitExpr(RightExpr);

          return SVB.getSymbolManager().getSymSymExpr(Left,
                                                      op,
                                                      Right, BO->getType());
        }

        if (!LeftIL) {
          const SymExpr *Left = VisitExpr(LeftExpr);
          llvm::APInt Value = RightIL->getValue();
          llvm::APSInt *Right = new llvm::APSInt(Value);

          return SVB.getSymbolManager().getSymIntExpr(Left,
                                                      op,
                                                      *Right, BO->getType());
        }

        if (!RightIL) {
          const SymExpr *Right = VisitExpr(RightExpr);
          llvm::APInt Value = LeftIL->getValue();
          llvm::APSInt *Left = new llvm::APSInt(Value);

          return SVB.getSymbolManager().getIntSymExpr(*Left,
                                                      op,
                                                      Right, BO->getType());
        }

        return nullptr;
      }

      if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E)) {
        const VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl());
        if (!VD) {
          return nullptr;
        }
        const MemRegion *VDregion = State->getRegion(VD, LCtx);
        SVal SymVal = State->getSVal(VDregion);

        return SymVal.getAsSymExpr();
      }

      if (const IntegerLiteral *IL = dyn_cast<IntegerLiteral>(E)) {
        llvm::APInt Value = IL->getValue();
        llvm::APSInt *SValue = new llvm::APSInt(Value);
        SVal SymVal = nonloc::ConcreteInt(*SValue);

        return SymVal.getAsSymExpr();
      }

      return nullptr;
    }
  };

  return Generator(State, LCtx, SVB).VisitBoundsExpr(BE);
}

SVal SimpleBoundsChecker::replaceSVal(ProgramStateRef State,
                                      SVal E, SVal From, SVal To) const {

  class Replacer : public FullSValVisitor<Replacer, SVal> {
    ProgramStateRef State;
    SVal From;
    SVal To;

    static bool isUnchanged(SymbolRef Sym, SVal Val) {
      return Sym == Val.getAsSymbol();
    }

    public:
    Replacer(ProgramStateRef _State, SVal _From, SVal _To)
      : State(_State), From(_From), To(_To)
    {
    }

    SVal VisitSymExpr(SymbolRef S) {
      if ( const BinarySymExpr *BSE = dyn_cast<BinarySymExpr>(S) ) {
        BinaryOperator::Opcode op = BSE->getOpcode();

        if (const SymIntExpr *SIE = dyn_cast<SymIntExpr>(BSE)) {
          SVal Left = Visit(SIE->getLHS());
          return nonloc::SymbolVal(new SymIntExpr(Left.getAsSymExpr(),
                                                  op,
                                                  SIE->getRHS(),
                                                  SIE->getType()));
        }

        if (const IntSymExpr *ISE = dyn_cast<IntSymExpr>(BSE)) {
          SVal Right = Visit(ISE->getRHS());
          return nonloc::SymbolVal(new IntSymExpr(ISE->getLHS(),
                                                  op,
                                                  Right.getAsSymExpr(),
                                                  ISE->getType()));
        }

        if (const SymSymExpr *SSE = dyn_cast<SymSymExpr>(BSE)) {
          SVal Left = Visit(SSE->getLHS());
          SVal Right = Visit(SSE->getRHS());
          return nonloc::SymbolVal(new SymSymExpr(Left.getAsSymExpr(),
                                                  op,
                                                  Right.getAsSymExpr(),
                                                  SSE->getType()));
        }
      }
      return nonloc::SymbolVal(S);
    }

    SVal VisitMemRegion(const MemRegion *R) {
      return loc::MemRegionVal(R);
    }

    SVal VisitSVal(SVal V) {
      return Visit(V.getAsSymExpr());
    }

    SVal VisitSymbolData(const SymbolData *S) {
      const SymExpr *P = (const SymExpr*)S;
      if ( P && P == From.getAsSymbol() )
        return To;
      return nonloc::SymbolVal(S);
    }

  };

  SVal NewE = Replacer(State, From, To).Visit(E);
  return NewE;
}



void ento::registerSimpleBoundsChecker(CheckerManager &mgr) {
  mgr.registerChecker<SimpleBoundsChecker>();
}
