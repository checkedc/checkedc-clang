//===- CompleteBottomUp.cpp - Complete Bottom-Up Data Structure Graphs ----===//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This is the exact same as the bottom-up graphs, but we use take a completed
// call graph and inline all indirect callees into their callers graphs, making
// the result more useful for things like pool allocation.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/DataStructure.h"
#include "llvm/Module.h"
#include "llvm/Analysis/DSGraph.h"
#include "Support/SCCIterator.h"
#include "Support/STLExtras.h"
using namespace llvm;

namespace {
  RegisterAnalysis<CompleteBUDataStructures>
  X("cbudatastructure", "'Complete' Bottom-up Data Structure Analysis");
}


// run - Calculate the bottom up data structure graphs for each function in the
// program.
//
bool CompleteBUDataStructures::run(Module &M) {
  BUDataStructures &BU = getAnalysis<BUDataStructures>();
  GlobalsGraph = new DSGraph(BU.getGlobalsGraph());
  GlobalsGraph->setPrintAuxCalls();

  // Our call graph is the same as the BU data structures call graph
  ActualCallees = BU.getActualCallees();

#if 1   // REMOVE ME EVENTUALLY
  // FIXME: TEMPORARY (remove once finalization of indirect call sites in the
  // globals graph has been implemented in the BU pass)
  TDDataStructures &TD = getAnalysis<TDDataStructures>();

  // The call graph extractable from the TD pass is _much more complete_ and
  // trustable than that generated by the BU pass so far.  Until this is fixed,
  // we hack it like this:
  for (Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI) {
    if (MI->isExternal()) continue;
    const std::vector<DSCallSite> &CSs = TD.getDSGraph(*MI).getFunctionCalls();

    for (unsigned CSi = 0, e = CSs.size(); CSi != e; ++CSi) {
      if (CSs[CSi].isIndirectCall()) {
        Instruction *TheCall = CSs[CSi].getCallSite().getInstruction();

        const std::vector<GlobalValue*> &Callees =
          CSs[CSi].getCalleeNode()->getGlobals();
        for (unsigned i = 0, e = Callees.size(); i != e; ++i)
          if (Function *F = dyn_cast<Function>(Callees[i]))
            ActualCallees.insert(std::make_pair(TheCall, F));
      }
    }
  }
#endif

  std::vector<DSGraph*> Stack;
  hash_map<DSGraph*, unsigned> ValMap;
  unsigned NextID = 1;

  if (Function *Main = M.getMainFunction()) {
    calculateSCCGraphs(getOrCreateGraph(*Main), Stack, NextID, ValMap);
  } else {
    std::cerr << "CBU-DSA: No 'main' function found!\n";
  }
  
  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I)
    if (!I->isExternal() && !DSInfo.count(I))
      calculateSCCGraphs(getOrCreateGraph(*I), Stack, NextID, ValMap);

  return false;
}

DSGraph &CompleteBUDataStructures::getOrCreateGraph(Function &F) {
  // Has the graph already been created?
  DSGraph *&Graph = DSInfo[&F];
  if (Graph) return *Graph;

  // Copy the BU graph...
  Graph = new DSGraph(getAnalysis<BUDataStructures>().getDSGraph(F));
  Graph->setGlobalsGraph(GlobalsGraph);
  Graph->setPrintAuxCalls();

  // Make sure to update the DSInfo map for all of the functions currently in
  // this graph!
  for (DSGraph::ReturnNodesTy::iterator I = Graph->getReturnNodes().begin();
       I != Graph->getReturnNodes().end(); ++I)
    DSInfo[I->first] = Graph;

  return *Graph;
}



