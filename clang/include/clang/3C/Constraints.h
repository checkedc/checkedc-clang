//=--Constraints.h------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This implements a simple constraint solver for expressions of the form:
//  a >= b
//  a implies b
//
// The 3C tool performs type inference to identify locations
// where a C type might be replaced with a Checked C type. This interface
// does the solving to figure out where those substitions might happen.
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_3C_CONSTRAINTS_H
#define LLVM_CLANG_3C_CONSTRAINTS_H

#include "Utils.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <set>

class Constraint;
class ConstraintVariable;
class Constraints;
class PersistentSourceLoc;
class ConstraintsGraph;

#define DEFAULT_REASON "UNKNOWN_REASON"
#define POINTER_IS_ARRAY_REASON "Pointer is array but alltypes is disabled."

template <typename T> struct PComp {
  bool operator()(const T Lhs, const T Rhs) const { return *Lhs < *Rhs; }
};

class VarAtom;

// Represents atomic values that can occur at positions in constraints.
class Atom {
public:
  enum AtomKind { A_Var, A_Ptr, A_Arr, A_NTArr, A_Wild, A_Const };

private:
  const AtomKind Kind;

public:
  Atom(AtomKind K) : Kind(K) {}
  virtual ~Atom() {}

  AtomKind getKind() const { return Kind; }

  virtual void print(llvm::raw_ostream &) const = 0;
  virtual void dump(void) const = 0;
  virtual void dumpJson(llvm::raw_ostream &) const = 0;
  std::string getStr() {
    std::string Buf;
    llvm::raw_string_ostream TmpS(Buf);
    print(TmpS);
    return TmpS.str();
  }
  virtual bool operator==(const Atom &) const = 0;
  virtual bool operator!=(const Atom &) const = 0;
  virtual bool operator<(const Atom &) const = 0;
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

// This refers to a location that we are trying to solve for.
class VarAtom : public Atom {
  friend class Constraints;

public:
  enum VarKind { V_Param, V_Return, V_Other };

  VarAtom(uint32_t D) : Atom(A_Var), Loc(D), Name("q"), KindV(V_Other) {}
  VarAtom(uint32_t D, std::string N)
      : Atom(A_Var), Loc(D), Name(N), KindV(V_Other) {}
  VarAtom(uint32_t D, std::string N, VarKind V)
      : Atom(A_Var), Loc(D), Name(N), KindV(V) {}

  static bool classof(const Atom *S) { return S->getKind() == A_Var; }

  static std::string varKindToStr(VarKind V) {
    switch (V) {
    case V_Param:
      return ">>";
    case V_Return:
      return "<<";
    case V_Other:
      return "";
    }
    return "";
  }

  void print(llvm::raw_ostream &O) const {
    O << varKindToStr(KindV) << Name << "_" << Loc;
  }

  void dump(void) const { print(llvm::errs()); }

  void dumpJson(llvm::raw_ostream &O) const {
    O << "\"" << varKindToStr(KindV) << Name << "_" << Loc << "\"";
  }

  bool operator==(const Atom &Other) const {
    if (const VarAtom *V = llvm::dyn_cast<VarAtom>(&Other))
      return V->Loc == Loc;
    return false;
  }

  bool operator!=(const Atom &Other) const { return !(*this == Other); }

  bool operator<(const Atom &Other) const {
    if (const VarAtom *V = llvm::dyn_cast<VarAtom>(&Other))
      return Loc < V->Loc;
    return false;
  }

  uint32_t getLoc() const { return Loc; }
  std::string getName() const { return Name; }
  VarKind getVarKind() const { return KindV; }

  // Returns the constraints associated with this atom.
  std::set<Constraint *, PComp<Constraint *>> &getAllConstraints() {
    return Constraints;
  }

private:
  uint32_t Loc;
  std::string Name;
  const VarKind KindV;
  // The constraint expressions where this variable is mentioned on the
  // LHS of an equality.
  std::set<Constraint *, PComp<Constraint *>> Constraints;
};

/* ConstAtom ordering is:
    NTARR < ARR < PTR < WILD
   and all ConstAtoms < VarAtoms
*/

// This refers to the constant NTARR.
class NTArrAtom : public ConstAtom {
public:
  NTArrAtom() : ConstAtom(A_NTArr) {}

  static bool classof(const Atom *S) { return S->getKind() == A_NTArr; }

  void print(llvm::raw_ostream &O) const { O << "NTARR"; }

  void dump(void) const { print(llvm::errs()); }

  void dumpJson(llvm::raw_ostream &O) const { O << "\"NTARR\""; }

  bool operator==(const Atom &Other) const {
    return llvm::isa<NTArrAtom>(&Other);
  }

  bool operator!=(const Atom &Other) const { return !(*this == Other); }

