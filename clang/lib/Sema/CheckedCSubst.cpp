//===------ CheckedCInterop - Support Methods for Checked C Interop  -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements methods for doing type substitution and parameter
//  subsitution during semantic analysis.  This is used when typechecking
//  generic type application and checking bounds.
//
//===----------------------------------------------------------------------===//

#include "TreeTransform.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Sema/Sema.h"
#include "llvm/ADT/DenseSet.h"

using namespace clang;
using namespace sema;

ExprResult Sema::ActOnFunctionTypeApplication(ExprResult TypeFunc, SourceLocation Loc,
  ArrayRef<TypeArgument> TypeArgs) {

  TypeFunc = CorrectDelayedTyposInExpr(TypeFunc);
  if (!TypeFunc.isUsable())
    return ExprError();

  // Make sure we have a generic function or function with a bounds-safe
  // interface.

  // TODO: generalize this
  DeclRefExpr *declRef = dyn_cast<DeclRefExpr>(TypeFunc.get());
  if (!declRef) {
    Diag(Loc, diag::err_type_args_limited);
    return ExprError();
  }

  const FunctionProtoType *funcType =
    declRef->getType()->getAs<FunctionProtoType>();

  if (!funcType) {
    Diag(Loc, diag::err_type_args_for_non_generic_expression);
    return ExprError();
  }

  // Make sure that the number of type names equals the number of type variables in 
  // the function type.
  if (funcType->getNumTypeVars() != TypeArgs.size()) {
    FunctionDecl* funDecl = dyn_cast<FunctionDecl>(declRef->getDecl());
    if (!funcType->isGenericFunction() && !funcType->isItypeGenericFunction()) {
      Diag(Loc,
       diag::err_type_args_for_non_generic_expression);
      // TODO: emit a note pointing to the declaration.
      return true;
    }

    // The location of beginning of _For_any is stored in typeVariables
    Diag(Loc,
      diag::err_type_list_and_type_variable_num_mismatch);

    if (funDecl)
      Diag(funDecl->typeVariables()[0]->getBeginLoc(),
        diag::note_type_variables_declared_at);

    return ExprError();
  }
  // Add parsed list of type names to declRefExpr for future references
  declRef->SetGenericInstInfo(Context, TypeArgs);

  // Substitute type arguments for type variables in the function type of the
  // DeclRefExpr.
  QualType NewTy = SubstituteTypeArgs(declRef->getType(), TypeArgs);
  declRef->setType(NewTy);
  return declRef;

}

// Type Instantiation
RecordDecl* Sema::ActOnRecordTypeApplication(RecordDecl *Base, ArrayRef<TypeArgument> TypeArgs) {
  assert(Base->isGenericOrItypeGeneric() && "Base decl must be generic in a type application");
  assert(Base->getCanonicalDecl() == Base && "Base must be canonical decl");
 
  auto &ctx = Base->getASTContext();

  // Unwrap the type arguments from a 'TypeArgument' to the underlying 'Type *'.
  llvm::SmallVector<const Type *, 4> RawArgs;
  for (auto TArg : TypeArgs) {
    // Use the canonical type of the argument when caching. Otherwise, the types of
    // s1 and s2 below are considered different, when they shouldn't.
    //   typedef int1 int;
    //   typedef int2 int;
    //   struct Foo<int1> s1;
    //   struct Foo<int2> s2;
    //   s1 = s2;
    RawArgs.push_back(TArg.typeName.getCanonicalType().getTypePtr());
  }

  // If possible, just retrieve the application from the cache.
  // This is needed not only for performance, but for correctness to handle
  // recursive references in type applications (e.g. a list which contains a list as a field).
  if (auto Cached = ctx.getCachedTypeApp(Base, RawArgs)) {
    return Cached;   
  }

  // Notice we pass dummy location arguments, since the type application doesn't exist in user code.
  RecordDecl *Inst = RecordDecl::Create(ctx, Base->getTagKind(), Base->getDeclContext(), SourceLocation(), SourceLocation(),
    Base->getIdentifier(), Base->getPreviousDecl(), RecordDecl::NonGeneric, ArrayRef<TypedefDecl *>(nullptr, static_cast<size_t>(0)) /* TypeParams */, Base, TypeArgs);

  // Add the new instance to the base's context, so that the instance is discoverable
  // by AST traversal operations: e.g. the AST dumper.
  Base->getDeclContext()->addDecl(Inst);

  // Cache the application early on before we tinker with the fields, in case
  // one of the fields refers back to the application.
  ctx.addCachedTypeApp(Base, RawArgs, Inst);

  auto Defn = Base->getDefinition();
  if (Defn && Defn->isCompleteDefinition()) {
    // If the underlying definition exists and has all its fields already populated, then we can complete
    // the type application.
    CompleteTypeAppFields(Inst);
  } else {
    // If the definition isn't available, it might be because we're typing a recursive struct declaration: e.g.
    // struct List _For_any(T) {
    //   struct List<T> *next;
    //   T *head;
    // };
    // While processing 'next', we can't instantiate 'List<T>' because we haven't processed the 'head' field yet.
    // The solution is to mark the application as "delayed" and "complete it" after we've parsed all the fields.
    Inst->setDelayedTypeApp(true);
    ctx.addDelayedTypeApp(Inst);
  }

  return Inst;
}

