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
#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/BreadthFirstIterator.h"

template<class DataType> struct BaseEdge;

template<typename DataType>
class BaseNode : public llvm::DGNode<BaseNode<DataType>, BaseEdge<DataType>> {
public:
  typedef BaseEdge<DataType> EdgeType;
  typedef BaseNode<DataType> NodeType;
  typedef llvm::DGNode<NodeType, EdgeType> SuperType;

  BaseNode() = delete;
  BaseNode(DataType D, EdgeType &E) : SuperType(E), Data(D) {}
  BaseNode(DataType D) : SuperType(), Data(D) {}
  BaseNode(const BaseNode &N) : SuperType(N), Data(N.Data) {}
  BaseNode(const BaseNode &&N) : SuperType(std::move(N)), Data(N.Data) {}

  ~BaseNode() {}

  DataType getData() const { return Data; }

  bool isEqualTo(const BaseNode &N) const { return this->Data == N.Data; }

private:
  DataType Data;
};

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

template<typename Data>
class BaseGraph : public llvm::DirectedGraph<BaseNode<Data>, BaseEdge<Data>> {
public:
  virtual ~BaseGraph() {
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
    auto *NSrc = this->findNode(BaseNode<Data>(Src));
    auto *NDst = this->findNode(BaseNode<Data>(Dst));
    assert(NSrc != this->end() && NDst != this->end());
    llvm::SmallVector<BaseEdge<Data>*, 10> Edges;
    (*NDst)->findEdgesTo(**NSrc, Edges);
    for (BaseEdge<Data> *E : Edges) {
      (*NDst)->removeEdge(*E);
      delete E;
    }
  }

  void addEdge(Data L, Data R, bool BD) {
    BaseNode<Data> *BL = this->addVertex(L);
    BaseNode<Data> *BR = this->addVertex(R);

    BaseEdge<Data> *BRL = new BaseEdge<Data>(*BL);
    this->connect(*BR, *BL, *BRL);
    if (BD) {
      BaseEdge<Data> *BLR = new BaseEdge<Data>(*BR);
      this->connect(*BL, *BR, *BLR);
    }
  }

  // Get all successors of a given Atom which are of particular type.
  bool getNeighbors(Data D, std::set<Data> &DataSet, bool Succs) {
    auto *N = this->findNode(BaseNode<Data>(D));
    if (N == this->end())
      return false;
    DataSet.clear();
    if (Succs) {
      const auto ES = (*N)->getEdges();
      for (auto *E : ES) {
        Data NodeData = E->getTargetNode().getData();
        DataSet.insert(NodeData);
      }
    } else {
      for (auto *Neighbor : this->Nodes) {
        if (Neighbor == *N)
          continue;
        if (Neighbor->hasEdgeTo(**N)) {
          Data NodeData = Neighbor->getData();
          DataSet.insert(NodeData);
        }
      }
    }
    return !DataSet.empty();
  }

  void visitBreadthFirst(Data Start, llvm::function_ref<void(Data)> Fn) {
    auto *N = this->findNode(BaseNode<Data>(Start));
    if (N == this->end())
      return;
    for (auto TNode : llvm::breadth_first(*N))
      Fn(TNode->getData());
  }

protected:
  virtual BaseNode<Data> *addVertex(Data D) {
    auto *N = new BaseNode<Data>(D);
    auto *OldN = this->findNode(*N);
    if (OldN != this->end()) {
      delete N;
      return *OldN;
    }
    this->addNode(*N);
    return N;
  }
};

typedef BaseNode<Atom *> CGNode;
typedef BaseEdge<Atom *> CGEdge;

class ConstraintsGraph : public BaseGraph<Atom *> {
public:
  void addConstraint(Geq *C, const Constraints &CS);
  std::set<ConstAtom*> &getAllConstAtoms();

protected:
  CGNode *addVertex(Atom *A) override;
private:
  std::set<ConstAtom*> AllConstAtoms;
};


// Used during debugging to create a single graph that contains edges and nodes
// from all constraint graphs. This single graph can then be printed to a file
// in graphviz format.
//enum EdgeType { Checked, Ptype };
//class EdgeProperties {
//public:
//  EdgeProperties(EdgeType Type, bool IsBidirectional);
//  EdgeType Type;
//  bool IsBidirectional;
//};
//
//class GraphVizOutputGraph
//    : public BaseGraph<
//        boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS,
//                                Atom *, EdgeProperties *>> {
//public:
//  typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS,
//                                Atom *, EdgeProperties *>
//      DirectedGraphType;
//
//  void mergeConstraintGraph(const ConstraintsGraph &Graph, EdgeType EdgeType);
//
//  // Dump the graph to stdout in a dot format.
//  void dumpCGDot(const std::string& GraphDotFile);
//
//  static void dumpConstraintGraphs(const std::string &GraphDotFile,
//                                   const ConstraintsGraph &Chk,
//                                   const ConstraintsGraph &Pty);
//
//private:
//  const std::string EdgeTypeColors[2] = { "red", "blue" };
//  const std::string EdgeDirections[2] = { "forward", "both" };
//};

#endif // _CONSTRAINTSGRAPH_H