  bool operator<(const Atom &Other) const { return !(*this == Other); }
};

// This refers to the constant ARR.
class ArrAtom : public ConstAtom {
public:
  ArrAtom() : ConstAtom(A_Arr) {}

  static bool classof(const Atom *S) { return S->getKind() == A_Arr; }

  void print(llvm::raw_ostream &O) const { O << "ARR"; }

  void dump(void) const { print(llvm::errs()); }

  void dumpJson(llvm::raw_ostream &O) const { O << "\"ARR\""; }

  bool operator==(const Atom &Other) const {
    return llvm::isa<ArrAtom>(&Other);
  }

  bool operator!=(const Atom &Other) const { return !(*this == Other); }

  bool operator<(const Atom &Other) const {
    return !(llvm::isa<NTArrAtom>(&Other) || *this == Other);
  }
};

// This refers to the constant PTR.
class PtrAtom : public ConstAtom {
public:
  PtrAtom() : ConstAtom(A_Ptr) {}

  static bool classof(const Atom *S) { return S->getKind() == A_Ptr; }

  void print(llvm::raw_ostream &O) const { O << "PTR"; }

  void dump(void) const { print(llvm::errs()); }

  void dumpJson(llvm::raw_ostream &O) const { O << "\"PTR\""; }

  bool operator==(const Atom &Other) const {
    return llvm::isa<PtrAtom>(&Other);
  }

  bool operator!=(const Atom &Other) const { return !(*this == Other); }

  bool operator<(const Atom &Other) const {
    return !(llvm::isa<ArrAtom>(&Other) || llvm::isa<NTArrAtom>(&Other) ||
             *this == Other);
  }
};

// This refers to the constant WILD.
class WildAtom : public ConstAtom {
public:
  WildAtom() : ConstAtom(A_Wild) {}

  static bool classof(const Atom *S) { return S->getKind() == A_Wild; }

  void print(llvm::raw_ostream &O) const { O << "WILD"; }

  void dump(void) const { print(llvm::errs()); }

  void dumpJson(llvm::raw_ostream &O) const { O << "\"WILD\""; }

  bool operator==(const Atom &Other) const {
    return llvm::isa<WildAtom>(&Other);
  }

  bool operator!=(const Atom &Other) const { return !(*this == Other); }

  bool operator<(const Atom &Other) const {
    return !(llvm::isa<ArrAtom>(&Other) || llvm::isa<NTArrAtom>(&Other) ||
             llvm::isa<PtrAtom>(&Other) || *this == Other);
  }
};

// Represents constraints of the form:
//  - a >= b
//  - a ==> b
class Constraint {
public:
  enum ConstraintKind { C_Geq, C_Imp };

private:
  const ConstraintKind Kind;
  PersistentSourceLoc PL;

public:
  std::string REASON = DEFAULT_REASON;

  Constraint(ConstraintKind K) : Kind(K) {}
  Constraint(ConstraintKind K, const std::string &Rsn) : Kind(K) {
    REASON = Rsn;
  }
  Constraint(ConstraintKind K, const std::string &Rsn, PersistentSourceLoc *PL);

  virtual ~Constraint() {}

  ConstraintKind getKind() const { return Kind; }

  virtual void print(llvm::raw_ostream &) const = 0;
  virtual void dump(void) const = 0;
  virtual void dumpJson(llvm::raw_ostream &) const = 0;
  virtual bool operator==(const Constraint &Other) const = 0;
  virtual bool operator!=(const Constraint &Other) const = 0;
  virtual bool operator<(const Constraint &Other) const = 0;
  virtual std::string getReason() { return REASON; }
  virtual void setReason(const std::string &Rsn) { REASON = Rsn; }

  const PersistentSourceLoc &getLocation() const { return PL; }
};

// a >= b
class Geq : public Constraint {
  friend class VarAtom;

public:
  Geq(Atom *Lhs, Atom *Rhs, bool IsCC = true)
      : Constraint(C_Geq), Lhs(Lhs), Rhs(Rhs), IsCheckedConstraint(IsCC) {}

  Geq(Atom *Lhs, Atom *Rhs, const std::string &Rsn, bool IsCC = true)
      : Constraint(C_Geq, Rsn), Lhs(Lhs), Rhs(Rhs), IsCheckedConstraint(IsCC) {}

  Geq(Atom *Lhs, Atom *Rhs, const std::string &Rsn, PersistentSourceLoc *PL,
      bool IsCC = true)
      : Constraint(C_Geq, Rsn, PL), Lhs(Lhs), Rhs(Rhs),
        IsCheckedConstraint(IsCC) {}

  static bool classof(const Constraint *C) { return C->getKind() == C_Geq; }

