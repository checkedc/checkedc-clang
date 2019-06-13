//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This implements a simple constraint solver for expressions of the form:
//  a = b
//  not a
//  a implies b
//
// The Checked C converter tool performs type inference to identify locations 
// where a C type might be replaced with a Checked C type. This interface 
// does the solving to figure out where those substitions might happen.
//
//===----------------------------------------------------------------------===//
#ifndef _CONSTRAINTS_H
#define _CONSTRAINTS_H
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include <set>
#include <map>

class Constraint;
class Constraints;

template<typename T>
struct PComp
{
  bool operator()(const T lhs, const T rhs) const {
    return *lhs < *rhs;
  }
};

// Represents atomic values that can occur at positions in constraints.
class Atom {
public:
  enum AtomKind {
    A_Var,
    A_Ptr,
    A_Arr,
    A_NTArr,
    A_Wild,
    A_Const
  };
private:
  const AtomKind Kind;
public:
  Atom(AtomKind K) : Kind(K) {}
  virtual ~Atom() {}

  AtomKind getKind() const { return Kind; }

  virtual void print(llvm::raw_ostream &) const = 0;
  virtual void dump(void) const = 0;
  virtual bool operator==(const Atom &) const = 0;
  virtual bool operator!=(const Atom &) const = 0;
  virtual bool operator<(const Atom &other) const = 0;
};

// This refers to a location that we are trying to solve for.
class VarAtom : public Atom {
  friend class Constraints;
public:
  VarAtom(uint32_t D) : Atom(A_Var), Loc(D) {}

  static bool classof(const Atom *S) {
    return S->getKind() == A_Var;
  }

  void print(llvm::raw_ostream &O) const {
    O << "q_" << Loc;
  }

  void dump(void) const {
    print(llvm::errs());
  }

  bool operator==(const Atom &other) const {
    if (const VarAtom *V = llvm::dyn_cast<VarAtom>(&other)) 
      return V->Loc == Loc;
    else 
      return false;
  }

  bool operator!=(const Atom &other) const {
    return !(*this == other);
  }

  bool operator<(const Atom &other) const {
    if (const VarAtom *V = llvm::dyn_cast<VarAtom>(&other))
      return Loc < V->Loc;
    else
      return false;
  }

  uint32_t getLoc() const {
    return Loc;
  }

private:
  uint32_t  Loc;
  // The constraint expressions where this variable is mentioned on the 
  // LHS of an equality.
  std::set<Constraint*, PComp<Constraint*>> Constraints;
};

class ConstAtom : public Atom {
public:
  ConstAtom() : Atom(A_Const) {}
  ConstAtom(AtomKind K) : Atom(K) {}

  static bool classof(const Atom *A) {
    // Something is a class of ConstAtom if it ISN'T a Var.
    return A->getKind() != A_Var;
  }
};

// This refers to the constant PTR.
class PtrAtom : public ConstAtom {
public:
  PtrAtom() : ConstAtom(A_Ptr) {}

  static bool classof(const Atom *S) {
    return S->getKind() == A_Ptr;
  }

  void print(llvm::raw_ostream &O) const {
    O << "PTR";
  }

  void dump(void) const {
    print(llvm::errs());
  }

  bool operator==(const Atom &other) const {
    return llvm::isa <PtrAtom>(&other);
  }

  bool operator!=(const Atom &other) const {
    return !(*this == other);
  }

  bool operator<(const Atom &other) const {
    return *this != other;
  }
};

class NTArrAtom;

// This refers to the constant ARR.
class ArrAtom : public ConstAtom {
public:
  ArrAtom() : ConstAtom(A_Arr) {}

  static bool classof(const Atom *S) {
    return S->getKind() == A_Arr;
  }

  void print(llvm::raw_ostream &O) const {
    O << "ARR";
  }

  void dump(void) const {
    print(llvm::errs());
  }

  bool operator==(const Atom &other) const {
    return llvm::isa<ArrAtom>(&other);
  }

  bool operator!=(const Atom &other) const {
    return !(*this == other);
  }

  bool operator<(const Atom &other) const {
    if (llvm::isa<PtrAtom>(&other) || llvm::isa<NTArrAtom>(&other) || *this == other)
      return false;
    else
      return true;
  }
};

