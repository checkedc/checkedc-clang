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
NewTyp *NewTyp::mkTypForConstrainedType(Constraints &CS, Decl *D, DeclStmt *K,
                                        VariableMap &VM) {
  // By cases, D could be a param, field, function, or variable decl. Get the 
  // type location for the declaration D, that will allow us to iterate over the
  // different levels of the type.
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

  // Get the "base" variable for the declaration.
  auto baseQVKeyI = VM.find(D);
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
      TL = TL.getNextTypeLoc();
    else
      break;
  }

  while (!TL.isNull()) {
    // What should the current type be qualified as? This can be answered by
    // looking the constraint up for the current variable.

    NewTyp *tmp;
    if (TL.getType()->isPointerType()) {
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
      }

      curKey++;
    } else
      tmp = new BaseNonPointerTyp(TL.getType());

    // If this is our first trip through the loop, update the Cur variable
    // to point to the NewTyp we created. Otherwise, update the ReferentTyp
    // field of Cur. Also, if this is our first trip through the loop,
    // update T to be the value we produced.
    if (Cur == NULL)
      Cur = tmp;
    else
      Cur->ReferentTyp = tmp;

    if (T == NULL) {
      T = Cur;
      T->DeclRewrite = D;
      T->StmtWhere = K;
    }

    Cur = tmp;

    TL = TL.getNextTypeLoc();
  }

  assert(T != NULL);
  return T;
}
