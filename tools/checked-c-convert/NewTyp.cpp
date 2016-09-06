//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#include "NewTyp.h"

using namespace clang;
using namespace llvm;

// Given a solved set of constraints CS and a declaration D, produce a
// NewTyp data structure that describes how the type declaration for D
// might be re-written. The NewTyp data structure is needed because the
// replacement of the type declaration in the original source code needs
// to be done all at once via the Rewriter.
NewTyp *NewTyp::mkTypForConstrainedType(Decl *D, DeclStmt *K,
                                        ProgramInfo &PI, ASTContext *C) {
  TypeLoc TL;

  if (VarDecl *VD = dyn_cast<VarDecl>(D))
    TL = VD->getTypeSourceInfo()->getTypeLoc();
  else if (FieldDecl *FD = dyn_cast<FieldDecl>(D))
    TL = FD->getTypeSourceInfo()->getTypeLoc();
  else if (FunctionDecl *UD = dyn_cast<FunctionDecl>(D))
    TL = UD->getTypeSourceInfo()->getTypeLoc();
  else
    llvm_unreachable("unknown decl type");

  assert(!TL.isNull());
  
  // Strip off function definitions from the type.
  while (!TL.isNull()) {
    QualType T = TL.getType();
    if (T->isFunctionNoProtoType() || T->isFunctionType() ||
      T->isFunctionProtoType())
      TL = TL.getNextTypeLoc();
    else
      break;
  }

  uint32_t baseQVKey;
  std::set<uint32_t> baseQVKeyS;
  PI.getVariable(D, baseQVKeyS, C);
  assert(baseQVKeyS.size() == 1);
  baseQVKey = *baseQVKeyS.begin();

  // Now, build up a NewTyp type.
  NewTyp *T = NULL;
  NewTyp *Cur = T;
  uint32_t curKey = baseQVKey;
  Constraints::EnvironmentMap env = PI.getConstraints().getVariables();

  // What if we don't have anything to do after stripping off function
  // definitions?
  assert(!TL.isNull());

  while (!TL.isNull()) {
    // What should the current type be qualified as? This can be answered by
    // looking the constraint up for the current variable.
    NewTyp *tmp = NULL;
    if (TypedefTypeLoc TTL = TL.getAs<TypedefTypeLoc>()) {
      // Skip everything for now.
      tmp = new BaseNonPointerTyp(TTL.getType());
    } else if (TL.getType()->isPointerType()) {
      VarAtom toFind(curKey);
      auto I = env.find(&toFind);
      assert(I != env.end());
      ConstAtom *C = I->second;

      // How is the current type index qualified? This controls which base
      // class we fill in.

      switch (C->getKind()) {
      case Atom::A_Wild:
        tmp = new WildTyp();
        break;
      case Atom::A_Ptr:
        tmp = new PtrTyp();
        break;
      case Atom::A_Arr:
        tmp = new ArrTyp();
        break;
      case Atom::A_Var:
      case Atom::A_Const:
        llvm_unreachable("bad value in environment map");
        break;
      default:
        llvm_unreachable("impossible");
      }

      curKey++;
    } else {
      tmp = new BaseNonPointerTyp(TL.getType());
    }

    // If this is our first trip through the loop, update the Cur variable
    // to point to the NewTyp we created. Otherwise, update the ReferentTyp
    // field of Cur. Also, if this is our first trip through the loop,
    // update T to be the value we produced.
    assert(tmp != NULL);
    if (Cur == NULL) {
      Cur = tmp;
    }
    else {
      Cur->ReferentTyp = tmp;
    }

    if (T == NULL) {
      T = Cur;
      T->DeclRewrite = D;
      T->StmtWhere = K;
    }

    Cur = tmp;

    if (isa<BaseNonPointerTyp>(Cur))
      break;
    else
      TL = TL.getNextTypeLoc();
  }

  assert(T != NULL);
  assert(T->ReferentTyp != NULL || T->getKind() == N_BaseNonPointer);
  return T;

  return NULL;
}
#if 0
  // By cases, D could be a param, field, function, or variable decl. Get the 
  // type location for the declaration D, that will allow us to iterate over the
  // different levels of the type.
  D->dump();
  TypeLoc TL;
  if (VarDecl *VD = dyn_cast<VarDecl>(D))
    TL = VD->getTypeSourceInfo()->getTypeLoc();
  else if (ParmVarDecl *PD = dyn_cast<ParmVarDecl>(D))
    TL = VD->getTypeSourceInfo()->getTypeLoc();
  else if (FieldDecl *FD = dyn_cast<FieldDecl>(D))
    TL = FD->getTypeSourceInfo()->getTypeLoc();
  else if (FunctionDecl *UD = dyn_cast<FunctionDecl>(D))
    TL = UD->getTypeSourceInfo()->getTypeLoc();
  else
    llvm_unreachable("unknown decl type");
  assert(!TL.isNull());
  errs() << "TL on entry\n";
  TL.getTypePtr()->dump();
  errs() << "\n";

  // Get the "base" variable for the declaration.
  VariableMap::iterator baseQVKeyI = VM.find(D);
  assert(baseQVKeyI != VM.end());
  uint32_t baseQVKey = baseQVKeyI->second;

  // Now, build up a NewTyp type.
  NewTyp *T = NULL;
  NewTyp *Cur = T;
  uint32_t curKey = baseQVKey;
  Constraints::EnvironmentMap env = CS.getVariables();

  // Strip off function definitions from the type.
  while (!TL.isNull()) {
    QualType T = TL.getType();
    if (T->isFunctionNoProtoType() || T->isFunctionType() ||
      T->isFunctionProtoType())
      TL = advanceTypeLoc(TL);
    else
      break;
  }

  errs() << "TL after stripping off functions\n";
  TL.getTypePtr()->dump();
  errs() << "\n";

  // What if we don't have anything to do after stripping off function
  // definitions?
  assert(!TL.isNull());

  while (!TL.isNull()) {
    // What should the current type be qualified as? This can be answered by
    // looking the constraint up for the current variable.
    errs() << "!TL.isNull loop\n";
    TL.getTypePtr()->dump();
    errs() << "\n";
    NewTyp *tmp = NULL;
    if (TL.getType()->isPointerType()) {
      VarAtom toFind(curKey);
      auto I = env.find(&toFind);
      assert(I != env.end());
      ConstAtom *C = I->second;

      // How is the current type index qualified? This controls which base
      // class we fill in.

      switch (C->getKind()) {
      case Atom::A_Wild:
        errs() << "A_Wild\n";
        tmp = new WildTyp();
        break;
      case Atom::A_Ptr:
        errs() << "A_Ptr\n";
        tmp = new PtrTyp();
        break;
      case Atom::A_Arr:
        errs() << "A_Arr\n";
        tmp = new ArrTyp();
        break;
      case Atom::A_Var:
      case Atom::A_Const:
        llvm_unreachable("bad value in environment map");
        break;
      default:
        llvm_unreachable("impossible");
      }

      curKey++;
    } else {
      errs() << "Base ptr!\n";
      tmp = new BaseNonPointerTyp(TL.getType());
    }

    // If this is our first trip through the loop, update the Cur variable
    // to point to the NewTyp we created. Otherwise, update the ReferentTyp
    // field of Cur. Also, if this is our first trip through the loop,
    // update T to be the value we produced.
    assert(tmp != NULL);
    if (Cur == NULL) {
      errs() << "Cur == NULL\n";
      Cur = tmp;
    } else {
      errs() << "Cur != NULL\n";
      Cur->ReferentTyp = tmp;
    }

    if (T == NULL) {
      T = Cur;
      T->DeclRewrite = D;
      T->StmtWhere = K;
    }

    Cur = tmp;

    TL = advanceTypeLoc(TL);
  }

  assert(T != NULL);
  assert(T->ReferentTyp != NULL || T->getKind() == N_BaseNonPointer);
  return T;
}
#endif