  void print(llvm::raw_ostream &O) const {
    Lhs->print(O);
    std::string Kind = IsCheckedConstraint ? " (C)>= " : " (P)>= ";
    O << Kind;
    Rhs->print(O);
    O << ", Reason:" << REASON;
  }

  void dump(void) const { print(llvm::errs()); }

  void dumpJson(llvm::raw_ostream &O) const {
    O << "{\"Geq\":{\"Atom1\":";
    Lhs->dumpJson(O);
    O << ", \"Atom2\":";
    Rhs->dumpJson(O);
    O << ", \"isChecked\":";
    O << (IsCheckedConstraint ? "true" : "false");
    O << ", \"Reason\":";
    llvm::json::Value ReasonVal(REASON);
    O << ReasonVal;
    O << "}}";
  }

  Atom *getLHS(void) const { return Lhs; }
  Atom *getRHS(void) const { return Rhs; }

  void setChecked(ConstAtom *C) {
    assert(!IsCheckedConstraint);
    IsCheckedConstraint = true;
    if (llvm::isa<ConstAtom>(Lhs)) {
      Lhs = Rhs; // reverse direction for checked constraint
      Rhs = C;
    } else {
      assert(llvm::isa<ConstAtom>(Rhs));
      Rhs = C;
    }
  }

  bool constraintIsChecked(void) const { return IsCheckedConstraint; }

  bool operator==(const Constraint &Other) const {
    if (const Geq *E = llvm::dyn_cast<Geq>(&Other))
      return *Lhs == *E->Lhs && *Rhs == *E->Rhs &&
             IsCheckedConstraint == E->IsCheckedConstraint;
    return false;
  }

  bool operator!=(const Constraint &Other) const { return !(*this == Other); }

  bool operator<(const Constraint &Other) const {
    ConstraintKind K = Other.getKind();
    if (K == C_Geq) {
      const Geq *E = llvm::dyn_cast<Geq>(&Other);
      assert(E != nullptr);
      if (*Lhs == *E->Lhs) {
        if (*Rhs == *E->Rhs) {
          if (IsCheckedConstraint == E->IsCheckedConstraint)
            return false;
          return IsCheckedConstraint < E->IsCheckedConstraint;
        }
        return *Rhs < *E->Rhs;
      }
      return *Lhs < *E->Lhs;
    }
    return C_Geq < K;
  }

private:
  Atom *Lhs;
  Atom *Rhs;
  bool IsCheckedConstraint;
};

// a ==> b
class Implies : public Constraint {
public:
  Implies(Geq *Premise, Geq *Conclusion)
      : Constraint(C_Imp), Premise(Premise), Conclusion(Conclusion) {}

  static bool classof(const Constraint *C) { return C->getKind() == C_Imp; }

  Geq *getPremise() { return Premise; }
  Geq *getConclusion() { return Conclusion; }

  void print(llvm::raw_ostream &O) const {
    Premise->print(O);
    O << " ==> ";
    Conclusion->print(O);
  }

  void dump(void) const { print(llvm::errs()); }

  void dumpJson(llvm::raw_ostream &O) const {
    O << "{\"Implies\":{\"Premise\":";
    Premise->dumpJson(O);
    O << ", \"Conclusion\":";
    Conclusion->dumpJson(O);
    O << "}}";
  }

  bool operator==(const Constraint &Other) const {
    if (const Implies *I = llvm::dyn_cast<Implies>(&Other))
      return *Premise == *I->Premise && *Conclusion == *I->Conclusion;
    return false;
  }

  bool operator!=(const Constraint &Other) const { return !(*this == Other); }

