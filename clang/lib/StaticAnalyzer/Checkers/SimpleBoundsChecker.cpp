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
// 1. This checker is Checked-C Bounds aware. It reads bounds-safe information
//    on the function declaration and check the index against the bounds
// 2. It uses Z3 for handling complex non-concrete bounds constraint. Clang
//    should be compiled with the Z3 solver enabled.
//
// Debug Preprocessor Flags:
// - DEBUG_DUMP: dumps memory regions, bounds and index expressions,
//               generated SMT formulas, and some return on failure causes
// - DEBUG_VISITORS: dumps visited nodes when traversing expressions
//                   and SVals in replaceSVal and getSymExpr
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
#define DEBUG_VISITORS 0

using namespace clang;
using namespace ento;

namespace {
  class SimpleBoundsChecker : public Checker<check::Location> {
    mutable std::unique_ptr<BuiltinBug> BT;

    void checkBoundsInfo(const DeclaratorDecl* decl, std::string label, ASTContext& Ctx) const;

    // Replaces the symbol 'from' with the symbol 'to' in the symbolic expression 'E'
    SVal replaceSVal(ProgramStateRef state, SVal E, SVal from, SVal to) const;

    // Generates a symbolic expression out of the given bounds expression.
    //  The terms (variables) should have symbolic values already
    const SymExpr* getSymExpr(ProgramStateRef state, const BoundsExpr* bounds, const LocationContext* LCtx, SValBuilder& SVB) const;

    void reportOutofBoundsAccess(ProgramStateRef outBound, const Stmt* LoadS, CheckerContext& C) const;


    public:
    void checkLocation(SVal l, bool isLoad, const Stmt* S, CheckerContext &C) const;
  };
}

