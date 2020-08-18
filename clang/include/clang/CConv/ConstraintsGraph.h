//=--ConstraintsGraph.h-------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Class we use to maintain constraints in a graph form.
//
//===----------------------------------------------------------------------===//

#ifndef _CONSTRAINTSGRAPH_H
#define _CONSTRAINTSGRAPH_H

#include <queue>
#include "clang/CConv/Constraints.h"
#include "llvm/ADT/DirectedGraph.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/BreadthFirstIterator.h"

template<typename DataType, typename EdgeType>
class DataNode : public llvm::DGNode<DataNode<DataType, EdgeType>, EdgeType> {
public:
  typedef DataNode<DataType, EdgeType> NodeType;
  typedef llvm::DGNode<NodeType, EdgeType> SuperType;

  DataNode() = delete;
  DataNode(DataType D, EdgeType &E) : SuperType(E), Data(D) {}
  DataNode(DataType D) : SuperType(), Data(D) {}
  DataNode(const DataNode &N) : SuperType(N), Data(N.Data) {}
  DataNode(const DataNode &&N) : SuperType(std::move(N)), Data(N.Data) {}

  ~DataNode() {}

  DataType getData() const { return Data; }

  void addPred(EdgeType &E){ PredEdges.insert(&E); }
  const llvm::SetVector<EdgeType *> &getPreds() { return PredEdges; }

  bool isEqualTo(const DataNode &N) const { return this->Data == N.Data; }

private:
  DataType Data;

  llvm::SetVector<EdgeType *> PredEdges;
};

template<class DataType> struct BaseEdge;

template<typename DataType>
using BaseNode = DataNode<DataType, BaseEdge<DataType>>;

template <typename Data> struct llvm::GraphTraits<BaseNode<Data> *> {
  using NodeRef = BaseNode<Data> *;

  static BaseNode<Data> *GetTargetNode(BaseEdge<Data> *P) {
    return &P->getTargetNode();
  }

  // Provide a mapped iterator so that the GraphTrait-based implementations can
  // find the target nodes without having to explicitly go through the edges.
  using ChildIteratorType =
  mapped_iterator<typename BaseNode<Data>::iterator, decltype(&GetTargetNode)>;

  static NodeRef getEntryNode(NodeRef N) { return N; }
  static ChildIteratorType child_begin(NodeRef N) {
    return ChildIteratorType(N->begin(), &GetTargetNode);
  }
  static ChildIteratorType child_end(NodeRef N) {
    return ChildIteratorType(N->end(), &GetTargetNode);
  }
};

template<class DataType>
struct BaseEdge : public llvm::DGEdge<BaseNode<DataType>, BaseEdge<DataType>> {
  typedef llvm::DGEdge<BaseNode<DataType>, BaseEdge<DataType>> SuperType;

  explicit BaseEdge(BaseNode<DataType> &Node) : SuperType(Node) {}
  explicit BaseEdge(const BaseEdge &E) : SuperType(E) {}

  bool isEqualTo(const BaseEdge &E) const {
    return this->getTargetNode() == E.getTargetNode();
  }
};

template<typename Data, typename EdgeType>
class DataGraph :
    public llvm::DirectedGraph<DataNode<Data, EdgeType>, EdgeType> {
public:
  typedef DataNode<Data, EdgeType> NodeType;

  virtual ~DataGraph() {
    for (auto *N : this->Nodes) {
      for (auto *E : N->getEdges())
        delete E;
      N->getEdges().clear();
      delete N;
      N = nullptr;
    }
    this->Nodes.clear();
  }

  void forEachEdge(llvm::function_ref<void(Data,Data)> fn) const {
    for (auto *N : this->Nodes)
      for (auto *E : N->getEdges())
        fn(N->getData(), E->getTargetNode().getData());
  }

  void removeEdge(Data Src, Data Dst) {
    auto *NSrc = this->findNode(NodeType(Src));
    auto *NDst = this->findNode(NodeType(Dst));
    assert(NSrc != this->end() && NDst != this->end());
    llvm::SmallVector<EdgeType*, 10> Edges;
    (*NDst)->findEdgesTo(**NSrc, Edges);
    for (BaseEdge<Data> *E : Edges) {
      (*NDst)->removeEdge(*E);
      delete E;
    }
  }

  void addEdge(Data L, Data R) {
    NodeType *BL = this->addVertex(L);
    NodeType *BR = this->addVertex(R);

    EdgeType *BLR = new EdgeType(*BR);
    BL->addEdge(*BLR);
    EdgeType *BRL = new EdgeType(*BL);
    BR->addPred(*BRL);
  }

  bool getNeighbors(Data D, std::set<Data> &DataSet, bool Succ){
    auto *N = this->findNode(NodeType(D));
    if (N == this->end())
      return false;
    DataSet.clear();
    llvm::SetVector<EdgeType *> Edges;
    if (Succ)
      Edges = (*N)->getEdges();
    else
      Edges = (*N)->getPreds();
    for (auto *E : Edges)
      DataSet.insert(E->getTargetNode().getData());
    return !DataSet.empty();
  }

  bool getSuccessors(Data D, std::set<Data> &DataSet) {
    return getNeighbors(D, DataSet, true);
  }

  bool getPredecessors(Data D, std::set<Data> &DataSet) {
    return getNeighbors(D, DataSet, false);
  }

  void visitBreadthFirst(Data Start, llvm::function_ref<void(Data)> Fn) {
    auto *N = this->findNode(NodeType(Start));
    if (N == this->end())
      return;
    for (auto TNode : llvm::breadth_first(*N))
      Fn(TNode->getData());
  }

protected:
  virtual NodeType *addVertex(Data D) {
    NodeType *N = new NodeType(D);
    auto *OldN = this->findNode(*N);
    if (OldN != this->end()) {
      delete N;
      return *OldN;
    }
    this->addNode(*N);
    return N;
  }
};