void Sema::CompleteTypeAppFields(RecordDecl *Incomplete) {
  assert(Incomplete->isInstantiated() && "Only instantiated record decls can be completed");
  assert(Incomplete->field_empty() && "Can't complete record decl with non-empty fields");

  auto Defn = Incomplete->genericBaseDecl()->getDefinition();
  assert(Defn && "The record definition should be populated at this point");
  for (auto *Field : Defn->fields()) {
    QualType InstType = SubstituteTypeArgs(Field->getType(), Incomplete->typeArgs());
    assert(!InstType.isNull() && "Subtitution of type args failed!");
    // TODO: are TypeSouceInfo and InstType in sync?
    FieldDecl *NewField = FieldDecl::Create(Field->getASTContext(), Incomplete, SourceLocation(), SourceLocation(),
      Field->getIdentifier(), InstType, Field->getTypeSourceInfo(), Field->getBitWidth(), Field->isMutable(), Field->getInClassInitStyle());

    // Substitute in the bounds-safe interface type (itype).
    if (auto IType = Field->getInteropTypeExpr()) {
      auto InstType = SubstituteTypeArgs(IType->getType(), Incomplete->typeArgs());
      InteropTypeExpr *NewIType = new (Context) InteropTypeExpr(InstType, SourceLocation(), SourceLocation(), IType->getTypeInfoAsWritten());
      NewField->setInteropTypeExpr(Context, NewIType);
    }
    // TODO: substitute type arguments in bounds expressions as well.
    //
    // Incomplete solution for now: leave free variables in bounds expressions.
    // Type variables can appear in the bounds expression, but the only relevant information they provide
    // is their size. However, since type variables represent incomplete types, they will only
    // appear in the bounds expression as pointers, whose size is known (equivalent to 'void *').
    // This allows us to not do any replacements in bounds expressions.
    // e.g. `_Array_ptr<int> a : count(sizeof(T*))` will be erased to `sizeof(void*)`,
    // which is correct because 'sizeof(T *) = sizeof(void *)' for all 'T'.
    NewField->setBoundsExpr(Context, Field->getBoundsExpr());

    Incomplete->addDecl(NewField);
  }

  Incomplete->setDelayedTypeApp(false);
  Incomplete->setCompleteDefinition();
}

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
  assert(Base->isGenericOrItypeGeneric() && "Can only check expanding cycles for generic structs");
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
    // 'EdgesVisitor' mutates the worklist by adding new nodes to it.
    ExpandingEdgesVisitor EdgesVisitor(Worklist, TVar, ExpandingSoFar);
    auto Defn = RDecl->getDefinition();
    // There might not be an underlying definition, because 'RDecl' might refer
    // to a forward-declared struct.
    if (!Defn) continue;
    for (auto Field : Defn->fields()) {
      // Visit the field's type.
      EdgesVisitor.AddEdges(Field->getType());
      // Visit the field's interop type, if one exists.
      auto Itype = Field->getInteropType();
      if (!Itype.isNull()) EdgesVisitor.AddEdges(Itype);
    }
  }

  return false; // no cycles: can complete decls
}

