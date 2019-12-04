//== SimpleBoundsChecker.cpp ------------------------------*- C++ -*--==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines SimpleBoundsChecker, which is a path-sensitive check
// which looks for an out-of-bound accesses.
//
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

#include "clang/Sema/Sema.h"
#include "clang/Sema/AvailableFactsAnalysis.h"
#include "clang/Analysis/CFG.h"

#include <string>


#define DEBUG_DUMP 0
#define BOUNDS_CHECK_WITH_Z3 1
#define TEST_PROGRAM_STATE 0
#define USE_PROGRAM_STATE 0

using namespace clang;
using namespace ento;


namespace {
    class SimpleBoundsChecker : public Checker<check::Location,
                                               check::BeginFunction> {
        mutable std::unique_ptr<BuiltinBug> BT;

        void checkBoundsInfo(const DeclaratorDecl* decl, std::string label, ASTContext& Ctx) const;

        SVal replaceSVal(ProgramStateRef state, SVal V, SVal from, SVal to) const;

        const SymExpr* getSymExpr(ProgramStateRef state, const BoundsExpr* bounds, const LocationContext* LCtx, SValBuilder& SVB) const;

        void reportOutofBoundsAccess(ProgramStateRef outBound, const Stmt* LoadS, CheckerContext& C) const;


    public:
        void checkLocation(SVal l, bool isLoad, const Stmt* S, CheckerContext &C) const;
        void checkBeginFunction(CheckerContext& C) const;
    };
}

#if TEST_PROGRAM_STATE
REGISTER_MAP_WITH_PROGRAMSTATE(BoundsMap, SymbolRef, BoundsState)
#else
REGISTER_MAP_WITH_PROGRAMSTATE(BoundsMap, SymbolRef, SymbolRef)
#endif

