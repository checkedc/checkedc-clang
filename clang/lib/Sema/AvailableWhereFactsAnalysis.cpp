//===== AvailableWhereFactsAnalysis.h - Dataflow analysis for available facts ====//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
// This file implements a dataflow analysis for available facts analysis.
//===---------------------------------------------------------------------===//

#include "clang/AST/ExprUtils.h"
#include "clang/Sema/BoundsUtils.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/Sema/AvailableWhereFactsAnalysis.h"

namespace clang {

//===---------------------------------------------------------------------===//
// Implementation of the methods in the AvailableWhereFactsAnalysis class. This is
// the main class that implements the dataflow analysis for available facts.
// This class uses helper methods from the AvailableFactsUtil class that are 
// defined later in this file.
//===---------------------------------------------------------------------===//

AvailableWhereFactsAnalysis::~AvailableWhereFactsAnalysis() {
  for (auto &BlockKV : BlockMap) {
    delete BlockKV.second;
  }
}

AvailableWhereFactsAnalysis::ElevatedCFGBlock::~ElevatedCFGBlock() {
  for (auto &EdgeStmtMap : Gen)
    for (auto *Fact : EdgeStmtMap.second)
      if (const InferredFact *IF = dyn_cast<InferredFact>(Fact))
        delete IF;
}

void AvailableWhereFactsAnalysis::Analyze(FunctionDecl *FD,
                                          StmtSetTy NestedStmts) {
  assert(Cfg && "expected CFG to exist");

  // Note: By default, PostOrderCFGView iterates in reverse order. So we always
  // get a reverse post order when we iterate PostOrderCFGView.
  for (const CFGBlock *B : PostOrderCFGView(Cfg)) {
    // SkipBlock will skip all null blocks and the exit block. PostOrderCFGView
    // does not traverse any unreachable blocks. So at the end of this loop
    // BlockMap only contains reachable blocks.
    if (AFUtil.SkipBlock(B))
      continue;

    // Create a mapping from CFGBlock to ElevatedCFGBlock.
    auto EB = new ElevatedCFGBlock(B);
    BlockMap[B] = EB;

    // Compute Gen and Kill sets for statements in the block and the block.
    if (B == &Cfg->getEntry()) {
      ComputeEntryGenKillSets(FD, EB);
    } else {
      ComputeGenKillSets(EB, NestedStmts);
    }
  }

  // Compute Gen sets between blocks.
  for (const CFGBlock *B : PostOrderCFGView(Cfg)) {
    if (AFUtil.SkipBlock(B))
      continue;

    ElevatedCFGBlock *EB = BlockMap[B];

    InitBlockGenOut(EB);
  }

  WorkListTy WorkList;
  WorkList.append(BlockMap[&Cfg->getEntry()]);
  AddSuccsToWorkList(&Cfg->getEntry(), WorkList);

  // Compute the In and Out sets for blocks.
  while (!WorkList.empty()) {
    ElevatedCFGBlock *EB = WorkList.next();
    WorkList.remove(EB);

    bool Changed = false;
    Changed |= ComputeInSet(EB);
    Changed |= ComputeOutSet(EB, WorkList);

    if (Changed)
      AddSuccsToWorkList(EB->Block, WorkList);
  }
}

void AvailableWhereFactsAnalysis::AddSuccsToWorkList(const CFGBlock *CurrBlock,
                                                     WorkListTy &WorkList) {
  if (!CurrBlock)
    return;

  for (const CFGBlock *SuccBlock : CurrBlock->succs()) {
    if (!AFUtil.SkipBlock(SuccBlock))
      WorkList.append(BlockMap[SuccBlock]);
  }
}

void AvailableWhereFactsAnalysis::ComputeEntryGenKillSets(FunctionDecl *FD,
                                                          ElevatedCFGBlock *EB) {
  EB->Kill = KillVarSetTy();
  EB->GenAllSucc = AbstractFactListTy();

  for (const ParmVarDecl *PD : FD->parameters()) {
    CollectFactsInDecl(EB->GenAllSucc, EB->Kill, PD);
    CollectFactsInWhereClause(EB->GenAllSucc, EB->Kill, PD->getWhereClause());
  }
}

void AvailableWhereFactsAnalysis::ComputeGenKillSets(ElevatedCFGBlock *EB,
                                                     StmtSetTy NestedStmts) {
  const Stmt *PrevStmt = nullptr;
  EB->Kill = KillVarSetTy();
  EB->GenAllSucc = AbstractFactListTy();

  for (CFGBlock::const_iterator I = EB->Block->begin(),
                                E = EB->Block->end();
       I != E; ++I) {
    CFGElement Elem = *I;
    if (Elem.getKind() != CFGElement::Statement)
      continue;
    const Stmt *CurrStmt = Elem.castAs<CFGStmt>().getStmt();
    if (!CurrStmt)
      continue;

    ComputeStmtGenKillSets(EB, CurrStmt, NestedStmts);

    // EB->GenAllSucc and EB->Kill are accumulated along the statements.
    // When the loop finishes, they have processed the last statements,
    // and have the final Gen and Kill for the block's result for AllSucc.
    if (!PrevStmt) {
      EB->Kill = EB->StmtKill[CurrStmt];
      EB->GenAllSucc = EB->StmtGen[CurrStmt];
    } else {
      EB->Kill = AFUtil.Union(EB->Kill, EB->StmtKill[CurrStmt]);
      auto FactsDiff = AFUtil.Difference(EB->GenAllSucc,
                                         EB->StmtKill[CurrStmt]);
      EB->GenAllSucc = AFUtil.Union(FactsDiff, EB->StmtGen[CurrStmt]);
    }
    
    EB->PrevStmtMap[CurrStmt] = PrevStmt;
    PrevStmt = CurrStmt;

    EB->LastStmt = CurrStmt;
  };
}

void AvailableWhereFactsAnalysis::ComputeStmtGenKillSets(ElevatedCFGBlock *EB,
                                                         const Stmt *CurrStmt,
                                                         StmtSetTy NestedStmts) {
  EB->StmtKill[CurrStmt] = KillVarSetTy();
  EB->StmtGen[CurrStmt] = AbstractFactListTy();

  // Determine whether CurrStmt generates a dataflow fact and a kill var.

  // A var declaration may have a where clause and have a kill var.
  if (const auto *DS = dyn_cast<DeclStmt>(CurrStmt)) {
    for (const Decl *D : DS->decls()) {
      if (const auto *V = dyn_cast<VarDecl>(D)) {
        if (!V->isInvalidDecl()) {
          CollectFactsInDecl(EB->StmtGen[CurrStmt], EB->StmtKill[CurrStmt], V);
          CollectFactsInWhereClause(EB->StmtGen[CurrStmt], EB->StmtKill[CurrStmt], V->getWhereClause());
        }
      }
    }
  // A where clause on an expression statement (which is represented in the
  // AST as a ValueStmt) can generate a dataflow fact.
  // For example: x = strlen(p) _Where p : bounds(p, p + x);
  } else if (const auto *VS = dyn_cast<ValueStmt>(CurrStmt)) {
    CollectFactsInWhereClause(EB->StmtGen[CurrStmt], EB->StmtKill[CurrStmt], VS->getWhereClause());

  // A where clause on a null statement (meaning a standalone where clause) can
  // generate a dataflow fact.
  // For example: _Where p : bounds(p, p + 1);
  } else if (const auto *NS = dyn_cast<NullStmt>(CurrStmt)) {
    CollectFactsInWhereClause(EB->StmtGen[CurrStmt], EB->StmtKill[CurrStmt], NS->getWhereClause());
  }

  // If a variable modified by CurrStmt occurs in the bounds expression of a
  // null-terminated array then the bounds of that null-terminated array should
  // be killed and its bounds should be reset to its declared bounds.
  // Note: Skip top-level statements that are nested in another top-level
  // statement.
  if (NestedStmts.find(CurrStmt) == NestedStmts.end()) {
    VarSetTy ModifiedVars;
    AFUtil.GetModifiedVars(CurrStmt, ModifiedVars);
    for (const VarDecl *V : ModifiedVars) {
      OS << "  V: " << V->getQualifiedNameAsString() << "\n";
      if (!V->isInvalidDecl()) {
        CollectFactsInDecl(EB->StmtGen[CurrStmt], EB->StmtKill[CurrStmt], V);
      }
    }
  }
}

void AvailableWhereFactsAnalysis::CollectFactsInDecl(
  AbstractFactListTy &Gen, KillVarSetTy &Kill, const VarDecl *V) {
  
  VarDecl *CV = const_cast<VarDecl *>(V);

  if (V->hasBoundsExpr()) {
    BoundsExpr *NormalizedBounds = SemaRef.NormalizeBounds(V);
    if (BoundsExpr *RBE = dyn_cast_or_null<RangeBoundsExpr>(NormalizedBounds)) {
      BoundsDeclFact *BDFact = new BoundsDeclFact(CV, RBE, SourceLocation());          
      Gen.push_back(BDFact);
      Kill.insert(std::make_pair(CV, KillBounds));
    }
  } else        
    Kill.insert(std::make_pair(CV, KillExpr));
}

void AvailableWhereFactsAnalysis::CollectFactsInWhereClause(
  AbstractFactListTy &Gen, KillVarSetTy &Kill, WhereClause *WC) {

  if (!WC)
    return;

  for (auto *Fact : WC->getFacts()) {
    if (auto *BF = dyn_cast<BoundsDeclFact>(Fact)) {
      // ignore the result value here,
      // just take the side-effect to save the normalzied bounds
      SemaRef.NormalizeBounds(BF);
      Gen.push_back(Fact);
      Kill.insert(std::make_pair(BF->getVarDecl(), KillBounds));
    } else if (dyn_cast<EqualityOpFact>(Fact)) {
      Gen.push_back(Fact);
    }
  }
}

// Initiazlize Gen and Out of a block
void AvailableWhereFactsAnalysis::InitBlockGenOut(ElevatedCFGBlock *EB) {

  const CFGBlock *CurrBlock = EB->Block;

  const Stmt *TermStmt = CurrBlock->getTerminatorStmt();
  if (!TermStmt || isa<AsmStmt>(TermStmt))
    return;

  // See CFGBlock::getLastCondition() at line 5950
  // The result is non-null if only CFGTerminator::StmtBranch,
  //  the block's size > 0, the succ_size() >= 2, the last
  //  stmt is a CFGStmt
  const Expr *TermCond = CurrBlock->getLastCondition();
  if (!TermCond)
    return;
  
  for (const CFGBlock *SuccBlock : CurrBlock->succs()) {
    if (AFUtil.SkipBlock(SuccBlock))
      continue;
    
    ElevatedCFGBlock *SuccEB = BlockMap[SuccBlock];

    EB->Gen[SuccEB] = AbstractFactListTy();
    EB->Out[SuccEB] = AbstractFactListTy();

    if (AFUtil.IsFallthroughEdge(CurrBlock, SuccBlock))
      continue;

    //
    // check SwitchStmt
    //
    bool IsSwitchCase = AFUtil.IsSwitchCaseBlock(CurrBlock, SuccBlock);
    if (IsSwitchCase) {
      const SwitchStmt *SS = dyn_cast<SwitchStmt>(TermStmt);
      // a fallthrough case
      if (!SS)
        continue;

      const Stmt *SuccLabel = SuccBlock->getLabel();
      bool IsDefaultCase = isa<DefaultStmt>(SuccLabel);

      // TODO: handle default case
      if (IsDefaultCase)
        continue;

      // When reaching here, it should be a valid switch-case edge on blocks
      const CaseStmt *CS = dyn_cast<CaseStmt>(SuccLabel);
      if (!CS)
        llvm_unreachable("not a CaseStmt");
      Expr *LHS = const_cast<Expr *>(CS->getLHS());    
      if (!LHS)
        llvm_unreachable("the case should have getLHS()");
      
      // Get the switch condition expression
      Expr *StmtCond = const_cast<Expr *>(SS->getCond());
      
      // Make an inferred fact with switch condition expression equals the labal value on CaseStmt
      BinaryOperator *Binop = ExprCreatorUtil::CreateBinaryOperator(SemaRef, StmtCond, LHS, BinaryOperatorKind::BO_EQ);
      InferredFact *EqFact = new InferredFact(Binop, SourceLocation());
      EB->Gen[SuccEB].push_back(EqFact);

      continue;
    }

    //
    // check IfStmt
    //
    // TODO: need guard from non-IfStmt
    bool EdgeCondition = AFUtil.ConditionOnEdge(CurrBlock, SuccBlock);

    // If the condition expression used in the if is a binary comparision, the inferred fact
    // is generated based on whether the edge is for then-block or the else-block
    if (const auto *BO = dyn_cast<BinaryOperator>(TermCond)) {
      if (BO->isComparisonOp()) {
        BinaryOperatorKind Op = EdgeCondition ? BO->getOpcode() : BinaryOperator::negateComparisonOp(BO->getOpcode());
        Expr *LHS = const_cast<Expr *>(BO->getLHS());
        Expr *RHS = const_cast<Expr *>(BO->getRHS());
        BinaryOperator *Binop = ExprCreatorUtil::CreateBinaryOperator(SemaRef, LHS, RHS, Op);
        InferredFact *EqFact = new InferredFact(Binop, SourceLocation());
        EB->Gen[SuccEB].push_back(EqFact);
        continue;
      }
    }

    // Otherwise, the fact is in a general form on the the condition expression
    // The unsupported expression will be dropped when the fact is used to 
    // generate the constraints. Here we just create the fact universally.
    const llvm::APInt APZero;
    Expr *LHS = const_cast<Expr *>(TermCond); 
    IntegerLiteral *Zero = ExprCreatorUtil::CreateIntegerLiteral(Ctx, APZero);

    if (EdgeCondition) {
      BinaryOperator *Binop = ExprCreatorUtil::CreateBinaryOperator(SemaRef, LHS, Zero, BinaryOperatorKind::BO_NE);
      InferredFact *EqFact = new InferredFact(Binop, SourceLocation());
      EB->Gen[SuccEB].push_back(EqFact);
    } else {
      BinaryOperator *Binop = ExprCreatorUtil::CreateBinaryOperator(SemaRef, LHS, Zero, BinaryOperatorKind::BO_EQ);
      InferredFact *EqFact = new InferredFact(Binop, SourceLocation());
      EB->Gen[SuccEB].push_back(EqFact);
    }
  }
}

bool AvailableWhereFactsAnalysis::ComputeInSet(ElevatedCFGBlock *EB) {
  const CFGBlock *CurrBlock = EB->Block;
  auto OrigIn = EB->In;

  auto Accu = AbstractFactListTy();
  bool IsFirst = true;

  // Iterate through all the predecessor blocks of EB.
  for (const CFGBlock *PredBlock : CurrBlock->preds()) {
    auto BlockIt = BlockMap.find(PredBlock);
    if (BlockIt == BlockMap.end())
      continue;

    ElevatedCFGBlock *PredEB = BlockIt->second;

    AbstractFactListTy PredOut = AFUtil.Union(PredEB->Out[EB], PredEB->OutAllSucc);

    if (IsFirst) {
      Accu = PredOut;
      IsFirst = false;
      continue;
    }

    Accu = AFUtil.Intersect(Accu, PredOut);
  }

  EB->In = Accu;

  return !AFUtil.IsEqual(OrigIn, EB->In);
}

bool AvailableWhereFactsAnalysis::ComputeOutSet(ElevatedCFGBlock *EB,
                                                WorkListTy &WorkList) {
  auto OrigOutAllSucc = EB->OutAllSucc;
  auto FactsDiff = AFUtil.Difference(EB->In, EB->Kill);
  EB->OutAllSucc = AFUtil.Union(EB->GenAllSucc, FactsDiff);

  // If the OutAllSucc is changed, all the successors block will be added to the 
  // WorkList after this function
  const bool isOutAllSuccChanged = !AFUtil.IsEqual(EB->OutAllSucc, OrigOutAllSucc);

  for (const CFGBlock *SuccBlock : EB->Block->succs()) {
    if (AFUtil.SkipBlock(SuccBlock))
      continue;

    ElevatedCFGBlock *SuccEB = BlockMap[SuccBlock];
    auto OrigOut = EB->Out[SuccEB];
    EB->Out[SuccEB] = AFUtil.Union(EB->Gen[SuccEB], FactsDiff);

    const bool isThisOutChanged = !AFUtil.IsEqual(EB->Out[SuccEB], OrigOut);

    // If OutAllSucc is changed, then all successors block will be added outside.
    // When OutAllSucc is not changed but the Out for this succ block is changed,
    // only this one succ block is added to the WorkList
    if (!isOutAllSuccChanged && isThisOutChanged)
      WorkList.append(SuccEB);
  }

  return isOutAllSuccChanged;
}

AbstractFactListTy AvailableWhereFactsAnalysis::GetStmtOut(ElevatedCFGBlock *EB,
                      const Stmt *CurrStmt) const {
  if (CurrStmt) {
    return EB->StmtGen[CurrStmt];
  }
  return EB->In;
}

AbstractFactListTy AvailableWhereFactsAnalysis::GetStmtIn(ElevatedCFGBlock *EB,
                      const Stmt *CurrStmt) const {
  // StmtIn of a statement is equal to the StmtOut of its previous statement.
  return GetStmtOut(EB, EB->PrevStmtMap[CurrStmt]);
}

void AvailableWhereFactsAnalysis::DumpAvailableFacts(FunctionDecl *FD) {
  OS << "\n--------------------------------------\n";
  // Print the function name.
  OS << "Function: " << FD->getName() << "\n";

  for (const CFGBlock *CurrBlock : GetOrderedBlocks()) {
    // Print the current block number.
    OS << "Block: B" << CurrBlock->getBlockID();

    // Print the predecessor blocks of the current block.
    OS << "; Pred: ";
    for (const CFGBlock *PredBlock : CurrBlock->preds()) {
      if (PredBlock) {
        OS << "B" << PredBlock->getBlockID();

        if (PredBlock != *(CurrBlock->preds().end() - 1))
          OS << ", ";
      }
    }

    // Print the successor blocks of the current block.
    OS << "; Succ: ";
    for (const CFGBlock *SuccBlock : CurrBlock->succs()) {
      if (SuccBlock) {
        OS << "B" << SuccBlock->getBlockID();

        if (SuccBlock != *(CurrBlock->succs().end() - 1))
          OS << ", ";
        }
    }
    OS << "\n";

    ElevatedCFGBlock *EB = BlockMap[CurrBlock];

    // Print each statement.
    for (CFGElement Elem : *CurrBlock) {
      if (Elem.getKind() == CFGElement::Statement) {
        const Stmt *CurrStmt = Elem.castAs<CFGStmt>().getStmt();
        if (!CurrStmt)
          continue;

        AFUtil.Print(CurrStmt);

        OS << "    Facts:";

        AbstractFactListTy Facts = GetStmtIn(EB, CurrStmt);

        AFUtil.DumpAbstractFacts(Facts);

        OS << "    KillVars: ";
        AFUtil.PrintKillVarSet(EB->StmtKill[CurrStmt]);

        OS << "\n";
      }
    }

    OS << "  Gen [B" << CurrBlock->getBlockID() << ", AllSucc]: ";
    AFUtil.DumpAbstractFacts(EB->GenAllSucc);

    OS << "\n  Kill [B" << CurrBlock->getBlockID() << ", AllSucc]: ";
    AFUtil.PrintKillVarSet(EB->Kill);

    OS << "\n  In [B" << CurrBlock->getBlockID() << "]: ";
    AFUtil.DumpAbstractFacts(EB->In);

    OS << "\n  Out [B" << CurrBlock->getBlockID() << ", AllSucc]: ";
    AFUtil.DumpAbstractFacts(EB->OutAllSucc);

    for (const CFGBlock *SuccBlock : CurrBlock->succs()) {
      if (AFUtil.SkipBlock(SuccBlock))
        continue;

      ElevatedCFGBlock *SuccEB = BlockMap[SuccBlock];
      
      OS << "\n  Gen ["
         << "B" << CurrBlock->getBlockID()
         << " -> "
         << "B" << SuccBlock->getBlockID()
         << "]: ";

      AFUtil.DumpAbstractFacts(EB->Gen[SuccEB]);

      OS << "\n  Out ["
         << "B" << CurrBlock->getBlockID()
         << " -> "
         << "B" << SuccBlock->getBlockID()
         << "]: ";

      AFUtil.DumpAbstractFacts(EB->Out[SuccEB]);
    }
    OS << "\n";
  }

  OS << "==-----------------------------------==\n";
}

OrderedBlocksTy AvailableWhereFactsAnalysis::GetOrderedBlocks() const {
  // We order the CFG blocks based on block ID. Block IDs decrease from entry
  // to exit. So we sort in the reverse order.
  OrderedBlocksTy OrderedBlocks;
  for (auto BlockEBPair : BlockMap) {
    const CFGBlock *B = BlockEBPair.first;
    OrderedBlocks.push_back(B);
  }

  llvm::sort(OrderedBlocks.begin(), OrderedBlocks.end(),
    [] (const CFGBlock *A, const CFGBlock *B) {
        return A->getBlockID() > B->getBlockID();
    });
  return OrderedBlocks;
}
// end of methods for the AvailableWhereFactsAnalysis class.

//===---------------------------------------------------------------------===//
// Implementation of the methods in the AvailableFactsUtil class. This class
// contains helper methods that are used by the AvailableFactsUtil class to
// perform the dataflow analysis.
//===---------------------------------------------------------------------===//

void AvailableFactsUtil::Print(const Expr * E) const {
  if (!E) {
    OS << "  Expr: null\n";
    return;
  }

  std::string Str;
  llvm::raw_string_ostream SS(Str);
  static PrintingPolicy print_policy(Ctx.getPrintingPolicy());
  print_policy.FullyQualifiedName = 1;
  print_policy.SuppressScope = 0;
  print_policy.PrintCanonicalTypes = 1;
  E->printPretty(SS, nullptr, print_policy);

  OS << "  Expr: " << SS.str();
  if (SS.str().back() != '\n')
    OS << "\n";
}

void AvailableFactsUtil::Print(const Stmt *Stmt) const {
  if (!Stmt) {
    OS << "  Stmt: null\n";
    return;
  }

  std::string Str;
  llvm::raw_string_ostream SS(Str);
  Stmt->printPretty(SS, nullptr, Ctx.getPrintingPolicy());

  OS << "  Stmt: " << SS.str();
  if (SS.str().back() != '\n')
    OS << "\n";
}

void AvailableFactsUtil::DumpAbstractFact(const AbstractFact *AFact) const {
  if (auto *Fact = dyn_cast<WhereClauseFact>(AFact)) {
    if (auto *BF = dyn_cast<BoundsDeclFact>(Fact)) {
      if (const BoundsExpr *Bounds = BF->getBoundsExpr()) {
        BoundsExpr *NormalizedBounds = SemaRef.NormalizeBounds(BF);
        RangeBoundsExpr *RBE = dyn_cast_or_null<RangeBoundsExpr>(NormalizedBounds);

        if (Bounds == AvailableWhereFactsAnalysis::Top) {
          OS << "Top\n";
          return;
        }

        Expr *Lower = RBE->getLowerExpr();
        Expr *Upper = RBE->getUpperExpr();

        OS << "     " << BF->getVarDecl()->getQualifiedNameAsString() << ": bounds(";
          Lower->printPretty(OS, nullptr, Ctx.getPrintingPolicy());
          OS << ", ";
          Upper->printPretty(OS, nullptr, Ctx.getPrintingPolicy());
        OS << ")";
      }
    } else if (auto *EF = dyn_cast<EqualityOpFact>(Fact)) {
      if (const BinaryOperator *BO = EF->EqualityOp) {
        std::string Str;
        llvm::raw_string_ostream SS(Str);
          
        BO->printPretty(SS, nullptr, Ctx.getPrintingPolicy());
        OS << "     " << SS.str();
      }
    }
  } else if (auto *IF = dyn_cast<InferredFact>(AFact)) {
    if (const BinaryOperator *BO = IF->EqualityOp) {
      std::string Str;
      llvm::raw_string_ostream SS(Str);
        
      BO->printPretty(SS, nullptr, Ctx.getPrintingPolicy());
      OS << "     " << SS.str();
    }
  } else {
    OS << "Unknown Fact";
  }

  OS << "\n";
}

void AvailableFactsUtil::DumpAbstractFacts(const AbstractFactListTy &Facts) const {
  size_t s = Facts.size();
  if (s == 0) {
    OS << "{}\n";
  } else {
    OS << "\n";
  }

  for (const AbstractFact *AFact : Facts) {
    DumpAbstractFact(AFact);
  }
}

void AvailableFactsUtil::PrintKillVarSet(KillVarSetTy VarSet) const {
  if (VarSet.size() == 0) {
    OS << "{}\n";
    return;
  }

  std::vector<KillVar> Vars(VarSet.begin(), VarSet.end());

  llvm::sort(Vars.begin(), Vars.end(),
    [&](const KillVar &A, const KillVar &B) {
       return (A.first->getQualifiedNameAsString().compare(
              B.first->getQualifiedNameAsString()) < 0) || (A.second < B.second);
    });

  for (const KillVar &V : Vars)
    OS << V.first->getQualifiedNameAsString() << ", ";
  OS << "\n";
}

bool AvailableFactsUtil::IsFallthroughEdge(const CFGBlock *PredBlock,
                                           const CFGBlock *CurrBlock) const {
  // A fallthrough edge between two blocks is always a true edge. If PredBlock
  // has only one successor and CurrBlock is that successor then it means the
  // edge between PredBlock and CurrBlock is a fallthrough edge.
  return PredBlock->succ_size() == 1 &&
         CurrBlock == *(PredBlock->succs().begin());
}

bool AvailableFactsUtil::IsSwitchCaseBlock(const CFGBlock *PredBlock,
                                           const CFGBlock *CurrBlock) const {
  // Check if PredBlock ends in a switch statement.
  const Stmt *TerminatorStmt = PredBlock->getTerminatorStmt();
  if (const auto *TS = dyn_cast_or_null<SwitchStmt>(TerminatorStmt)) {
    const auto *SSCond = TS->getCond();
    if (SSCond->containsErrors())
      return false;
  } else
    return false;

  const Stmt *BlockLabel = CurrBlock->getLabel();
  return BlockLabel &&
        (isa<CaseStmt>(BlockLabel) ||
         isa<DefaultStmt>(BlockLabel));
}

bool AvailableFactsUtil::ConditionOnEdge(const CFGBlock *PredBlock,
                                    const CFGBlock *CurrBlock) const {
  if (PredBlock->succ_empty())
    llvm_unreachable("not guard from a PredBlock with no succ");

  // Get the last successor in the list of successors of PredBlock.
  const CFGBlock *LastSucc = *(PredBlock->succs().end() - 1);

  return (CurrBlock != LastSucc);
}

void AvailableFactsUtil::GetModifiedVars(const Stmt *CurrStmt,
                                         VarSetTy &ModifiedVars) const {
  // Get all variables modified by CurrStmt or statements nested in CurrStmt.
  if (!CurrStmt)
    return;

  Expr *E = nullptr;

  // If the variable is modified using a unary operator, like ++I or I++.
  if (const auto *UO = dyn_cast<const UnaryOperator>(CurrStmt)) {
    if (UO->isIncrementDecrementOp()) {
      assert(UO->getSubExpr() && "invalid UnaryOperator expression");
      E = TranspareCasts(UO->getSubExpr());
    }
  // Else if the variable is being assigned to, like I = ...
  } else if (const auto *BO = dyn_cast<const BinaryOperator>(CurrStmt)) {
    if (BO->isAssignmentOp()) {
      E = TranspareCasts(BO->getLHS());
      if (const auto *UO = dyn_cast<UnaryOperator>(E)) {
        if (UO->getOpcode() == UO_Deref) {
          E = TranspareCasts(UO->getSubExpr());
        }
      } else if (const MemberExpr *ME = dyn_cast<MemberExpr>(E)) {
        E = TranspareCasts(ME->getBase());
      // } else if (auto *AE = dyn_cast<ArraySubscriptExpr>(E)) {
      //   E = TranspareCasts(AE->getBase());
      }
    }
  }

  if (const auto *D = dyn_cast_or_null<DeclRefExpr>(E)) {
    if (const auto *V = dyn_cast_or_null<VarDecl>(D->getDecl())) {
      ModifiedVars.insert(V);
    }
  }

  for (const Stmt *NestedStmt : CurrStmt->children())
    GetModifiedVars(NestedStmt, ModifiedVars);
}

Expr *AvailableFactsUtil::TranspareCasts(const Expr *E) const {
  Expr *Pre = Lex.IgnoreValuePreservingOperations(Ctx, const_cast<Expr *>(E));
  Expr *Post = Pre;
  if (auto *CE = dyn_cast<CastExpr>(Pre)) {
    if (CE->getCastKind() == CastKind::CK_IntegralCast)
      Post = CE->getSubExpr();
  }

  if (auto *CE = dyn_cast<CastExpr>(Pre))
    if (CE->getCastKind() == CastKind::CK_LValueToRValue)
      Post = CE->getSubExpr();

  if (Pre == Post) {
    return Post;
  } else {
    return TranspareCasts(Post);
  }
}

Expr *AvailableFactsUtil::IgnoreCasts(const Expr *E) const {
  return Lex.IgnoreValuePreservingOperations(Ctx, const_cast<Expr *>(E));
}

bool AvailableFactsUtil::SkipBlock(const CFGBlock *B) const {
  return !B || B == &Cfg->getExit();
}

// Common templated set operation functions.
template<class T, class U>
T AvailableFactsUtil::Difference(T &A, U &B) const {
  if (!A.size() || !B.size())
    return A;

  auto CopyA = A;
  for (auto Item : A) {
    if (B.count(Item))
      CopyA.erase(Item);
  }
  return CopyA;
}

template<class T>
T AvailableFactsUtil::Union(T &A, T &B) const {
  auto CopyA = A;
  for (auto Item : B)
    CopyA.insert(Item);

  return CopyA;
}

template<class T>
T AvailableFactsUtil::Intersect(T &A, T &B) const {
  if (!A.size() || !B.size())
    return T();

  auto CopyA = A;
  for (auto Item : A) {
    if (!B.count(Item))
      CopyA.erase(Item);
  }
  return CopyA;
}

template<class T>
bool AvailableFactsUtil::IsEqual(T &A, T &B) const {
  return A.size() == B.size() &&
         A.size() == Intersect(A, B).size();
}

bool AvailableFactsUtil::IsVarInFact(const AbstractFact *Fact, const VarDecl *V) const {
  if (!Fact)
    return false;

  if (const auto *WF = dyn_cast<WhereClauseFact>(Fact)) {
    if (const auto *BF = dyn_cast<BoundsDeclFact>(WF)) {
      BoundsExpr *NormalizedBounds = SemaRef.NormalizeBounds(BF);

      return (BF->getVarDecl() == V) 
          || (BoundsUtil::IsVarInNormalizeBounds(NormalizedBounds, V));
    }

    if (const auto *EF = dyn_cast<EqualityOpFact>(WF)) {
      return ExprUtil::IsVarUsed(V, EF->EqualityOp);
    }
    
    llvm_unreachable("no other subclass of WhereClauseFact yet");
  }

  if (const auto *IF = dyn_cast<InferredFact>(Fact)) {
    return ExprUtil::IsVarUsed(V, IF->EqualityOp);;
  }    

  return false;
}

// Template specializations of common set operation functions.
bool AvailableFactsUtil::IsFactEqual(const AbstractFact *Fact1, const AbstractFact *Fact2) const {
  if (!Fact1 || !Fact2)
    llvm_unreachable("A fact in a container should not be null");

  // pointer equality
  if (Fact1 == Fact2)
    return true;

  // value equality
  if (const auto *WF1 = dyn_cast<WhereClauseFact>(Fact1))
    if (const auto *WF2 = dyn_cast<WhereClauseFact>(Fact2)) {

      if (const auto *BF1 = dyn_cast<BoundsDeclFact>(WF1))
        if (const auto *BF2 = dyn_cast<BoundsDeclFact>(WF2)) {
          BoundsExpr *NormalizedBounds1 = SemaRef.NormalizeBounds(BF1);
          RangeBoundsExpr *RBE1 = dyn_cast_or_null<RangeBoundsExpr>(NormalizedBounds1);

          BoundsExpr *NormalizedBounds2 = SemaRef.NormalizeBounds(BF2);
          RangeBoundsExpr *RBE2 = dyn_cast_or_null<RangeBoundsExpr>(NormalizedBounds2);

          if (RBE1 == AvailableWhereFactsAnalysis::Top) {
            return (RBE2 == AvailableWhereFactsAnalysis::Top);
          }

          return 
            Lex.CompareExprSemantically(RBE1->getLowerExpr(), RBE2->getLowerExpr()) 
            && Lex.CompareExprSemantically(RBE1->getUpperExpr(), RBE2->getUpperExpr()) ;
        }

      // TODO
      // if (const auto *EF1 = dyn_cast<EqualityOpFact>(WF1))
      //   if (const auto *EF2 = dyn_cast<EqualityOpFact>(WF2)) {
      //     return false;
      //   }

      return false;      
    }

  // TODO
  // if (const auto *IF1 = dyn_cast<InferredFact>(Fact1))
  //   if (const auto *IF2 = dyn_cast<InferredFact>(Fact2)) {
  //     return false;
  //   }

  return false;
}

template<>
AbstractFactListTy AvailableFactsUtil::Difference<AbstractFactListTy, KillVarSetTy>(
  AbstractFactListTy &Facts, KillVarSetTy &Kill) const {
  
if (!Facts.size() || !Kill.size())
    return Facts;

  auto result = AbstractFactListTy();

  for (auto AFact : Facts) {
    bool found = false;

    if (auto *Fact = dyn_cast<WhereClauseFact>(AFact)) {
      if (auto *BF = dyn_cast<BoundsDeclFact>(Fact)) {
        for (auto KillVar : Kill) {
          if (KillVar.second == KillBounds && BF->getVarDecl() == KillVar.first) {
            found = true;
            break;
          }
        }
      } else if (auto *EF = dyn_cast<EqualityOpFact>(Fact)) {
        for (auto KillVar : Kill) {
          if (KillVar.second == KillExpr && IsVarInFact(AFact, KillVar.first)) {
            found = true;
            break;
          }
        }
      }
    } else if (auto *IF = dyn_cast<InferredFact>(AFact)) {
        for (auto KillVar : Kill) {
          if (KillVar.second == KillExpr && IsVarInFact(AFact, KillVar.first)) {
            found = true;
          break;
        }
      }
    }

    if (!found)
      result.push_back(AFact);
  }
  return result;
}

template<>
AbstractFactListTy AvailableFactsUtil::Union<AbstractFactListTy>(
  AbstractFactListTy &A, AbstractFactListTy &B) const {
  
  auto result = A;
  for (auto Fact2 : B) {
    auto FindB = std::find_if(A.begin(), A.end(), [&](auto Fact1) {
      return AvailableFactsUtil::IsFactEqual(Fact1, Fact2);
    });

    if (FindB != std::end(A))
      continue;
    else
      result.push_back(Fact2);
  }

  return result;
}

template<>
AbstractFactListTy AvailableFactsUtil::Intersect<AbstractFactListTy>(
  AbstractFactListTy &A, AbstractFactListTy &B) const {

  auto result = AbstractFactListTy();

  if (!A.size() || !B.size())
    return result;

  for (auto Fact1 : A) {
    auto FindA = std::find_if(B.begin(), B.end(), [&](auto Fact2) {
      return AvailableFactsUtil::IsFactEqual(Fact1, Fact2);
    });

    if (FindA != std::end(B))
      result.push_back(Fact1);
  }

  return result;
}

template<>
bool AvailableFactsUtil::IsEqual<AbstractFactListTy>(
  AbstractFactListTy &A, AbstractFactListTy &B) const {
  return A.size() == B.size() &&
         A.size() == Intersect(A, B).size();
}

// end of methods for the AvailableWhereFactsAnalysis class.

} // end namespace clang
