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

#include "clang/3C/Constraints.h"
#include "llvm/ADT/BreadthFirstIterator.h"
#include "llvm/ADT/DirectedGraph.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/Support/GraphWriter.h"

template <class DataType> struct DataEdge;

template <typename DataType, typename EdgeType = DataEdge<DataType>>
class DataNode : public llvm::DGNode<DataNode<DataType, EdgeType>, EdgeType> {
public:
  typedef DataNode<DataType, EdgeType> NodeType;
  typedef llvm::DGNode<NodeType, EdgeType> SuperType;

  DataNode() = delete;
  DataNode(DataType D, EdgeType &E) : SuperType(E), Data(D) {}
  explicit DataNode(DataType D) : SuperType(), Data(D) {}
  DataNode(const DataNode &N) : SuperType(N), Data(N.Data) {}
  DataNode(const DataNode &&N) : SuperType(std::move(N)), Data(N.Data) {}

  ~DataNode() = default;

  DataType getData() const { return Data; }

  void connectTo(NodeType &Other) {
    auto *BLR = new EdgeType(Other);
    this->addEdge(*BLR);
    auto *BRL = new EdgeType(*this);
    Other.addPredecessor(*BRL);
  }

  const llvm::SetVector<EdgeType *> &getPredecessors() {
    return PredecessorEdges;
  }

  // Nodes are defined exactly by the data they contain, not by their
  // connections to other nodes.
  bool isEqualTo(const DataNode &N) const { return this->Data == N.Data; }

private:
  // Data element stored in each node. This is used by isEqualTo to discriminate
  // between nodes.
  DataType Data;

  // While the constraint graph is directed, we want to efficiently traverse
  // edges in the opposite direction. This set contains an edge entry pointing
  // back to every node that has an edge to this node.
  llvm::SetVector<EdgeType *> PredecessorEdges;
  void addPredecessor(EdgeType &E) { PredecessorEdges.insert(&E); }
};

namespace llvm {
// Boilerplate template specialization
template <typename Data, typename EdgeType>
struct GraphTraits<DataNode<Data, EdgeType> *> {
  using NodeRef = DataNode<Data, EdgeType> *;

  static NodeRef GetTargetNode(EdgeType *P) { return &P->getTargetNode(); }

  using ChildIteratorType =
      mapped_iterator<typename DataNode<Data, EdgeType>::iterator,
                      decltype(&GetTargetNode)>;

  static NodeRef getEntryNode(NodeRef N) { return N; }
  static ChildIteratorType child_begin(NodeRef N) {
    return ChildIteratorType(N->begin(), &GetTargetNode);
  }
  static ChildIteratorType child_end(NodeRef N) {
    return ChildIteratorType(N->end(), &GetTargetNode);
  }
};
} // namespace llvm

template <class DataType>
struct DataEdge : public llvm::DGEdge<DataNode<DataType>, DataEdge<DataType>> {
  typedef llvm::DGEdge<DataNode<DataType>, DataEdge<DataType>> SuperType;
  explicit DataEdge(DataNode<DataType> &Node) : SuperType(Node) {}
  DataEdge(const DataEdge &E) : SuperType(E) {}
};

class GraphVizOutputGraph;

// Define a general purpose extension to the llvm provided graph class that
// stores some data at each node in the graph. This is used by the checked and
// pointer type constraint graphs (which store atoms at each node) as well as
// the array bounds graph (which stores BoundsKeys).
template <typename Data, typename EdgeType = DataEdge<Data>>
class DataGraph
    : public llvm::DirectedGraph<DataNode<Data, EdgeType>, EdgeType> {
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
    invalidateBFSCache();
  }

  void removeEdge(Data Src, Data Dst) {
    auto *NSrc = this->findNode(NodeType(Src));
    auto *NDst = this->findNode(NodeType(Dst));
    assert(NSrc != this->end() && NDst != this->end());
    llvm::SmallVector<EdgeType *, 10> Edges;
    (*NDst)->findEdgesTo(**NSrc, Edges);
    for (EdgeType *E : Edges) {
      (*NDst)->removeEdge(*E);
      delete E;
    }
    invalidateBFSCache();
  }

  void addEdge(Data L, Data R) {
    NodeType *BL = this->findOrCreateNode(L);
    NodeType *BR = this->findOrCreateNode(R);
    BL->connectTo(*BR);
    invalidateBFSCache();
  }

  void addUniqueEdge(Data L, Data R) {
    NodeType *BL = this->findOrCreateNode(L);
    NodeType *BR = this->findOrCreateNode(R);
    llvm::SmallVector<EdgeType *, 10> Edges;
    BL->findEdgesTo(*BR, Edges);
    if (Edges.empty()) {
      addEdge(L, R);
    }
  }

  bool getNeighbors(Data D, std::set<Data> &DataSet, bool Succ) {
    auto *N = this->findNode(NodeType(D));
    if (N == this->end())
      return false;
    DataSet.clear();
    llvm::SetVector<EdgeType *> Edges;
    if (Succ)
      Edges = (*N)->getEdges();
    else
      Edges = (*N)->getPredecessors();
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
    // Insert into BFS cache.
    if (BFSCache.find(Start) == BFSCache.end()) {
      std::set<Data> ReachableNodes;
      for (auto TNode : llvm::breadth_first(*N)) {
        ReachableNodes.insert(TNode->getData());
      }
      BFSCache[Start] = ReachableNodes;
    }
    for (auto SN : BFSCache[Start])
      Fn(SN);
  }