// This refers to the constant NTARR.
class NTArrAtom : public ConstAtom {
public:
  NTArrAtom() : ConstAtom(A_NTArr) {}

  static bool classof(const Atom *S) {
    return S->getKind() == A_NTArr;
  }

  void print(llvm::raw_ostream &O) const {
    O << "NTARR";
  }

  void dump(void) const {
    print(llvm::errs());
  }

  bool operator==(const Atom &other) const {
    return llvm::isa<NTArrAtom>(&other);
  }

  bool operator!=(const Atom &other) const {
    return !(*this == other);
  }

  bool operator<(const Atom &other) const {
    if (llvm::isa<PtrAtom>(&other) || llvm::isa<NTArrAtom>(&other) || *this == other)
      return false;
    else
      return true;
  }
};

// This refers to the constant WILD.
class WildAtom : public ConstAtom {
public:
  WildAtom() : ConstAtom(A_Wild) {}

  static bool classof(const Atom *S) {
    return S->getKind() == A_Wild;
  }

  void print(llvm::raw_ostream &O) const {
    O << "WILD";
  }

  void dump(void) const {
    print(llvm::errs());
  }

  bool operator==(const Atom &other) const {
    if (llvm::isa<WildAtom>(&other)) 
      return true;
    else 
      return false;
  }

  bool operator!=(const Atom &other) const {
    return !(*this == other);
  }

  bool operator<(const Atom &other) const {
    if (llvm::isa<ArrAtom>(&other) || llvm::isa<PtrAtom>(&other) || *this == other)
      return false;
    else
      return true;
  }
};

// Represents constraints of the form:
//  - a = b
//  - not a
//  - a => b
class Constraint {
public:
  enum ConstraintKind {
    C_Eq,
    C_Not,
    C_Imp
  };
private:
  const ConstraintKind Kind;
public:
  Constraint(ConstraintKind K) : Kind(K) {}
  virtual ~Constraint() {}

  ConstraintKind getKind() const { return Kind; }

  virtual void print(llvm::raw_ostream &) const = 0;
  virtual void dump(void) const = 0;
  virtual bool operator==(const Constraint &other) const = 0;
  virtual bool operator!=(const Constraint &other) const = 0;
  virtual bool operator<(const Constraint &other) const = 0;
};

// a = b
class Eq : public Constraint {
public:

  Eq(Atom *lhs, Atom *rhs)
    : Constraint(C_Eq), lhs(lhs), rhs(rhs) {}

  static bool classof(const Constraint *C) {
    return C->getKind() == C_Eq;
  }

  void print(llvm::raw_ostream &O) const {
    lhs->print(O);
    O << " == ";
    rhs->print(O);
  }

  void dump(void) const {
    print(llvm::errs());
  }

  Atom *getLHS(void) const { return lhs; }
  Atom *getRHS(void) const { return rhs; }

  bool operator==(const Constraint &other) const {
    if (const Eq *E = llvm::dyn_cast<Eq>(&other))
      return *lhs == *E->lhs && *rhs == *E->rhs;
    else
      return false;
  }

  bool operator!=(const Constraint &other) const {
    return !(*this == other);
  }

  bool operator<(const Constraint &other) const {
    ConstraintKind K = other.getKind();
    if (K == C_Eq) {
      const Eq *E = llvm::dyn_cast<Eq>(&other);
      assert(E != NULL);

      if (*lhs == *E->lhs && *rhs == *E->rhs)
        return false;
      else if (*lhs == *E->lhs && *rhs != *E->rhs)
        return *rhs < *E->rhs;
      else
        return *lhs < *E->lhs;
    }
    else 
      return C_Eq < K;
  }

private:
  Atom *lhs;
  Atom *rhs;
};

// not a
class Not : public Constraint {
public:
  Not(Constraint *b)
    : Constraint(C_Not), body(b) {}

  static bool classof(const Constraint *C) {
    return C->getKind() == C_Not;
  }

  void print(llvm::raw_ostream &O) const {
    O << "~(";
    body->print(O);
    O << ")";
  }

  void dump(void) const {
    print(llvm::errs());
  }

  bool operator==(const Constraint &other) const {
    if (const Not *N = llvm::dyn_cast<Not>(&other))
      return *body == *N->body;
    else
      return false;
  }