  bool operator<(const Constraint &Other) const {
    ConstraintKind K = Other.getKind();
    if (K == C_Imp) {
      const Implies *I = llvm::dyn_cast<Implies>(&Other);
      assert(I != nullptr);

      if (*Premise == *I->Premise && *Conclusion == *I->Conclusion)
        return false;
      if (*Premise == *I->Premise && *Conclusion != *I->Conclusion)
        return *Conclusion < *I->Conclusion;
      return *Premise < *I->Premise;
    }
    return C_Imp < K;
  }

private:
  Geq *Premise;
  Geq *Conclusion;
};

// This is the solution, the first item is Checked Solution and the second
// is Ptr solution.
typedef std::pair<ConstAtom *, ConstAtom *> VarSolTy;
typedef std::map<VarAtom *, VarSolTy, PComp<VarAtom *>> EnvironmentMap;

typedef uint32_t ConstraintKey;

using VarAtomPred = llvm::function_ref<bool(VarAtom *)>;

class ConstraintsEnv {

public:
  ConstraintsEnv() : ConsFreeKey(0), UseChecked(true) { Environment.clear(); }
  const EnvironmentMap &getVariables() const { return Environment; }
  void dump() const;
  void print(llvm::raw_ostream &) const;
  void dumpJson(llvm::raw_ostream &) const;
  /* constraint variable generation */
  VarAtom *getFreshVar(VarSolTy InitC, std::string Name, VarAtom::VarKind VK);
  VarAtom *getOrCreateVar(ConstraintKey V, VarSolTy InitC, std::string Name,
                          VarAtom::VarKind VK);
  VarAtom *getVar(ConstraintKey V) const;
  /* solving */
  ConstAtom *getAssignment(Atom *A);
  bool assign(VarAtom *V, ConstAtom *C);
  void doCheckedSolve(bool DoC) { UseChecked = DoC; }
  void mergePtrTypes();
  std::set<VarAtom *> resetSolution(VarAtomPred Pred, ConstAtom *CA);
  void resetFullSolution(VarSolTy InitC);
  bool checkAssignment(VarSolTy C);
  std::set<VarAtom *> filterAtoms(VarAtomPred Pred);

private:
  EnvironmentMap Environment; // Solution map: Var --> Sol
  uint32_t ConsFreeKey;       // Next available integer to assign to a Var
  bool UseChecked;            // Which solution map to use -- checked (vs. ptyp)
};

class ConstraintVariable;

class Constraints {
public:
  Constraints();
  ~Constraints();
  // Remove the copy constructor, so we never accidentally copy this thing.
  Constraints(const Constraints &O) = delete;

  typedef std::set<Constraint *, PComp<Constraint *>> ConstraintSet;

  // Map from a unique key of a function to its constraint variables.
  typedef std::map<std::string, std::set<ConstraintVariable *>>
      FuncKeyToConsMap;

  bool removeConstraint(Constraint *C);
  bool addConstraint(Constraint *C);
  // It's important to return these by reference. Programs can have
  // 10-100-100000 constraints and variables, and copying them each time
  // a client wants to examine the environment is untenable.
  const ConstraintSet &getConstraints() const { return TheConstraints; }
  const EnvironmentMap &getVariables() const {
    return Environment.getVariables();
  }

  void editConstraintHook(Constraint *C);

  void solve();
  void dump() const;
  void print(llvm::raw_ostream &) const;
  void dumpJson(llvm::raw_ostream &) const;

  Geq *createGeq(Atom *Lhs, Atom *Rhs, bool IsCheckedConstraint = true);
  Geq *createGeq(Atom *Lhs, Atom *Rhs, const std::string &Rsn,
                 bool IsCheckedConstraint = true);
  Geq *createGeq(Atom *Lhs, Atom *Rhs, const std::string &Rsn,
                 PersistentSourceLoc *PL, bool IsCheckedConstraint = true);
  Implies *createImplies(Geq *Premise, Geq *Conclusion);

  VarAtom *createFreshGEQ(std::string Name, VarAtom::VarKind VK, ConstAtom *Con,
                          std::string Rsn = DEFAULT_REASON,
                          PersistentSourceLoc *PSL = nullptr);

  VarAtom *getFreshVar(std::string Name, VarAtom::VarKind VK);
  VarAtom *getOrCreateVar(ConstraintKey V, std::string Name,
                          VarAtom::VarKind VK);
  VarAtom *getVar(ConstraintKey V) const;
  PtrAtom *getPtr() const;
  ArrAtom *getArr() const;
  NTArrAtom *getNTArr() const;
  WildAtom *getWild() const;
  ConstAtom *getAssignment(Atom *A);
  ConstraintsGraph &getChkCG();
  ConstraintsGraph &getPtrTypCG();

  void resetEnvironment();
  bool checkInitialEnvSanity();

  // Remove all constraints that were generated because of the
  // provided reason.
  bool removeAllConstraintsOnReason(std::string &Reason,
                                    ConstraintSet &RemovedCons);

private:
  ConstraintSet TheConstraints;
  // These are constraint graph representation of constraints.
  ConstraintsGraph *ChkCG;
  ConstraintsGraph *PtrTypCG;
  std::map<std::string, ConstraintSet> ConstraintsByReason;
  ConstraintsEnv Environment;

  // Confirm a constraint is well-formed
  bool check(Constraint *C);

  // Managing constraints based on the underlying reason.
  // add constraint to the map.
  bool addReasonBasedConstraint(Constraint *C);
  // Remove constraint from the map.
  bool removeReasonBasedConstraint(Constraint *C);

  VarSolTy getDefaultSolution();

  // Solve constraint set via graph-based dynamic transitive closure
  bool graphBasedSolve();

  // These atoms can be singletons, so we'll store them in the
  // Constraints class.
  PtrAtom *PrebuiltPtr;
  ArrAtom *PrebuiltArr;
  NTArrAtom *PrebuiltNTArr;
  WildAtom *PrebuiltWild;
};

#endif