void SimpleBoundsChecker::checkLocation(SVal l, bool isLoad, const Stmt* LoadS,
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
  ProgramStateRef state = C.getState();
  ProgramStateManager &SM = state->getStateManager();
  SValBuilder &svalBuilder = SM.getSValBuilder();
  ASTContext &Ctx = svalBuilder.getContext();

  // Get the index of the accessed element.
  DefinedOrUnknownSVal Idx = ER->getIndex().castAs<DefinedOrUnknownSVal>();

  if (Idx.isZeroConstant())
    return;

  // Get the size of the array.
  DefinedOrUnknownSVal NumElements = C.getStoreManager().getSizeInElements(state, ER->getSuperRegion(), ER->getValueType());

  ProgramStateRef StInBound = state->assumeInBound(Idx, NumElements, true);
  ProgramStateRef StOutBound = state->assumeInBound(Idx, NumElements, false);

  bool bugFound = (!StInBound && StOutBound);

  if ( bugFound ){
    // We already know there is an out-of-bounds access
    // report and exit
    reportOutofBoundsAccess(StOutBound, LoadS, C);
    return;
  }


  // For handling complex expressions over indices:

  // 1. Create a Z3 instance
  SMTSolverRef solver = CreateZ3Solver();

  // 2. Get the Symbolic Expr of the index and bounds expressions
  //

  const LocationContext *LCtx = C.getLocationContext();
  const FunctionDecl *FD = dyn_cast_or_null<FunctionDecl>(LCtx->getDecl());
  if (!FD) {
#ifdef DEBUG_DUMP
    llvm::errs() << "ERR: checkLocation: Cannot get the FunctionDecl to read the bounds!\n";
#endif
    return;
  }

  // Match the deref base pointer to the corresponding function argument
  const BoundsExpr* BE = nullptr;
  for(unsigned int i=0; i<FD->getNumParams(); i++) {
    const ParmVarDecl* arg = FD->getParamDecl(i);
    if (!arg->hasBoundsDeclaration(Ctx) && !arg->hasBoundsSafeInterface(Ctx))
      continue;
    if (state->getSVal(state->getRegion(arg, LCtx)).getAsRegion() == ER->getBaseRegion()) {
      BE = arg->getBoundsExpr();
      break;
    }
  }

  if (!BE) {
#if DEBUG_DUMP
    llvm::errs() << "ERR: No Bounds Expression has been found on the pointer that is being derefed\n";
#endif
    return;
  }

  SymbolRef symBE = getSymExpr(state, BE, LCtx, svalBuilder);
  if (!symBE) {
#if DEBUG_DUMP
    llvm::errs() << "ERR: Failed to generate a Symbolic Expression out of the Bounds Expression.\n";
#endif
    return;
  }

#if DEBUG_DUMP
  llvm::errs() << "Symbolic Bounds Expression: ";
  symBE->dump();
  llvm::errs() << "\n";
#endif

  const SymExpr* symIdx = Idx.getAsSymbol();
  if (!symIdx) {
    // symIdx is NULL: Index might be concrete, fall back to normal check!
    if (StOutBound && !StInBound) {
      reportOutofBoundsAccess(StOutBound, LoadS, C);
    }
    return;
  }

#if DEBUG_DUMP
  llvm::errs() << "Symbolic Index Expression: ";
  symIdx->dump();
  llvm::errs() << "\n";
#endif

  // 3. Encode the expression as a SMT formula
  //    it should be of the form: (idx < lower_bound) v (idx >= upper_bound)
  //
  // TODO: currently only expressions of count(n) is handled; generalize for bounds(LB, UB)
  //
  // SMT expression of the bounds expression
  SMTExprRef smtBE = SMTConv::getExpr(solver, Ctx, symBE);
  // SMT expression of the index
  SMTExprRef smtIdx = SMTConv::getExpr(solver, Ctx, symIdx);
  // SMT expression for (idx >= UpperBound)
  SMTExprRef overUB = solver->mkBVSge(smtIdx, smtBE);
  // SMT expression for (idx < LowerBound)
  SMTExprRef underLB = solver->mkBVSlt(smtIdx, solver->mkBitvector(llvm::APSInt(32), 32));

  SMTExprRef smtOOBounds = solver->mkOr(underLB, overUB);

  // Forcing the expression in the 'count' bounds to be non-negative '> 0'
  SMTExprRef positiveBE = solver->mkBVSgt(smtBE, solver->mkBitvector(llvm::APSInt(32), 32));

  // the final SMT expression
  SMTExprRef constraint = solver->mkAnd(positiveBE, smtOOBounds);

#if DEBUG_DUMP
  llvm::errs() << "SMT constraints for (LB <= Idx < UB) expression:\n";
  constraint->print(llvm::errs());
  llvm::errs() << "\n";
#endif

  solver->addConstraint(constraint);


  // 4. Solve the SMT formula for a bad input using Z3
  Optional<bool> isSat = solver->check();
  if ( isSat.hasValue() ) {
    if ( !isSat.getValue() ) // If the formula is UNSAT, there is no input value that makes the index go out-of-bounds
      return;

    bugFound = true;
  }
  // 5. [Optional] TODO: Read the model. The model represents a possible input
  //               value that makes the index go out of bounds.
  //               Only useful for bug reports and debugging!
  // ----


  if ( bugFound ) {
    reportOutofBoundsAccess(StOutBound, LoadS, C);
    return;
  }

  // Array bound check succeeded. From this point forward this array bound
  // should be seen as in-bound in the program states.
  C.addTransition(StInBound);
}

void SimpleBoundsChecker::reportOutofBoundsAccess(ProgramStateRef outBound, const Stmt* LoadS, CheckerContext& C) const {
  ExplodedNode *N = C.generateErrorNode(outBound);
  if (!N)
    return;

  if (!BT)
    BT.reset(new BuiltinBug(
          this, "Out-of-bound array access",
          "Access out-of-bound array element (buffer overflow)"));

  // Generate a report for this bug.
  auto report = llvm::make_unique<BugReport>(*BT, BT->getDescription(), N);

  report->addRange(LoadS->getSourceRange());
  C.emitReport(std::move(report));
  return;
}