void SimpleBoundsChecker::checkLocation(SVal l, bool isLoad, const Stmt* LoadS,
                                      CheckerContext &C) const {

    const MemRegion *R = l.getAsRegion();
    if (!R)
        return;

    const ElementRegion *ER = dyn_cast<ElementRegion>(R);
    if (!ER)
        return;

    llvm::errs() << "------------------------------------------------\n";
    // Only load statements of array subscript type should reach this point.

#if DEBUG_DUMP
    llvm::errs() << "\nLoad Statement:\n";
    LoadS->dumpColor();
    const ArraySubscriptExpr* ASE = dyn_cast<ArraySubscriptExpr>(LoadS->IgnoreImplicit());
    if (!ASE){
        llvm::errs() << "Load statement is not an array subscript expression!\n";
//        return;
    }
    else {
        const Expr* arrayBase = ASE->getBase();
        llvm::errs() << "\nArray base:\n";
        arrayBase->dumpColor();
    }
#endif


    ProgramStateRef state = C.getState();
    ProgramStateManager &SM = state->getStateManager();
    SValBuilder &svalBuilder = SM.getSValBuilder();
    //SymbolManager& symMgr = svalBuilder.getSymbolManager();
    ASTContext &Ctx = svalBuilder.getContext();
    
#if DEBUG_DUMP
    state->getEnvironment().print(llvm::errs(), "\nCheckLocation:: ", "\n", Ctx);
#endif

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

    // 2. Read the Symbolic expr of the index
    //

#if DEBUG_DUMP
    MemRegion::Kind K = ER->getSuperRegion()->getKind();
    llvm::errs() << "\nCheckLocation: memreg.kind: " << K << "\n";
#endif


#if !USE_PROGRAM_STATE

    const LocationContext *LCtx = C.getLocationContext();
    const FunctionDecl *FD = dyn_cast_or_null<FunctionDecl>(LCtx->getDecl());
    if (!FD) {
        llvm::errs() << "FD in checkLoc is NULL!\n";
        return;
    }

    //llvm::errs() << "ER super region: "; ER->getSuperRegion()->dump(); llvm::errs() << "\n";
    //llvm::errs() << "ER base region: "; ER->getBaseRegion()->dump(); llvm::errs() << "\n";
    //llvm::errs() << "ER super SVal: "; state->getSVal(ER->getSuperRegion()).dump(); llvm::errs() << "\n";

    const BoundsExpr* BE = NULL;

    for(unsigned int i=0; i<FD->getNumParams(); i++) {
        const ParmVarDecl* arg = FD->getParamDecl(i);
        //llvm::errs() << "Arg " << i << " Base region: "; state->getRegion(arg, LCtx)->getBaseRegion()->dump(); llvm::errs() << "\n";
        //llvm::errs() << "Arg " << i << " region: "; state->getRegion(arg, LCtx)->dump(); llvm::errs() << "\n";
        //llvm::errs() << "Arg " << i << " SVal: "; state->getSVal(state->getRegion(arg, LCtx)).dump(); llvm::errs() << "\n";
        //llvm::errs() << "Arg " << i << " symbolic base: "; if(state->getRegion(arg, LCtx)->getSymbolicBase())state->getRegion(arg, LCtx)->getSymbolicBase()->dump(); llvm::errs() << "\n";
        //llvm::errs() << "equality: " << (state->getSVal(state->getRegion(arg, LCtx)).getAsRegion() == ER->getBaseRegion()) << "\n";
        //if (!arg->hasBoundsDeclaration(Ctx)) continue;
        //llvm::errs() << "Arg " << i << " Sym Val: "; SymVal.dump(); llvm::errs() << "\n";
        //arg->hasBoundsSafeInterface(Ctx);
        if (state->getSVal(state->getRegion(arg, LCtx)).getAsRegion() == ER->getBaseRegion()) {
            BE = arg->getBoundsExpr();
            break;
        }
    }

    if (!BE) {
        llvm::errs() << "BE is not set: the deref is not coming from function arguments !\n";
        return;
    }

    SymbolRef symBE;

    //const ParmVarDecl* arg0 = FD->getParamDecl(0);
    //const BoundsExpr* BE = arg0->getBoundsExpr();
    //if ( BE ) BE->dumpColor(); else {llvm::errs() << "BoundsExpr of first arg is NULL!\n"; return;}
    //SVal BESymVal = state->getSVal(BE, LCtx); // <-- This is the normal way one should read the symbolic values associated with expression in the 'environment'
    //SymbolRef BER = BESymVal.getAsSymbol();

//    const ParmVarDecl* arg1 = FD->getParamDecl(1);
//    arg1->dumpColor();
    //SVal arg1SVal = state->getSVal(state->getLValue(arg1, LCtx));
//    SVal arg1SVal = state->getSVal(state->getRegion(arg1, LCtx), arg1->getType());

//    if (!BER) {
       // llvm::errs() << "BER is NULL (BoundsExpr SymbolRef is not in the ENV)!\n"; // <-- This means that this expression is not processed by the symbolic engine

//        symBE = arg1SVal.getAsSymbol();
//    }
//    else {
//        symBE = BER;
//    }

    const SymExpr* genBESymExpr = getSymExpr(state, BE, LCtx, svalBuilder);
    if (!genBESymExpr) {
        llvm::errs() << "The generated symExpr for BE is NULL!\n";
    }
    else {
        //llvm::errs() << "Generated symExpr: ";
        //genBESymExpr->dump();
        //llvm::errs() << "\n";
        symBE = genBESymExpr;
    }

    if (!symBE)
        return;

#if DEBUG_DUMP
    llvm::errs() << "symBE: ";
    symBE->dump(); llvm::errs() << "\n";
#endif

    const SymExpr* symIdx = Idx.getAsSymbol();

    if ( !symIdx ) {
        llvm::errs() << "symIdx is NULL! Index might be concrete, fall back to normal check!\n";

        if ( StOutBound && !StInBound ) {
            reportOutofBoundsAccess(StOutBound, LoadS, C);
        }
        return;
    }

// #if DEBUG_DUMP
//     llvm::errs() << "BEGIN symbol iteration on Idx:\n";
// #endif
//     SVal from;
//     bool fromIsSet = false;
//     for( SymExpr::symbol_iterator I = symIdx->symbol_begin(); I != symIdx->symbol_end(); ++I ) {
//         const SymExpr *SE = *I;
//         const SymbolData* SD = dyn_cast<SymbolData>(SE);
//         if (SD) { from = svalBuilder.makeSymbolVal(SD); fromIsSet = true; }
// #if DEBUG_DUMP
//         SE->dump(); llvm::errs() << "\n";
// #endif
//     }
// #if DEBUG_DUMP
//     llvm::errs() << "END symbol iteration on Idx\n";
// #endif

//     if ( fromIsSet ) {
//         SVal newIdx = replaceSVal(state, Idx, from, arg1SVal);
//         if (newIdx.getAsSymbol()) {
// #if DEBUG_DUMP
//             llvm::errs() << "newIdx: ";
//             newIdx.getAsSymbol()->dump(); llvm::errs() << "\n";
// #endif
//             symIdx = newIdx.getAsSymbol();
//         }
//         else llvm::errs() << "empty newIdx!\n";

//     }
//     else {
//         llvm::errs() << "'from' is not set!\n";
//     }
    


#endif // !USE_PROGRAM_STATE

// #define FACT_COLLECTION 0
// #if FACT_COLLECTION

//     Sema *SemaRef; // TODO how do I get this??!
//     std::unique_ptr<CFG> Cfg = CFG::buildCFG(nullptr, FD->getBody(), Ctx, CFG::BuildOptions());
//     if ( Cfg != nullptr ) {
//         //CheckBoundsDeclarations Checker(SemaRef, FD->getBody(), Cfg.get(), FD->getBoundsExpr());
//         AvailableFactsAnalysis Collector(SemaRef, Cfg.get());
//         Collector.Analyze();
//         Collector.DumpComparisonFacts(llvm::errs(), FD->getNameInfo().getName().getAsString());
//         //Checker.TraverseCFG(Collector);
//     }
// #endif


    // 3. Encode the expression as a SMT formula
    //    it should be of the form: (idx < lower_bound) v (idx > upper_bound)
    //
    // TODO: currently only expressions of count(n) is handled; generalize for bounds(LB, UB)
    //
    // SMT expression of the bounds expression
    SMTExprRef smtBE = SMTConv::getExpr(solver, Ctx, symBE); //smtBE->print(llvm::errs()); llvm::errs()<<"\n";
    // SMT expression of the index
    SMTExprRef smtIdx = SMTConv::getExpr(solver, Ctx, symIdx); //smtIdx->print(llvm::errs()); llvm::errs()<<"\n";
    // SMT expression for (idx > UpperBound)
    SMTExprRef overUB = solver->mkBVSgt(smtIdx, smtBE); //overUB->print(llvm::errs()); llvm::errs()<<"\n";
    // SMT expression for (idx < LowerBound)
    SMTExprRef underLB = solver->mkBVSlt(smtIdx, solver->mkBitvector(llvm::APSInt(32), 32)); //underLB->print(llvm::errs()); llvm::errs()<<"\n";

    SMTExprRef smtOOBounds = solver->mkOr(underLB, overUB);

    // Forcing the expression in the 'count' bounds to be non-negative '>= 0'
    SMTExprRef positiveBE = solver->mkBVSge(smtBE, solver->mkBitvector(llvm::APSInt(32), 32));
    
    // the final SMT expression
    SMTExprRef constraint = solver->mkAnd(positiveBE, smtOOBounds); //constraint->print(llvm::errs()); llvm::errs() << "\n";

    
    solver->addConstraint(constraint);


    // 4. Solve the SMT formula for a bad input using Z3
    Optional<bool> isSat = solver->check();
    if ( isSat.hasValue() ) {
        if ( !isSat.getValue() ) // If the formula is UNSAT, there is no input value that makes the index go out-of-bounds
            return;

        bugFound = true;
    }
    // 5. [Optional] Read the model. The model represents a possible input
    //               value that makes the index go out of bounds.
    //               Only useful for bug reports and debugging!


    if ( bugFound ) {
        reportOutofBoundsAccess(StOutBound, LoadS, C);
        return;
    }

    // Array bound check succeeded.  From this point forward the array bound
    // should always succeed.
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

void SimpleBoundsChecker::checkBoundsInfo(const DeclaratorDecl* decl, std::string label, ASTContext& Ctx) const {
    llvm::errs() << "Processing: " << label << " ( " << decl->getDeclName() << " )\n";

    //decl->dump();

    if ( decl->hasBoundsSafeInterface(Ctx) )
        llvm::errs() << "    Bounds-safe interface *found*!\n";
    else
        llvm::errs() << "    Bounds-safe interface not found!\n";

    if ( decl->hasBoundsDeclaration(Ctx) )
        llvm::errs() << "    Bounds declaration *found*!\n";
    else
        llvm::errs() << "    Bounds declaration not found!\n";


    const BoundsExpr* BE = decl->getBoundsExpr();
    if (BE) { llvm::errs() << "    BoundsExpr: \n"; BE->dumpColor(); }

    const InteropTypeExpr* IE = decl->getInteropTypeExpr();
    if (IE) { llvm::errs() << "    InteropExpr: \n"; IE->dumpColor(); }
}

void SimpleBoundsChecker::checkBeginFunction(CheckerContext& C) const {
    ProgramStateRef state = C.getState();
    ProgramStateManager &SM = state->getStateManager();
    SValBuilder &svalBuilder = SM.getSValBuilder();
    ASTContext &Ctx = svalBuilder.getContext();

    const LocationContext *LCtx = C.getLocationContext();
    const FunctionDecl *FD = dyn_cast_or_null<FunctionDecl>(LCtx->getDecl());
    if (!FD)
        return;
    
    FD = FD->getCanonicalDecl();

#if DEBUG_DUMP
    state->getEnvironment().print(llvm::errs(), "\ncheckBegin::\n", "\n", Ctx);
#endif


    CheckedScopeSpecifier CSS = FD->getCheckedSpecifier();
    if ( CSS == CSS_Bounds || CSS == CSS_Memory ) {
        #if DEBUG_DUMP
        llvm::errs() << "Skipping the function " << FD->getDeclName() << ". Because it is in Checked scope!\n";
        #endif
        return;
    }

    bool hasBoundsDecl = FD->hasBoundsDeclaration(Ctx);
    bool hasBoundsSafeDecl = FD->hasBoundsSafeInterface(Ctx);
    for( unsigned int i=0; i<FD->getNumParams(); i++ ) {
        const ParmVarDecl* arg = FD->getParamDecl(i);
        hasBoundsDecl = hasBoundsDecl || arg->hasBoundsDeclaration(Ctx);
        hasBoundsSafeDecl = hasBoundsSafeDecl || arg->hasBoundsSafeInterface(Ctx);
    }

    // If the function has bounds declaration then it is not a bounds-safe interface!
    if ( hasBoundsDecl ) {
        #if DEBUG_DUMP
        llvm::errs() << "Skipping the function " << FD->getDeclName() << ". Because it has bounds declaration!\n";
        #endif
        return;
    }

    if ( !hasBoundsSafeDecl ) {
        #if DEBUG_DUMP
        llvm::errs() << "Skipping the function " << FD->getDeclName() << ". Because it doesn't have bounds-safe information!\n";
        #endif
        return;
    }




#if DEBUG_DUMP
    checkBoundsInfo(FD, "function", Ctx);
    for( unsigned int i=0; i<FD->getNumParams(); i++ ) {
        const ParmVarDecl* arg = FD->getParamDecl(i);
        checkBoundsInfo(arg, "arg" + std::to_string(i), Ctx);
    }
#endif

#if USE_PROGRAM_STATE
    const ParmVarDecl* arg0 = FD->getParamDecl(0);
    const MemRegion* arg0region = state->getRegion(arg0, LCtx);
    Loc argLoc = svalBuilder.makeLoc(arg0region);
    SVal argSVal = state->getSVal(argLoc);

    llvm::errs() << "checkBegin: memreg.kind: " << arg0region->getKind() << "\n";

    const CountBoundsExpr* CBE = dyn_cast<CountBoundsExpr>(arg0->getBoundsExpr());
    SVal argBoundSVal = C.getSVal(CBE->getCountExpr());

    SymbolRef from = argSVal.getAsSymbolicExpression();
    SymbolRef to = argBoundSVal.getAsSymbolicExpression();
    //BoundsState to = BoundsState::mkOneBoundsState();

#if DEBUG_DUMP
    llvm::errs() << "\nargsval: ";
    argSVal.dump();
    llvm::errs() << "\nargboundsval: ";
    argBoundSVal.dump();

    SVal bounds2 = state->getSVal(CBE, LCtx);
    llvm::errs() << "\nargboundsval.v2: ";
    bounds2.dump();
#endif

    state = state->set<BoundsMap>(from, to);
    C.addTransition(state);
#endif
}

const SymExpr* SimpleBoundsChecker::getSymExpr(ProgramStateRef state, const BoundsExpr* BE, const LocationContext* LCtx, SValBuilder& SVB) const {
  class Generator { //: public RecursiveASTVisitor<Generator> {
    ProgramStateRef state;
    SValBuilder &SVB;
    const LocationContext* LCtx;

    public:
    Generator(ProgramStateRef _state, const LocationContext* _LCtx, SValBuilder& _SVB)
      : state(_state), LCtx(_LCtx), SVB(_SVB)
    {
//      llvm::errs() << "Generator class ctor!\n";
    }

    const SymExpr* VisitBoundsExpr(const BoundsExpr* BE) {
//      llvm::errs() << "DBG: visitBoundsExpr: \n";
//      BE->dump();
      if (const CountBoundsExpr* CBE = dyn_cast<CountBoundsExpr>(BE)) {
        return VisitExpr(CBE->getCountExpr());
      }
      return nullptr;
    }

    const SymExpr* VisitExpr(Expr* E) {
//      llvm::errs() << "DBG: visitExpr: \n";
//      E->dump();
      E = E->IgnoreCasts();

      if (const BinaryOperator* BO = dyn_cast<BinaryOperator>(E)) {
//        llvm::errs() << "entered BinaryOperator:\n";
        BinaryOperator::Opcode op = BO->getOpcode();
        Expr* leftExpr = BO->getLHS();
        Expr* rightExpr = BO->getRHS();

        const IntegerLiteral* leftIL = dyn_cast<IntegerLiteral>(leftExpr);
        const IntegerLiteral* rightIL = dyn_cast<IntegerLiteral>(rightExpr);

        if (!leftIL && !rightIL) {
            const SymExpr* left = VisitExpr(leftExpr);
            const SymExpr* right = VisitExpr(rightExpr);

//            llvm::errs() << "symexpr of left: ";
//            if (left) left->dump(); else llvm::errs() << "NULL"; llvm::errs() << "\n";
//            llvm::errs() << "symexpr of right: ";
//            if (right) right->dump(); else llvm::errs() << "NULL"; llvm::errs() << "\n";

            return SVB.getSymbolManager().getSymSymExpr(left, op, right, BO->getType());
        }
        
        if (!leftIL) {
            const SymExpr* left = VisitExpr(leftExpr);
            llvm::APInt value = rightIL->getValue();
            llvm::APSInt* right = new llvm::APSInt(value);

//            llvm::errs() << "symexpr of left: ";
//            if (left) left->dump(); else llvm::errs() << "NULL"; llvm::errs() << "\n";
//            llvm::errs() << "symexpr of right: ";
//            right->dump(); llvm::errs() << "\n";

            return SVB.getSymbolManager().getSymIntExpr(left, op, *right, BO->getType());
        }

        if (!rightIL) {
            const SymExpr* right = VisitExpr(rightExpr);
            llvm::APInt value = leftIL->getValue();
            llvm::APSInt* left = new llvm::APSInt(value);

//            llvm::errs() << "symexpr of left: ";
//            left->dump(); llvm::errs() << "\n";
//            llvm::errs() << "symexpr of right: ";
 //           if (right) right->dump(); else llvm::errs() << "NULL"; llvm::errs() << "\n";

            return SVB.getSymbolManager().getIntSymExpr(*left, op, right, BO->getType());
        }


        llvm::errs() << "returning null from BinaryOperator\n";
        return nullptr;
      }

      if (const DeclRefExpr* DRE = dyn_cast<DeclRefExpr>(E)) {
//        llvm::errs() << "entered DeclRefExpr\n";
        const ValueDecl* VD = DRE->getDecl();
        const ParmVarDecl *PVD = dyn_cast<ParmVarDecl>(VD);
        if (!PVD) {
          llvm::errs() << "PVD from DRE is NULL\n";
          return nullptr;
        }
        const MemRegion* PVDregion = state->getRegion(PVD, LCtx);
        //Loc argLoc = SVB.makeLoc(PVDregion);
        SVal SymVal = state->getSVal(PVDregion);//argLoc);
//        llvm::errs() << "DeclRef to SymExpr:\n---\n";
//        PVD->dump(); llvm::errs() << "\n---\n";
//        SymVal.dump(); llvm::errs() << "\n";
        return SymVal.getAsSymExpr();
      }

      if (const IntegerLiteral* IL = dyn_cast<IntegerLiteral>(E)) {
//        llvm::errs() << "entered IntegerLiteral:\n";
        llvm::APInt value = IL->getValue();
//        llvm::errs() << "APInt value:";
//        value.dump(); llvm::errs() << "\n";
        llvm::APSInt svalue(value);
        SVal SymVal = nonloc::ConcreteInt(svalue);
//        llvm::errs() << "Generated Symval value:";
//        SymVal.dump(); llvm::errs() << "\n";
        return SymVal.getAsSymExpr();
      }

//      llvm::errs() << "returning null\n";


      return nullptr;
    }
  };

  return Generator(state, LCtx, SVB).VisitBoundsExpr(BE);
}

SVal SimpleBoundsChecker::replaceSVal(ProgramStateRef state, SVal V, SVal from, SVal to) const {

  class Replacer : public FullSValVisitor<Replacer, SVal> {
    ProgramStateRef state;
    //SValBuilder &SVB;
    SVal from;
    SVal to;

    static bool isUnchanged(SymbolRef Sym, SVal Val) {
      return Sym == Val.getAsSymbol();
    }

  public:
    Replacer(ProgramStateRef _state, SVal _from, SVal _to)
        : state(_state), from(_from), to(_to)//, SVB(state->getStateManager().getSValBuilder())
        {
            #if DEBUG_DUMP
            llvm::errs() << "replacer is created!\n";
            #endif
        }

    SVal VisitSymExpr(SymbolRef S) {
        #if DEBUG_DUMP
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
        #if DEBUG_DUMP
        llvm::errs() << "Visitor::MemRegion:: "; R->dump(); llvm::errs() << "\n";
        #endif
        return loc::MemRegionVal(R);
    }

    SVal VisitSVal(SVal V) {
        #if DEBUG_DUMP
        llvm::errs() << "Visitor::SVal:: "; V.dump(); llvm::errs() << "\n";
        #endif
        return Visit(V.getAsSymExpr());
    }

    SVal VisitSymbolData(const SymbolData *S) {
        #if DEBUG_DUMP
        llvm::errs() << "Visitor::SymbolData:: "; S->dump(); llvm::errs() << "\n";
        #endif
        const SymExpr *P = (const SymExpr*)S;
        if ( P && P == from.getAsSymbol() )
            return to;
        return nonloc::SymbolVal(S);
    }

  };

  // A crude way of preventing this function from calling itself from evalBinOp.
  //static bool isReentering = false;
  //if (isReentering)
  //  return V;

  //isReentering = true;
  SVal newV = Replacer(state, from, to).Visit(V);
  //isReentering = false;

  return newV;
}



void ento::registerSimpleBoundsChecker(CheckerManager &mgr) {
  mgr.registerChecker<SimpleBoundsChecker>();
}