namespace {
// LocRebuilderTransform is an uncustomized 'TreeTransform' that is used
// solely for re-building 'TypeLocs' within 'TypeApplication'.
// We use this vanilla transform instead of a recursive call to 'TypeApplication::TransformType' because
// we sometimes substitute a type variable for another type variable, and in those cases we
// want to re-build 'TypeLocs', but not do further substitutions.
// e.g.
//   struct Box _For_any(U) { U *x; }
//   struct List _For_any(T) { struct Box<T> box; }
//
// When typing 'Box<T>', we need to substitute 'T' for 'U' in 'Box'.
// T and U end up with the same representation in the IR because we use an
// index-based representation for variables, not a name-based representation.
class LocRebuilderTransform : public TreeTransform<LocRebuilderTransform> {
public:
  LocRebuilderTransform(Sema& SemaRef) : TreeTransform<LocRebuilderTransform>(SemaRef) {}
};

class TypeApplication : public TreeTransform<TypeApplication> {
  typedef TreeTransform<TypeApplication> BaseTransform;
  typedef ArrayRef<TypeArgument> TypeArgList;

private:
  TypeArgList TypeArgs;
  unsigned Depth;
  LocRebuilderTransform LocRebuilder;

public:
  TypeApplication(Sema &SemaRef, TypeArgList TypeArgs, unsigned Depth) :
    BaseTransform(SemaRef), TypeArgs(TypeArgs), Depth(Depth), LocRebuilder(SemaRef) {}

  QualType TransformTypeVariableType(TypeLocBuilder &TLB,
                                     TypeVariableTypeLoc TL) {
    const TypeVariableType *TV = TL.getTypePtr();
    unsigned TVDepth = TV->GetDepth();

    if (TVDepth < Depth) {
      // Case 1: the type variable is bound by a type quantifier (_Forall scope)
      // that lexically encloses the type quantifier that is being applied.
      // Nothing changes in this case.
      QualType Result = TL.getType();
      TypeVariableTypeLoc NewTL = TLB.push<TypeVariableTypeLoc>(Result);
      NewTL.setNameLoc(TL.getNameLoc());
      return Result;
    } else if (TVDepth == Depth) {
      // Case 2: the type variable is bound by the type quantifier that is
      // being applied.  Substitute the appropriate type argument.
      TypeArgument TypeArg = TypeArgs[TV->GetIndex()];
      TypeLoc NewTL =  TypeArg.sourceInfo->getTypeLoc();
      TLB.reserve(NewTL.getFullDataSize());
      // Run the type transform with the type argument's location information
      // so that the type location class pushed on to the TypeBuilder is the
      // matching class for the transformed type.
      QualType Result = LocRebuilder.TransformType(TLB, NewTL);
      // We don't expect the type argument to change.
      assert(Result == TypeArg.typeName);
      return Result;
    } else {
      // Case 3: the type variable is bound by a type quantifier nested within the
      // the one that is being applied.  Create a type variable with a decremented
      // depth, to account for the removal of the enclosing scope.
      QualType Result =
         SemaRef.Context.getTypeVariableType(TVDepth - 1, TV->GetIndex(),
                                             TV->IsBoundsInterfaceType());
      TypeVariableTypeLoc NewTL = TLB.push<TypeVariableTypeLoc>(Result);
      NewTL.setNameLoc(TL.getNameLoc());
      return Result;
    }
  }

  QualType TransformTypedefType(TypeLocBuilder &TLB, TypedefTypeLoc TL) {
    // Preserve typedef information, unless the underlying type has a type
    // variable embedded in it.
    const TypedefType *T = TL.getTypePtr();
    // See if the underlying type changes.
    QualType UnderlyingType = T->desugar();
    QualType TransformedType = getDerived().TransformType(UnderlyingType);
    if (UnderlyingType == TransformedType) {
      QualType Result = TL.getType();
      // It didn't change, so just copy the original type location information.
      TypedefTypeLoc NewTL = TLB.push<TypedefTypeLoc>(Result);
      NewTL.setNameLoc(TL.getNameLoc());
      return Result;
    }
    // Something changed, so we need to delete the typedef type from the AST and
    // and use the underlying transformed type.

    // Synthesize some dummy type source information.
    TypeSourceInfo *DI = getSema().Context.getTrivialTypeSourceInfo(UnderlyingType,
                                                getDerived().getBaseLocation());
    // Use that to get dummy location information.
    TypeLoc NewTL = DI->getTypeLoc();
    TLB.reserve(NewTL.getFullDataSize());
    // Re-run the type transformation with the dummy location information so
    // that the type location class pushed on to the TypeBuilder is the matching
    // class for the underlying type.
    QualType Result = getDerived().TransformType(TLB, NewTL);
    if (Result != TransformedType) {
      llvm::outs() << "Dumping transformed type:\n";
      Result.dump(llvm::outs());
      llvm::outs() << "Dumping result:\n";
      Result.dump(llvm::outs());
    }
    return Result;
  }