const SymExpr* SimpleBoundsChecker::getSymExpr(ProgramStateRef state, const BoundsExpr* BE, const LocationContext* LCtx, SValBuilder& SVB) const {
  class Generator { //: public RecursiveASTVisitor<Generator> {
    ProgramStateRef state;
    const LocationContext* LCtx;
    SValBuilder &SVB;

    public:
    Generator(ProgramStateRef _state, const LocationContext* _LCtx, SValBuilder& _SVB)
      : state(_state), LCtx(_LCtx), SVB(_SVB)
    {
#if DEBUG_VISITORS
      llvm::errs() << "Generator class ctor!\n";
#endif
    }

    const SymExpr* VisitBoundsExpr(const BoundsExpr* BE) {
#if DEBUG_VISITORS
      llvm::errs() << "Generator: visitBoundsExpr: \n";
      BE->dump();
#endif
      if (const CountBoundsExpr* CBE = dyn_cast<CountBoundsExpr>(BE)) {
        return VisitExpr(CBE->getCountExpr());
      }
      return nullptr;
    }

    const SymExpr* VisitExpr(Expr* E) {
#if DEBUG_VISITORS
      llvm::errs() << "DBG: visitExpr: \n";
      E->dump();
#endif
      E = E->IgnoreCasts();

      if (const BinaryOperator* BO = dyn_cast<BinaryOperator>(E)) {
        BinaryOperator::Opcode op = BO->getOpcode();
        Expr* leftExpr = BO->getLHS();
        Expr* rightExpr = BO->getRHS();

        const IntegerLiteral* leftIL = dyn_cast<IntegerLiteral>(leftExpr);
        const IntegerLiteral* rightIL = dyn_cast<IntegerLiteral>(rightExpr);

        if (!leftIL && !rightIL) {
          const SymExpr* left = VisitExpr(leftExpr);
          const SymExpr* right = VisitExpr(rightExpr);

#if DEBUG_VISITORS
          llvm::errs() << "DBG: VisitExpr: SymExpr of left: ";
          if (left) left->dump(); else llvm::errs() << "NULL"; llvm::errs() << "\n";
          llvm::errs() << "DBG: VisitExpr: SymExpr of right: ";
          if (right) right->dump(); else llvm::errs() << "NULL"; llvm::errs() << "\n";
#endif

          return SVB.getSymbolManager().getSymSymExpr(left, op, right, BO->getType());
        }

        if (!leftIL) {
          const SymExpr* left = VisitExpr(leftExpr);
          llvm::APInt value = rightIL->getValue();
          llvm::APSInt* right = new llvm::APSInt(value);

#if DEBUG_VISITORS
          llvm::errs() << "DBG: VisitExpr: SymExpr of left: ";
          if (left) left->dump(); else llvm::errs() << "NULL"; llvm::errs() << "\n";
          llvm::errs() << "DBG: VisitExpr: SymExpr of right: ";
          right->dump(); llvm::errs() << "\n";
#endif

          return SVB.getSymbolManager().getSymIntExpr(left, op, *right, BO->getType());
        }

        if (!rightIL) {
          const SymExpr* right = VisitExpr(rightExpr);
          llvm::APInt value = leftIL->getValue();
          llvm::APSInt* left = new llvm::APSInt(value);

#if DEBUG_VISITORS
          llvm::errs() << "DBG: VisitExpr: SymExpr of left: ";
          left->dump(); llvm::errs() << "\n";
          llvm::errs() << "DBG: VisitExpr: SymExpr of right: ";
          if (right) right->dump(); else llvm::errs() << "NULL"; llvm::errs() << "\n";
#endif
          return SVB.getSymbolManager().getIntSymExpr(*left, op, right, BO->getType());
        }

#if DEBUG_VISITORS
        llvm::errs() << "DBG: VisitExpr: Returning null from BinaryOperator\n";
#endif
        return nullptr;
      }

      if (const DeclRefExpr* DRE = dyn_cast<DeclRefExpr>(E)) {
        const VarDecl* VD = dyn_cast<VarDecl>(DRE->getDecl());
        if (!VD) {
#if DEBUG_VISITORS
          llvm::errs() << "Unexpected (SymExpr Generator): The given Expr contains non-variable DeclRefs\n";
#endif
          return nullptr;
        }
        // const ParmVarDecl *PVD = dyn_cast<ParmVarDecl>(VD);
        // if (!PVD) {
        //   llvm::errs() << "PVD from DRE is NULL\n";
        //   return nullptr;
        // }
        const MemRegion* PVDregion = state->getRegion(VD, LCtx);
        //Loc argLoc = SVB.makeLoc(PVDregion);
        SVal SymVal = state->getSVal(PVDregion);
        return SymVal.getAsSymExpr();
      }

      if (const IntegerLiteral* IL = dyn_cast<IntegerLiteral>(E)) {
        llvm::APInt value = IL->getValue();
        llvm::APSInt* svalue = new llvm::APSInt(value);
        SVal SymVal = nonloc::ConcreteInt(*svalue);
#if DEBUG_VISITORS
        llvm::errs() << "DBG: VisitExpr: Generated Symval for the IntergerLiteral: ";
        SymVal.dump(); llvm::errs() << "\n";
#endif
        return SymVal.getAsSymExpr();
      }

      return nullptr;
    }
  };

  return Generator(state, LCtx, SVB).VisitBoundsExpr(BE);
}

