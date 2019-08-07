//===------ ExpandingCycles - Support Methods for Detecting Expanding Cycles  -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements methods for detecting expanding cycles. Expanding cycles
//  are cycles in the dependencies between generic structs that prevent
//  definitions of some generics.
//
//===----------------------------------------------------------------------===//

#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaDiagnostic.h"
#include "clang/AST/RecursiveASTVisitor.h"

using namespace clang;
using namespace sema;

// Definitions for expanding cycles check
namespace {
  // A graph node is a triple '(BaseRecordDecl, TypeArgIndex, Expanding)' (here stored as two nested 'std::pairs' so we can keep them in a 'DenseSet').
  // The semantics of a triple are as follows: a triple is in the set if, starting from one of the type arguments of 'Base', 
  // it's possible to arrive at the type argument with index 'TypeArgIndex' which is defined in 'BaseRecordDecl'.
  // 'Expanding' indicates whether any of the edges taken to arrive to (BaseRecordDecl, TypeArgIndex) is expanding ('Expanding = 1')
  // or if they're all non-expanding ('Expanding = 0'). 
  using Node = std::pair<std::pair<const RecordDecl *, int>, char>;
  constexpr char NON_EXPANDING = 0;
  constexpr char EXPANDING = 1;

  // Retrieve the underlying type variable from a typedef that appears as the param to a generic record.
  const TypeVariableType *GetTypeVar(TypedefDecl *TDef) {
    const TypeVariableType *TypeVar = llvm::dyn_cast<TypeVariableType>(TDef->getUnderlyingType());
    assert(TypeVar && "Expected a type variable as the parameter of a generic record");
    return TypeVar;
  }

  /// A visitor that determines whether a type references a given type variable.
  /// Even though this class is a RecursiveASTVisitor, the entry point to call is
  /// the 'ContainsTypeVar' method.
  ///
  /// e.g. ContainsTypeVarVisitor().ContainsTypeVar('List<T>', 'T') == true
  ///      ContainsTypeVarVisitor().ContainsTypeVar('List<int>', 'T') == false
  class ContainsTypeVarVisitor : public RecursiveASTVisitor<ContainsTypeVarVisitor> {
    /// The type variable we're searching for.
    /// This is set by 'ContainsTypeVar' before every search.
    const TypeVariableType *TypeVar = nullptr;
    /// Whether the type contains the type variable we're looking for.
    /// Set after calling 'RecursiveASTVisitor::TraverType'.
    bool ContainsTV = false;

  public:
    /// Determine whether 'Type' contains within it any references to 'TypeVar'.
    bool ContainsTypeVar(QualType Type, const TypeVariableType *TypeVar) {
      this->TypeVar = TypeVar;
      this->ContainsTV = false;
      TraverseType(Type);
      return ContainsTV;
    }

    bool TraverseTypeVariableType(const TypeVariableType *Type) {
      ContainsTV = ContainsTV || (Type == TypeVar);
      return true;
    }

    // The following methods are needed because 'RecursiveASTVisitor' doesn't
    // know how to decompose the corresponding types.
   
    bool TraverseTypedefType(const TypedefType *Type) {
      return TraverseType(Type->desugar());
    }

    bool TraverseRecordType(const RecordType *Type) {
      auto RDecl = Type->getDecl();
      if (RDecl->isInstantiated()) {
        for (auto TArg : RDecl->typeArgs()) TraverseType(TArg.typeName);
      }
      return true;
    }
  };

  /// A visitor that, given a type and a type variable, generates out-edges from the
  /// type variable in the expanding cycles graph.
  /// To generate the edges, we need to destruct the given type and find within it all type
  /// applications where the variable appears. The resulting edges are "expanding" or "non-expanding"
  /// depending on whether the variable appears at the top level as a type argument.
  ///
  /// The new edges aren't returned; instead, they're added as a side effect to the
  /// 'Worklist' argument in the constructor.
  class ExpandingEdgesVisitor : public RecursiveASTVisitor<ExpandingEdgesVisitor> {
    /// The worklist where the new nodes will be inserted.
    std::stack<Node> &Worklist;
    /// The type variable that we're looking for in embedded type applications.
    const TypeVariableType *TypeVar = nullptr;
    /// Whether the path so far contains at least one expanding edge.
    char ExpandingSoFar = NON_EXPANDING;
    /// A visitor object to find out whether a type variable is referenced within a given type.
    ContainsTypeVarVisitor ContainsVisitor;