  Decl *TransformDecl(SourceLocation Loc, Decl *D) {
    RecordDecl *RDecl;
    if ((RDecl = dyn_cast<RecordDecl>(D)) && RDecl->isInstantiated()) {
      llvm::SmallVector<TypeArgument, 4> NewArgs;
      for (auto TArg : RDecl->typeArgs()) {
        auto NewType = SemaRef.SubstituteTypeArgs(TArg.typeName, TypeArgs);
        auto *SourceInfo = getSema().Context.getTrivialTypeSourceInfo(NewType, getDerived().getBaseLocation());
        NewArgs.push_back(TypeArgument { NewType, SourceInfo });
      }
      auto *Res = SemaRef.ActOnRecordTypeApplication(RDecl->genericBaseDecl(), ArrayRef<TypeArgument>(NewArgs));
      return Res;
    } else {
      return BaseTransform::TransformDecl(Loc, D);
    }
  }
};
}

QualType Sema::SubstituteTypeArgs(QualType QT, ArrayRef<TypeArgument> TypeArgs) {
   if (QT.isNull())
     return QT;

   // Transform the type and strip off the quantifier.
   TypeApplication TypeApp(*this, TypeArgs, 0 /* Depth */);
   QualType TransformedQT = TypeApp.TransformType(QT);

   // Something went wrong in the transformation.
   if (TransformedQT.isNull())
     return QT;

   if (const FunctionProtoType * FPT = TransformedQT->getAs<FunctionProtoType>()) {
     FunctionProtoType::ExtProtoInfo EPI = FPT->getExtProtoInfo();
     EPI.GenericFunction = false;
     EPI.ItypeGenericFunction = false;
     EPI.NumTypeVars = 0;
     QualType Result =
       Context.getFunctionType(FPT->getReturnType(), FPT->getParamTypes(), EPI);
     return Result;
   }

   return TransformedQT;
}

namespace {
/// A `TreeTransform` that computes the list of free `TypeVariableTypes` in a type.
/// A variable is free if isn't bound.
/// Example:
///   FreeVariablesFinder().find(_Exists(3, _Exists(4, struct Foo<1, 3, struct Foo<4, 0>>)))
///   ==> [1, 0] (3 and 4 are bound)
/// TODO: add list of variable binders
class FreeVariablesFinder : public TreeTransform<FreeVariablesFinder> {

private:
  typedef TreeTransform<FreeVariablesFinder> BaseTransform;

  /// The set of current bound variables
  llvm::DenseSet<const TypeVariableType *> BoundVars;
  /// The set of free variables found so far
  llvm::DenseSet<const TypeVariableType *> FreeVars;
  /// The set of free typedef declarations found so far
  llvm::DenseSet<const TypedefNameDecl *> FreeTypedefDecls;
  ASTContext &Context;

public:
  FreeVariablesFinder(Sema &SemaRef) : BaseTransform(SemaRef), Context(SemaRef.Context) {}

  /// Returns the list of free type variables referenced in the given type.
  std::vector<const TypeVariableType *> find(QualType Ty) {
    getDerived().TransformType(Ty); // Populates `FreeVars` as a side effect.
    return std::vector<const TypeVariableType *>(FreeVars.begin(), FreeVars.end());
  }

  /// Returns the list of free typedef declarations referenced in the given type.
  /// Typedef declarations enable more readable diagnostics than type variable types.
  std::vector<const TypedefNameDecl *> findTypedefDecls(QualType Ty) {
    getDerived().TransformType(Ty); // Populates `FreeTypedefDecls` as a side effect.
    return std::vector<const TypedefNameDecl *>(FreeTypedefDecls.begin(), FreeTypedefDecls.end());
  }

  // The TransformType* static overrides are below.
  // For each one, we just want to compute bound and free variables. We're not
  // really interested in transforming the type, so we always ultimately return
  // whatever the base class returns.