unsigned CompleteBUDataStructures::calculateSCCGraphs(DSGraph &FG,
                                                  std::vector<DSGraph*> &Stack,
                                                  unsigned &NextID, 
                                         hash_map<DSGraph*, unsigned> &ValMap) {
  assert(!ValMap.count(&FG) && "Shouldn't revisit functions!");
  unsigned Min = NextID++, MyID = Min;
  ValMap[&FG] = Min;
  Stack.push_back(&FG);

  // The edges out of the current node are the call site targets...
  for (unsigned i = 0, e = FG.getFunctionCalls().size(); i != e; ++i) {
    Instruction *Call = FG.getFunctionCalls()[i].getCallSite().getInstruction();

    // Loop over all of the actually called functions...
    ActualCalleesTy::iterator I, E;
    for (tie(I, E) = ActualCallees.equal_range(Call); I != E; ++I)
      if (!I->second->isExternal()) {
        DSGraph &Callee = getOrCreateGraph(*I->second);
        unsigned M;
        // Have we visited the destination function yet?
        hash_map<DSGraph*, unsigned>::iterator It = ValMap.find(&Callee);
        if (It == ValMap.end())  // No, visit it now.
          M = calculateSCCGraphs(Callee, Stack, NextID, ValMap);
        else                    // Yes, get it's number.
          M = It->second;
        if (M < Min) Min = M;
      }
  }

  assert(ValMap[&FG] == MyID && "SCC construction assumption wrong!");
  if (Min != MyID)
    return Min;         // This is part of a larger SCC!

  // If this is a new SCC, process it now.
  bool IsMultiNodeSCC = false;
  while (Stack.back() != &FG) {
    DSGraph *NG = Stack.back();
    ValMap[NG] = ~0U;

    DSGraph::NodeMapTy NodeMap;
    FG.cloneInto(*NG, FG.getScalarMap(), FG.getReturnNodes(), NodeMap,
                 DSGraph::UpdateInlinedGlobals);

    // Update the DSInfo map and delete the old graph...
    for (DSGraph::ReturnNodesTy::iterator I = NG->getReturnNodes().begin();
         I != NG->getReturnNodes().end(); ++I)
      DSInfo[I->first] = &FG;
    delete NG;
    
    Stack.pop_back();
    IsMultiNodeSCC = true;
  }

  // Clean up the graph before we start inlining a bunch again...
  if (IsMultiNodeSCC)
    FG.removeTriviallyDeadNodes();
  
  Stack.pop_back();
  processGraph(FG);
  ValMap[&FG] = ~0U;
  return MyID;
}


/// processGraph - Process the BU graphs for the program in bottom-up order on
/// the SCC of the __ACTUAL__ call graph.  This builds "complete" BU graphs.
void CompleteBUDataStructures::processGraph(DSGraph &G) {
  // The edges out of the current node are the call site targets...
  for (unsigned i = 0, e = G.getFunctionCalls().size(); i != e; ++i) {
    const DSCallSite &CS = G.getFunctionCalls()[i];
    Instruction *TheCall = CS.getCallSite().getInstruction();

    // The Normal BU pass will have taken care of direct calls well already,
    // don't worry about them.
    if (!CS.getCallSite().getCalledFunction()) {
      // Loop over all of the actually called functions...
      ActualCalleesTy::iterator I, E;
      for (tie(I, E) = ActualCallees.equal_range(TheCall); I != E; ++I) {
        Function *CalleeFunc = I->second;
        if (!CalleeFunc->isExternal()) {
          // Merge the callee's graph into this graph.  This works for normal
          // calls or for self recursion within an SCC.
          G.mergeInGraph(CS, *CalleeFunc, getOrCreateGraph(*CalleeFunc),
                         DSGraph::KeepModRefBits |
                         DSGraph::StripAllocaBit |
                         DSGraph::DontCloneCallNodes);
        }
      }
    }
  }

  // Re-materialize nodes from the globals graph.
  // Do not ignore globals inlined from callees -- they are not up-to-date!
  G.getInlinedGlobals().clear();
  G.updateFromGlobalGraph();

  // Recompute the Incomplete markers
  G.maskIncompleteMarkers();
  G.markIncompleteNodes(DSGraph::MarkFormalArgs);

  // Delete dead nodes.  Treat globals that are unreachable but that can
  // reach live nodes as live.
  G.removeDeadNodes(DSGraph::KeepUnreachableGlobals);
}