  public:
    /// Note the worklist argument is mutated by this visitor.
    ExpandingEdgesVisitor(std::stack<Node>& Worklist, const TypeVariableType *TypeVar, char ExpandingSoFar):
      Worklist(Worklist), TypeVar(TypeVar), ExpandingSoFar(ExpandingSoFar) {}

    void AddEdges(QualType Type) {
      TraverseType(Type);
    }

    bool TraverseRecordType(const RecordType *Type) {
      auto InstDecl = Type->getDecl();
      if (!InstDecl->isInstantiated()) return true;
      auto BaseDecl = InstDecl->genericBaseDecl();
      assert(InstDecl->typeArgs().size() == BaseDecl->typeParams().size() && "Number of type args and params must match");
      auto NumArgs = InstDecl->typeArgs().size();
      for (size_t i = 0; i < NumArgs; i++) {
        auto TypeArg = InstDecl->typeArgs()[i].typeName.getCanonicalType();
        auto DestTypeVar = GetTypeVar(BaseDecl->typeParams()[i]);
        auto DestIndex = DestTypeVar->GetIndex();
        if (TypeArg.getTypePtr() == TypeVar) {
          // Non-expanding edges are created if the type variable appears directly as an argument of the decl.
          // So in this case the new edge is marked as expanding only if we'd previously seen an expanding edge.
          Worklist.push(Node(std::make_pair(BaseDecl, DestIndex), ExpandingSoFar));
        } else if (ContainsVisitor.ContainsTypeVar(TypeArg, TypeVar)) {
          // Expanding edges are created if the type variable doesn't appear directly, but is contained in the type argument.
          // In this case we always mark the edge as expanding.
          Worklist.push(Node(std::make_pair(BaseDecl, DestIndex), EXPANDING));
        }

        // Now recurse in the type argument to uncover edges that might show up there.
        // e.g. as in the argument to 'struct A<struct B<struct B<T> > >'
        TraverseType(TypeArg);
      }
      return true;
    }

    bool TraverseTypedefType(const TypedefType *Type) {
      return TraverseType(Type->desugar());
    }
  };
}

bool Sema::DiagnoseExpandingCycles(RecordDecl *Base, SourceLocation Loc) {
  assert(Base->isGeneric() && "Can only check expanding cycles for generic structs");
  assert(Base == Base->getCanonicalDecl() && "Expected canonical base decl");
  llvm::DenseSet<Node> Visited;
  std::stack<Node> Worklist;

  // Seed the worklist with the type parameters to 'Base'.
  for (size_t i = 0; i < Base->typeParams().size(); ++i) {
    Worklist.push(Node(std::make_pair<>(Base, i), NON_EXPANDING));
  }

  // Explore the implicit graph via DFS.
  while (!Worklist.empty()) {
    auto Curr = Worklist.top();
    Worklist.pop();
    if (Visited.find(Curr) != Visited.end()) continue; // already visited: don't explore further
    Visited.insert(Curr);
    auto RDecl = Curr.first.first;
    auto TVarIndex = Curr.first.second;
    auto TVar = GetTypeVar(RDecl->typeParams()[TVarIndex]);
    auto ExpandingSoFar = Curr.second;
    if (ExpandingSoFar == EXPANDING && RDecl == Base) {
      Diag(Loc, diag::err_expanding_cycle);
      return true;
    }
    ExpandingEdgesVisitor EdgesVisitor(Worklist, TVar, ExpandingSoFar);
    auto Defn = RDecl->getDefinition();
    // There might not be an underlying definition, because 'RDecl' might refer
    // to a forward-declared struct.
    if (!Defn) continue;
    for (auto Field : Defn->fields()) {
      // 'EdgesVisitor' mutates the worklist by adding new nodes to it.
      EdgesVisitor.AddEdges(Field->getType());
    }
  }

  return false; // no cycles: can complete decls
}