  /// For a type variable, add it to the list of free variables if it isn't bound.
  QualType TransformTypeVariableType(TypeLocBuilder &TLB, TypeVariableTypeLoc TL) {
    // We don't need to call `.getCanonical()` here because type variables
    // are their own canonical types.
    auto *TypeVar = TL.getTypePtr()->getAs<TypeVariableType>();
    if (BoundVars.find(TypeVar) == BoundVars.end()) FreeVars.insert(TypeVar);
    return BaseTransform::TransformTypeVariableType(TLB, TL);
  }

  /// For an existential type, add the introduced variable to the set of bound variables
  /// and recurse on the inner type.
  QualType TransformExistentialType(TypeLocBuilder &TLB, ExistentialTypeLoc TL) {
    // What we want is to extract the new bound type variable.
    // We go about it carefully: we can't call 'TL.getType().getCanonical()' because
    // canonical types for existentials aren't compositional.
    // Instead, we get the "raw" type.
    auto *ExistType = TL.getTypePtr();
    // We call '.getCanonical()' because the type in the variable position
    // in an existential could be either a `TypedefType` or a `TypeVariableType`.
    // In the former case, we want to make sure we get the raw type variable.
    auto *TypeVar = Context.getCanonicalType(ExistType->typeVar())->getAs<TypeVariableType>();
    if (!TypeVar) llvm_unreachable("expected a TypeVariableType");
    BoundVars.insert(TypeVar);
    return BaseTransform::TransformExistentialType(TLB, TL);
  }

  /// For a `TypedefType`, if it declares a free type variable,
  /// add its declaration to the set of free declarations.
  QualType TransformTypedefType(TypeLocBuilder &TLB, TypedefTypeLoc TL) {
    auto *Typedef = TL.getTypePtr()->getAs<TypedefType>();
    QualType Underlying = Typedef->desugar();

    if (const TypeVariableType *TypeVar = dyn_cast<TypeVariableType>(Underlying))
      if (BoundVars.find(TypeVar) == BoundVars.end())
        FreeTypedefDecls.insert(Typedef->getDecl());

    // TODO: doing two recursive calls is potentially slow. Figure out a way to do this
    // with just one call.
    TransformType(Underlying);
    return BaseTransform::TransformTypedefType(TLB, TL);
  }

  /// For a `RecordDecl` corresponding to a type application, recursively explore the type arguments.
  /// TODO: replace this case once we have a dedicated type for type applications.
  Decl *TransformDecl(SourceLocation Loc, Decl *D) {
    RecordDecl *RDecl;
    if ((RDecl = dyn_cast<RecordDecl>(D)) && RDecl->isInstantiated()) {
      for (auto TArg : RDecl->typeArgs()) getDerived().TransformType(TArg.typeName);
    }
    return BaseTransform::TransformDecl(Loc, D);
  }
};

class AlphaRenamer : public TreeTransform<AlphaRenamer> {

public:
  using SubstMap = llvm::DenseMap<const TypeVariableType *, QualType>;

private:
  typedef TreeTransform<AlphaRenamer> BaseTransform;

  ASTContext &Context;
  /// Maps olds bound variables to new variables so we can renumber them.
  SubstMap Substs;
  /// The depth to use for renumbering if we encounter a new bound variable.
  int NewDepth = 0;

public:
  AlphaRenamer(Sema &SemaRef) : BaseTransform(SemaRef), Context(SemaRef.Context) {}

  QualType Rename(QualType Tpe, int NewDepth, SubstMap Substs) {
    this->NewDepth = NewDepth;
    this->Substs = Substs;
    return getDerived().TransformType(Tpe);
  }

  QualType TransformTypeVariableType(TypeLocBuilder &TLB, TypeVariableTypeLoc TL) {
    auto *TypeVar = TL.getTypePtr();
    auto Iter = Substs.find(TypeVar);
    if (Iter == Substs.end()) return BaseTransform::TransformTypeVariableType(TLB, TL);
    // We need to renumber the variable.
    auto NewT = TLB.push<TypeVariableTypeLoc>(Iter->second);
    NewT.setNameLoc(TL.getNameLoc());
    return NewT.getType();
  }