template<typename Data>
using BaseGraph = DataGraph<Data, BaseEdge<Data>>;

typedef BaseNode<Atom *> CGNode;
typedef BaseEdge<Atom *> CGEdge;

class ConstraintsGraph : public BaseGraph<Atom *> {
public:
  void addConstraint(Geq *C, const Constraints &CS);
  std::set<ConstAtom*> &getAllConstAtoms();

protected:
  CGNode *addVertex(Atom *A) override;
private:
  friend class GraphVizOutputGraph;
  std::set<ConstAtom*> AllConstAtoms;
};

enum EdgeKind { EK_Checked, EK_Ptype };
class EdgeProperties {
public:
  EdgeProperties(EdgeKind Kind, bool IsBidirectional)
      : Kind(Kind), IsBidirectional(IsBidirectional) {}

  EdgeKind Kind;
  bool IsBidirectional;
};

class GraphvizEdge :
    public llvm::DGEdge<DataNode<Atom*, GraphvizEdge>, GraphvizEdge> {
public:
  explicit GraphvizEdge(DataNode<Atom*, GraphvizEdge> &Node, EdgeKind Type)
      : DGEdge(Node), Properties(Type, false) {}
  explicit GraphvizEdge(const GraphvizEdge &E)
      : DGEdge(E), Properties(E.Properties) {}
  EdgeProperties Properties;
};

class GraphVizOutputGraph : public DataGraph<Atom*,GraphvizEdge> {
public:
  static void dumpConstraintGraphs(const std::string &GraphDotFile,
                                   const ConstraintsGraph &Chk,
                                   const ConstraintsGraph &Pty);
  mutable std::set<std::pair<Atom *,Atom *>> DoneChecked;
  mutable std::set<std::pair<Atom *,Atom *>> DonePtyp;
private:
  void mergeConstraintGraph(const ConstraintsGraph &Graph, EdgeKind EdgeType);
  friend struct llvm::GraphTraits<GraphVizOutputGraph>;

};

template<> struct llvm::GraphTraits<GraphVizOutputGraph> {
  using NodeRef = DataNode<Atom*,GraphvizEdge> *;
  using nodes_iterator = GraphVizOutputGraph::iterator;

  static NodeRef GetTargetNode(GraphvizEdge *P) {
    return &P->getTargetNode();
  }

  using ChildIteratorType =
  mapped_iterator<typename DataNode<Atom *, GraphvizEdge>::iterator,
                  decltype(&GetTargetNode)>;

  static nodes_iterator nodes_begin(const GraphVizOutputGraph &G) {
    return const_cast<GraphVizOutputGraph&>(G).Nodes.begin();
  }

  static nodes_iterator nodes_end(const GraphVizOutputGraph &G) {
    return const_cast<GraphVizOutputGraph&>(G).Nodes.end();
  }

  static ChildIteratorType child_begin(NodeRef N) {
    return ChildIteratorType(N->begin(), &GetTargetNode);
  }

  static ChildIteratorType child_end(NodeRef N) {
    return ChildIteratorType(N->end(), &GetTargetNode);
  }
};

template<> struct llvm::DOTGraphTraits<GraphVizOutputGraph>
    : public llvm::DefaultDOTGraphTraits,
             llvm::GraphTraits<GraphVizOutputGraph> {
  DOTGraphTraits(bool simple = false) : DefaultDOTGraphTraits(simple) {}

  std::string getNodeLabel(const DataNode<Atom *, GraphvizEdge> *Node,
                           const GraphVizOutputGraph &CG);


  static std::string getEdgeAttributes
      (const DataNode<Atom *, GraphvizEdge> *Node, ChildIteratorType T,
       const GraphVizOutputGraph &CG);
};

#endif // _CONSTRAINTSGRAPH_H