protected:
  // Finds the node containing the Data if it exists, otherwise a new Node
  // is allocated. Node equality is defined only by the data stored in a node,
  // so if any node already contains the data, this node will be found.
  virtual NodeType *findOrCreateNode(Data D) {
    auto *OldN = this->findNode(NodeType(D));
    if (OldN != this->end())
      return *OldN;

    auto *NewN = new NodeType(D);
    this->addNode(*NewN);
    return NewN;
  }

private:
  template <typename G> friend struct llvm::GraphTraits;
  friend class GraphVizOutputGraph;
  std::map<Data, std::set<Data>> BFSCache;

  void invalidateBFSCache() { BFSCache.clear(); }
};

// Specialize the graph for the checked and pointer type constraint graphs. This
// graphs stores atoms at each node.
class ConstraintsGraph : public DataGraph<Atom *> {
public:
  // Add an edge to the graph according to the Geq constraint. This is an edge
  // RHSAtom -> LHSAtom
  void addConstraint(Geq *C, const Constraints &CS);

  // Const atoms are the starting points for the solving algorithm so, we need
  // be able to retrieve them from the graph.
  std::set<ConstAtom *> &getAllConstAtoms();

protected:
  // Add vertex is overridden to save const atoms as they are added to the graph
  // so that getAllConstAtoms can efficiently retrieve them.
  NodeType *findOrCreateNode(Atom *A) override;

private:
  std::set<ConstAtom *> AllConstAtoms;
};

// Below this point we define a graph class specialized for generating the
// graphviz output for the combine checked and ptype graphs.

// Once we combine the graphs, we need to know if an edge came from the ptype
// or the checked constraint graph. We also combine edges to render a pair of
// edges pointing in different directions between the same pair of nodes as a
// single bidirectional edge.
class GraphVizEdge
    : public llvm::DGEdge<DataNode<Atom *, GraphVizEdge>, GraphVizEdge> {
public:
  enum EdgeKind { EK_Checked, EK_Ptype };
  explicit GraphVizEdge(DataNode<Atom *, GraphVizEdge> &Node, EdgeKind Kind)
      : DGEdge(Node), Kind(Kind), IsBidirectional(false) {}
  GraphVizEdge(const GraphVizEdge &E)
      : DGEdge(E), Kind(E.Kind), IsBidirectional(E.IsBidirectional) {}

  EdgeKind Kind;
  bool IsBidirectional;
};

// The graph subclass for graphviz output uses the specialized edge class to
// hold the extra information required. It also provides the methods for
// merging the checked and ptype graphs as well as a static function to do the
// actual conversion and output to file.
class GraphVizOutputGraph : public DataGraph<Atom *, GraphVizEdge> {
public:
  // Merge the provided graphs and dump the graphviz visualization to the given
  // file name. The first graph argument is the checked graph while the second
  // is the pointer type graph.
  static void dumpConstraintGraphs(const std::string &GraphDotFile,
                                   const ConstraintsGraph &Chk,
                                   const ConstraintsGraph &Pty);

  // These maps are used because the graphviz utility provided by llvm does not
  // give an easy way to differentiate between multiple edges between the same
  // pair of nodes. When there is both a checked an a ptype edge between two
  // nodes, these maps ensure that each is output exactly once and has the
  // correct color when it is output.
  mutable std::set<std::pair<Atom *, Atom *>> DoneChecked;
  mutable std::set<std::pair<Atom *, Atom *>> DonePtyp;

private:
  void mergeConstraintGraph(const ConstraintsGraph &Graph,
                            GraphVizEdge::EdgeKind EdgeType);
};

// Below this is boiler plate needed to work with the llvm graphviz output
// functions.

namespace llvm {
template <> struct GraphTraits<GraphVizOutputGraph> {
  using NodeRef = DataNode<Atom *, GraphVizEdge> *;
  using nodes_iterator = GraphVizOutputGraph::iterator;

  static NodeRef GetTargetNode(GraphVizEdge *P) { return &P->getTargetNode(); }

  using ChildIteratorType =
      mapped_iterator<typename DataNode<Atom *, GraphVizEdge>::iterator,
                      decltype(&GetTargetNode)>;

  static nodes_iterator nodes_begin(const GraphVizOutputGraph &G) {
    return const_cast<GraphVizOutputGraph &>(G).Nodes.begin();
  }

  static nodes_iterator nodes_end(const GraphVizOutputGraph &G) {
    return const_cast<GraphVizOutputGraph &>(G).Nodes.end();
  }

  static ChildIteratorType child_begin(NodeRef N) {
    return {N->begin(), &GetTargetNode};
  }

  static ChildIteratorType child_end(NodeRef N) {
    return {N->end(), &GetTargetNode};
  }
};

template <>
struct DOTGraphTraits<GraphVizOutputGraph>
    : public llvm::DefaultDOTGraphTraits,
      llvm::GraphTraits<GraphVizOutputGraph> {
  DOTGraphTraits(bool simple = false) : DefaultDOTGraphTraits(simple) {}

  std::string getNodeLabel(const DataNode<Atom *, GraphVizEdge> *Node,
                           const GraphVizOutputGraph &CG);
  static std::string
  getEdgeAttributes(const DataNode<Atom *, GraphVizEdge> *Node,
                    ChildIteratorType T, const GraphVizOutputGraph &CG);
};
} // namespace llvm

#endif // _CONSTRAINTSGRAPH_H