  QualType TransformTypedefType(TypeLocBuilder &TLB, TypedefTypeLoc TL) {
    auto UnderlyingType = QualType(Context.getCanonicalType(TL.getTypePtr()), 0 /* Quals */);
    // Something changed, so we need to delete the typedef type from the AST and
    // and use the underlying transformed type.
    // Synthesize some dummy type source information.
    TypeSourceInfo *DI = getSema().Context.getTrivialTypeSourceInfo(UnderlyingType, getDerived().getBaseLocation());
    // Use that to get dummy location information.
    TypeLoc NewTL = DI->getTypeLoc();
    TLB.reserve(NewTL.getFullDataSize());
    // Re-run the type transformation with the dummy location information so
    // that the type location class pushed on to the TypeBuilder is the matching
    // class for the underlying type.
    return getDerived().TransformType(TLB, NewTL);
  }

  Decl *TransformDecl(SourceLocation Loc, Decl *D) {
    RecordDecl *RDecl;
    if ((RDecl = dyn_cast<RecordDecl>(D)) && RDecl->isInstantiated()) {
      llvm::SmallVector<TypeArgument, 4> NewArgs;
      for (auto TArg : RDecl->typeArgs()) {
        auto NewType = getDerived().TransformType(TArg.typeName);
        auto *SourceInfo = getSema().Context.getTrivialTypeSourceInfo(NewType, getDerived().getBaseLocation());
        NewArgs.push_back(TypeArgument { NewType, SourceInfo });
      }
      auto *Res = SemaRef.ActOnRecordTypeApplication(RDecl->genericBaseDecl(), ArrayRef<TypeArgument>(NewArgs));
      return Res;
    } else {
      return BaseTransform::TransformDecl(Loc, D);
    }
  }

  QualType TransformExistentialType(TypeLocBuilder &TLB, ExistentialTypeLoc TL) {
    auto *ExistType = TL.getTypePtr();
    // We call '.getCanonical()' because the type in the variable position
    // in an existential could be either a `TypedefType` or a `TypeVariableType`.
    // In the former case, we want to make sure we get the raw type variable.
    auto *TypeVar = Context.getCanonicalType(ExistType->typeVar())->getAs<TypeVariableType>();
    if (!TypeVar) llvm_unreachable("expected a TypeVariableType");
    // We need to renumber `TypeVar`, so we add the mapping `TypeVar -> NewDepth` to the substitutions.
    // The index is 0 because existentials only bound one type variable.
    // IsBoundsInterfaceType is false because an existential isn't a bounds interface.
    // We use the `[]` operator instead of `insert`, because the key might already be in the map,
    // in which case we want to replace the associated value.
    // Example:
    //   Suppose we're canonicalizing `_Exists(1, _Exists(2, struct Foo<0>))`, where 0 is free
    //   First, we canonicalize the inner type: _Exists(2, struct Foo<0>) => _Exists(1, struct Foo<0>)
    //   Then we handle the outer existential by calling TransformExistentialType(_Exists(1, struct Foo<0>)) where Substs = map(1 => 1)
    //   At this point, we encounter a new bound variable (1), and need to send 1 => 2. But 1 is already a key in
    //   the substitution map, so we need to replace the binding 1 => 1 by 1 => 2.
    Substs[TypeVar] = Context.getTypeVariableType(NewDepth, 0 /* Index */, false /* IsBoundsInterfaceType */);
    // Increment NewDepth so that there are no future collisions.
    NewDepth += 1;
    // Now the default recursion on both components so that the substitution can happen.
    return BaseTransform::TransformExistentialType(TLB, TL);
  }
};
}