  bool operator!=(const Constraint &other) const {
    return !(*this == other);
  }

  bool operator<(const Constraint &other) const {
    ConstraintKind K = other.getKind();
    if (K == C_Not) {
      const Not *N = llvm::dyn_cast<Not>(&other);
      assert(N != NULL);

      return *body < *N->body;
    }
    else 
      return C_Not < K;
  }

  Constraint *getBody() const {
    return body;
  }

private:
  Constraint *body;
};

// a => b
class Implies : public Constraint {
public:

  Implies(Constraint *premise, Constraint *conclusion)
    : Constraint(C_Imp), premise(premise), conclusion(conclusion) {}

  static bool classof(const Constraint *C) {
    return C->getKind() == C_Imp;
  }

  Constraint *getPremise() const { return premise; }
  Constraint *getConclusion() const { return conclusion; }

  void print(llvm::raw_ostream &O) const {
    premise->print(O);
    O << " => ";
    conclusion->print(O);
  }

  void dump(void) const {
    print(llvm::errs());
  }

  bool operator==(const Constraint &other) const {
    if (const Implies *I = llvm::dyn_cast<Implies>(&other)) 
      return *premise == *I->premise && *conclusion == *I->conclusion;
    else 
      return false;
  }

  bool operator!=(const Constraint &other) const {
    return !(*this == other);
  }
 
  bool operator<(const Constraint &other) const {
    ConstraintKind K = other.getKind();
    if (K == C_Imp) {
      const Implies *I = llvm::dyn_cast<Implies>(&other);
      assert(I != NULL);

      if (*premise == *I->premise && *conclusion == *I->conclusion)
        return false;
      else if (*premise == *I->premise && *conclusion != *I->conclusion)
        return *conclusion < *I->conclusion;
      else
        return *premise < *I->premise;
    }
    else
      return C_Imp < K;
  }

private:
  Constraint *premise;
  Constraint *conclusion;
};

class Constraints {
public:
  Constraints();
  ~Constraints();
  // Remove the copy constructor, so we never accidentally copy this thing.
  Constraints(const Constraints& O) = delete;

  typedef std::set<Constraint*, PComp<Constraint*> > ConstraintSet;
  // The environment maps from Vars to Consts (one of Ptr, Arr, Wild).
  typedef std::map<VarAtom*, ConstAtom*, PComp<VarAtom*> > EnvironmentMap;

  bool addConstraint(Constraint *c);
  // It's important to return these by reference. Programs can have 
  // 10-100-100000 constraints and variables, and copying them each time
  // a client wants to examine the environment is untenable.
  ConstraintSet &getConstraints() { return constraints; }
  EnvironmentMap &getVariables() { return environment; }
  // Solve the system of constraints. Return true in the second position if
  // the system is solved. If the system is solved, the first position is 
  // an empty. If the system could not be solved, the constraints in conflict
  // are returned in the first position.
  // TODO: this functionality is not implemented yet.
  std::pair<ConstraintSet, bool> solve(void);
  void dump() const;
  void print(llvm::raw_ostream &) const;

  Eq *createEq(Atom *lhs, Atom *rhs);
  Not *createNot(Constraint *body);
  Implies *createImplies(Constraint *premise, Constraint *conclusion);

  VarAtom *getOrCreateVar(uint32_t v);
  VarAtom *getVar(uint32_t v) const;
  PtrAtom *getPtr() const;
  ArrAtom *getArr() const;
  NTArrAtom *getNTArr() const;
  WildAtom *getWild() const;

private:
  ConstraintSet constraints;
  EnvironmentMap environment;

  bool step_solve(EnvironmentMap &);
  bool check(Constraint *C);

  template <typename T>
  bool
  propEq(EnvironmentMap &env, Eq *Dyn, T *A, ConstraintSet &R,
      EnvironmentMap::iterator &CurValLHS);

  template <typename T>
  bool
  propImp(Implies *, T*, ConstraintSet &, ConstAtom *);

  // These atoms can be singletons, so we'll store them in the 
  // Constraints class.
  PtrAtom *prebuiltPtr;
  ArrAtom *prebuiltArr;
  NTArrAtom *prebuiltNTArr;
  WildAtom *prebuiltWild;
};

#endif
