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

static const Type *getNextTy(const Type *Ty) {
  if (const PointerType *PT = dyn_cast<PointerType>(Ty))
    return PT->getPointeeType().getTypePtr();
  else
    return NULL;
}

// Given a solved set of constraints CS and a declaration D, produce a
// NewTyp data structure that describes how the type declaration for D
// might be re-written. The NewTyp data structure is needed because the
// replacement of the type declaration in the original source code needs
// to be done all at once via the Rewriter.
NewTyp *NewTyp::mkTypForConstrainedType(Decl *D, DeclStmt *K,
                                        ProgramInfo &PI, ASTContext *C) {
  const Type *Ty = NULL;
  if (VarDecl *VD = dyn_cast<VarDecl>(D))
    Ty = VD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
  else if (FieldDecl *FD = dyn_cast<FieldDecl>(D))
    Ty = FD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
  else if (FunctionDecl *UD = dyn_cast<FunctionDecl>(D))
    Ty = UD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
  else
    llvm_unreachable("unknown decl type");

  assert(Ty != NULL);
  Ty = Ty->getUnqualifiedDesugaredType();

  // Strip off function definitions from the type.
  while (Ty != NULL) {
    if (const FunctionType *FT = dyn_cast<FunctionType>(Ty))
      Ty = FT->getReturnType().getTypePtr();
    else if (const FunctionNoProtoType *FNPT = dyn_cast<FunctionNoProtoType>(Ty))
      Ty = FNPT->getReturnType().getTypePtr();
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

  while (Ty != NULL) {
    QualType QT(Ty, 0);
    NewTyp *tmp = NULL;
    if (Ty->isPointerType()) {
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
      tmp = new BaseNonPointerTyp(QT);
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
      Ty = getNextTy(Ty);
  }

  assert(T != NULL);
  assert(T->ReferentTyp != NULL || T->getKind() == N_BaseNonPointer);
  return T;
}

std::string BaseNonPointerTyp::mkStr() {
  return T.getUnqualifiedType().getAsString();
}