SVal SimpleBoundsChecker::replaceSVal(ProgramStateRef state, SVal E, SVal from, SVal to) const {

  class Replacer : public FullSValVisitor<Replacer, SVal> {
    ProgramStateRef state;
    SVal from;
    SVal to;

    static bool isUnchanged(SymbolRef Sym, SVal Val) {
      return Sym == Val.getAsSymbol();
    }

    public:
    Replacer(ProgramStateRef _state, SVal _from, SVal _to)
      : state(_state), from(_from), to(_to)
    {
#if DEBUG_VISITORS
      llvm::errs() << "replacer is created!\n";
#endif
    }

    SVal VisitSymExpr(SymbolRef S) {
#if DEBUG_VISITORS
      llvm::errs() << "Visitor::SymExpr:: "; S->dump(); llvm::errs() << "\n";
#endif
      if ( const BinarySymExpr* BSE = dyn_cast<BinarySymExpr>(S) ) {
        BinaryOperator::Opcode op = BSE->getOpcode();

        if (const SymIntExpr *SIE = dyn_cast<SymIntExpr>(BSE)) {
          SVal left = Visit(SIE->getLHS());
          return nonloc::SymbolVal(new SymIntExpr(left.getAsSymExpr(), op, SIE->getRHS(), SIE->getType()));
        }

        if (const IntSymExpr *ISE = dyn_cast<IntSymExpr>(BSE)) {
          SVal right = Visit(ISE->getRHS());
          return nonloc::SymbolVal(new IntSymExpr(ISE->getLHS(), op, right.getAsSymExpr(), ISE->getType()));
        }

        if (const SymSymExpr *SSE = dyn_cast<SymSymExpr>(BSE)) {
          SVal left = Visit(SSE->getLHS());
          SVal right = Visit(SSE->getRHS());
          return nonloc::SymbolVal(new SymSymExpr(left.getAsSymExpr(), op, right.getAsSymExpr(), SSE->getType()));
        }
      }
      return nonloc::SymbolVal(S);
    }

    SVal VisitMemRegion(const MemRegion *R) {
#if DEBUG_VISITORS
      llvm::errs() << "Visitor::MemRegion:: "; R->dump(); llvm::errs() << "\n";
#endif
      return loc::MemRegionVal(R);
    }

    SVal VisitSVal(SVal V) {
#if DEBUG_VISITORS
      llvm::errs() << "Visitor::SVal:: "; V.dump(); llvm::errs() << "\n";
#endif
      return Visit(V.getAsSymExpr());
    }

    SVal VisitSymbolData(const SymbolData *S) {
#if DEBUG_VISITORS
      llvm::errs() << "Visitor::SymbolData:: "; S->dump(); llvm::errs() << "\n";
#endif
      const SymExpr *P = (const SymExpr*)S;
      if ( P && P == from.getAsSymbol() )
        return to;
      return nonloc::SymbolVal(S);
    }

  };

  SVal newE = Replacer(state, from, to).Visit(E);
  return newE;
}



void ento::registerSimpleBoundsChecker(CheckerManager &mgr) {
  mgr.registerChecker<SimpleBoundsChecker>();
}