const ExistentialType *Sema::ActOnExistentialType(ASTContext &Context, const Type *TypeVar, QualType InnerType) {
  auto *Cached = Context.getCachedExistentialType(TypeVar, InnerType);
  if (Cached) return Cached;
  // TODO: explain why we compute the canonical type here
  auto CanonInnerType = InnerType.getCanonicalType();
  // The type we need to generate isn't already cached, so we need to cache it.
  // Before caching it we need to generate an underlying canonical type.
  //
  // Generating the canonical type is a multi-step process:
  //   1) First we generate the set of free variables in `InnerType`.
  //   2) Then we compute `NewDepth` = max free variable + 1.
  //   3) We shift every bound variable in `InnerType` so they're numbered starting at `NewDepth`.
  // The result of step 3) yields a canonical type.
  //
  // Example:
  //   _Exists(X, _Exists(Y, struct Foo<X, Y, Z>))
  //   Suppose the underlying type really is
  //   _Exists(3, _Exists(4, struct Foo<3, 4, 0>))
  //   Free variables = {0}, so NewDepth = 1
  //   Shift all bound variables so they start at `NewDepth`:
  //   _Exists(1, Exists(2, struct Foo<1, 2, 0>)) // this is the canonical type
  //
  // Once we have the canonical type, we can insert the new existential in the cache while
  // making it point to the canonical type. Also notice that in computing the canonical type
  // we moved from using the user-visible names for type variables (e.g. 'X', 'Y', and 'Z') to
  // the low-level representation with depth levels.
  FreeVariablesFinder FreeVarsFinder(*this);
  auto FreeVars = FreeVarsFinder.find(CanonInnerType);
  // `TypeVar` could be a TypedefType, so we need to get its underlying type.
  auto *TypeVarRaw = Context.getCanonicalType(TypeVar)->getAs<TypeVariableType>();
  if (!TypeVarRaw) llvm_unreachable("Expected a TypeVariableType as the canonical type");
  // Now compute the new depth to which the bound variables should be moved in the canonical type.
  // This is just the max depth of any free variables, plus 1.
  // Example 1: if there are no free variables, then we start at 0.
  // Example 2: if the free variables are [0, 3, 4], we start at 5.
  unsigned int NewDepth = 0;
  for (auto *FreeVar : FreeVars) {
    // We want the largest free variable, but we want to ignore `TypeVarRaw` because
    // it's not really free. Example:
    //   TypeVarRaw = 0, InnerType = struct Foo<0>
    //   FreeVars(struct Foo<0>) = [0], but we want to ignore it, because
    //   in the resulting type _Exists(0, struct Foo<0>), 0 isn't free.
    if (FreeVar != TypeVarRaw && FreeVar->GetDepth() >= NewDepth) NewDepth = FreeVar->GetDepth() + 1;
  }
  // We can now alpha-rename `InnerType` so that bound variables start at `NewDepth`.
  // Example: E(3, E(4, struct Foo<3, 4, 1>)) -> E(2, E(3, struct Foo<2, 3, 1>))
  AlphaRenamer::SubstMap Substs;
  // We first insert the mapping OuterMostTypeVar -> NewDepth, because the outermost existential
  // isn't built yet (we're building it right now).
  // Example:
  // To canonicalze E(1, struct Foo<1>), we're given the two components separately (1, struct Foo<1>).
  // We need to alpha-rename 'struct Foo<1>', but we're missing the outermost binder and its correponding mapping
  // 1 -> 0. We need to inject this mapping into the substitution map before we alpha-rename the body.
  auto NewTypeVar = Context.getTypeVariableType(NewDepth, 0 /* position */, false /* isBoundsInterfaceType */);
  Substs.insert(std::make_pair(TypeVarRaw, NewTypeVar));
  AlphaRenamer Renamer(*this);
  auto NewInnerType = Renamer.Rename(CanonInnerType, NewDepth + 1 /* NewDepth is already used */, Substs);
  // We can finally get the canonical type with components (NewTypeVar, NewInnerType).
  auto *CanonType = Context.getCachedExistentialType(NewTypeVar.getTypePtr(), NewInnerType);
  if (!CanonType) {
    CanonType = new (Context, TypeAlignment) ExistentialType(NewTypeVar.getTypePtr(), NewInnerType, QualType());
    Context.addCachedExistentialType(NewTypeVar.getTypePtr(), NewInnerType, CanonType);
  }
  // An then we create the existential the user requested.
  // We have to query the cache again, because if the original and canonical types are equal
  // then the type we're looking for might have been added while creating the canonical type.
  auto *ExistType = Context.getCachedExistentialType(TypeVar, InnerType);
  if (!ExistType) {
    ExistType = new (Context, TypeAlignment) ExistentialType(TypeVar, InnerType, QualType(CanonType, 0 /* Quals */));
    Context.addCachedExistentialType(TypeVar, InnerType, ExistType);
  }
  return ExistType;
}

std::vector<const TypedefNameDecl *> Sema::FindFreeVariableDecls(QualType T) {
  FreeVariablesFinder finder(*this);
  return finder.findTypedefDecls(T);
}
