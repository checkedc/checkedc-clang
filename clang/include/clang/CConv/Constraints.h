//=--Constraints.h------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This implements a simple constraint solver for expressions of the form:
//  a = b
//  a implies b
//
// The Checked C converter tool performs type inference to identify locations
// where a C type might be replaced with a Checked C type. This interface
// does the solving to figure out where those substitions might happen.
//===----------------------------------------------------------------------===//

#ifndef _CONSTRAINTS_H
#define _CONSTRAINTS_H
#include "llvm/Support/Casting.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"
#include <set>
#include <map>
#include "Utils.h"

class Constraint;
class ConstraintVariable;
class Constraints;
class PersistentSourceLoc;

#define DEFAULT_REASON "UNKNOWN_REASON"

template<typename T>
struct PComp
{
  bool operator()(const T Lhs, const T Rhs) const {
    return *Lhs < *Rhs;
  }
};

class VarAtom;

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
  virtual void dump_json(llvm::raw_ostream &) const = 0;
  std::string getStr() {
    std::string Buf;
    llvm::raw_string_ostream TmpS(Buf);
    print(TmpS);
    return TmpS.str();
  }
  virtual bool operator==(const Atom &) const = 0;
  virtual bool operator!=(const Atom &) const = 0;
  virtual bool operator<(const Atom &) const = 0;
  // Check if this atom contains the provided atom.
  virtual bool containsConstraint(VarAtom *) = 0;
};

class ConstAtom : public Atom {
public:
  ConstAtom() : Atom(A_Const) {}
  ConstAtom(AtomKind K) : Atom(K) {}

  static bool classof(const Atom *A) {
    // Something is a class of ConstAtom if it ISN'T a Var.
    return A->getKind() != A_Var;
  }

  virtual bool containsConstraint(VarAtom *) {
    // Constant atom can never contain a VarAtom.
    return false;
  }
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

  void dump_json(llvm::raw_ostream &O) const {
    O << "\"q_" << Loc << "\"";
  }

  bool operator==(const Atom &Other) const {
    if (const VarAtom *V = llvm::dyn_cast<VarAtom>(&Other))
      return V->Loc == Loc;
    else 
      return false;
  }

  bool operator!=(const Atom &Other) const {
    return !(*this == Other);
  }

  bool operator<(const Atom &Other) const {
    if (const VarAtom *V = llvm::dyn_cast<VarAtom>(&Other))
      return Loc < V->Loc;
    else
      return false;
  }

  uint32_t getLoc() const {
    return Loc;
  }

  bool containsConstraint(VarAtom *ToFind) {
    // This is a VarAtom and contains is same as equality.
    return (*this == *ToFind);
  }

  // Returns the constraints associated with this atom.
  std::set<Constraint *, PComp<Constraint *>> &getAllConstraints() {
    return Constraints;
  }

private:
  uint32_t  Loc;
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

  static bool classof(const Atom *S) {
    return S->getKind() == A_NTArr;
  }

  void print(llvm::raw_ostream &O) const {
    O << "NTARR";
  }

  void dump(void) const {
    print(llvm::errs());
  }

  void dump_json(llvm::raw_ostream &O) const {
    O << "\"NTARR\"";
  }

  bool operator==(const Atom &Other) const {
    return llvm::isa<NTArrAtom>(&Other);
  }

  bool operator!=(const Atom &Other) const {
    return !(*this == Other);
  }

  bool operator<(const Atom &Other) const {
    if (*this == Other)
      return false;
    else
      return true;
  }
};

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

  void dump_json(llvm::raw_ostream &O) const {
    O << "\"ARR\"";
  }

  bool operator==(const Atom &Other) const {
    return llvm::isa<ArrAtom>(&Other);
  }

  bool operator!=(const Atom &Other) const {
    return !(*this == Other);
  }

  bool operator<(const Atom &Other) const {
    if (llvm::isa<NTArrAtom>(&Other) || *this == Other)
      return false;
    else
      return true;
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

  void dump_json(llvm::raw_ostream &O) const {
    O << "\"PTR\"";
  }

  bool operator==(const Atom &Other) const {
    return llvm::isa <PtrAtom>(&Other);
  }

  bool operator!=(const Atom &Other) const {
    return !(*this == Other);
  }

  bool operator<(const Atom &Other) const {
    if (llvm::isa<ArrAtom>(&Other) || llvm::isa<NTArrAtom>(&Other) ||
	    *this == Other)
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

  void dump_json(llvm::raw_ostream &O) const {
    O << "\"WILD\"";
  }

  bool operator==(const Atom &Other) const {
    if (llvm::isa<WildAtom>(&Other))
      return true;
    else
      return false;
  }

  bool operator!=(const Atom &Other) const {
    return !(*this == Other);
  }

  bool operator<(const Atom &Other) const {
    if (llvm::isa<ArrAtom>(&Other) ||
        llvm::isa<NTArrAtom>(&Other) || llvm::isa<PtrAtom>(&Other) ||
            *this == Other)
      return false;
    else
      return true;
  }
};


// Represents constraints of the form:
//  - a = b
//  - a >= b
//  - a => b
class Constraint {
public:
  enum ConstraintKind {
    C_Eq,
    C_Geq,
    C_Imp
  };
private:
  const ConstraintKind Kind;
public:
  std::string REASON = DEFAULT_REASON;
  std::string FileName = "";
  unsigned LineNo = 0;
  unsigned ColStart = 0;
  Constraint(ConstraintKind K) : Kind(K) { }
  Constraint(ConstraintKind K, std::string &rsn) : Kind(K) {
    REASON = rsn;
  }
  Constraint(ConstraintKind K, std::string &Rsn, PersistentSourceLoc *PL);

  virtual ~Constraint() {}

  ConstraintKind getKind() const { return Kind; }

  virtual void print(llvm::raw_ostream &) const = 0;
  virtual void dump(void) const = 0;
  virtual void dump_json(llvm::raw_ostream &) const = 0;
  virtual bool operator==(const Constraint &other) const = 0;
  virtual bool operator!=(const Constraint &other) const = 0;
  virtual bool operator<(const Constraint &other) const = 0;
  virtual std::string getReason() {
    return REASON;
  }
  // Check if the provided constraint contains the
  // provided VarAtom.
  virtual bool containsConstraint(VarAtom *toFind) = 0;
};

// a >= b
class Geq : public Constraint {
    friend class VarAtom;
public:

    Geq(Atom *Lhs, Atom *Rhs, bool isCC = true)
            : Constraint(C_Geq), lhs(Lhs), rhs(Rhs), isCheckedConstraint(isCC) {}

    Geq(Atom *Lhs, Atom *Rhs, std::string &Rsn, bool isCC = true)
            : Constraint(C_Geq, Rsn), lhs(Lhs), rhs(Rhs), isCheckedConstraint(isCC) {}

    Geq(Atom *Lhs, Atom *Rhs, std::string &Rsn, PersistentSourceLoc *PL, bool isCC = true)
            : Constraint(C_Geq, Rsn, PL), lhs(Lhs), rhs(Rhs), isCheckedConstraint(isCC) {}

    static bool classof(const Constraint *C) {
        return C->getKind() == C_Geq;
    }

    void print(llvm::raw_ostream &O) const {
        lhs->print(O);
        std::string kind = isCheckedConstraint ? " (C)>= " : " (P)>= ";
        O << kind;
        rhs->print(O);
        O << ", Reason:" << REASON;
    }

    void dump(void) const {
        print(llvm::errs());
    }

    void dump_json(llvm::raw_ostream &O) const {
        O << "{\"Geq\":{\"Atom1\":";
        lhs->dump_json(O);
        O << ", \"Atom2\":";
        rhs->dump_json(O);
        O << ", \"isChecked\":";
        O << (isCheckedConstraint ? "true" : "false");
        O << ", \"Reason\":";
        llvm::json::Value reasonVal(REASON);
        O << reasonVal;
        O << "}}";
    }

    Atom *getLHS(void) const { return lhs; }
    Atom *getRHS(void) const { return rhs; }
    void setChecked(ConstAtom *C) {
      assert(!isCheckedConstraint && llvm::isa<ConstAtom>(lhs));
      lhs = rhs; // reverse direction for checked constraint
      rhs = C;
      isCheckedConstraint = true;
    }
    bool constraintIsChecked(void) const { return isCheckedConstraint; }

    bool operator==(const Constraint &Other) const {
        if (const Geq *E = llvm::dyn_cast<Geq>(&Other))
            return *lhs == *E->lhs && *rhs == *E->rhs
                 && isCheckedConstraint == E->isCheckedConstraint;
        else
            return false;
    }

    bool operator!=(const Constraint &Other) const {
        return !(*this == Other);
    }

    bool operator<(const Constraint &Other) const {
        ConstraintKind K = Other.getKind();
        if (K == C_Geq) {
            const Geq *E = llvm::dyn_cast<Geq>(&Other);
            assert(E != nullptr);
            if (*lhs == *E->lhs) {
              if (*rhs == *E->rhs) {
                if (isCheckedConstraint == E->isCheckedConstraint)
                  return false;
                else
                  return isCheckedConstraint < E->isCheckedConstraint;
              } else
                return *rhs < *E->rhs;
            }
            else
              return *lhs < *E->lhs;
        }
        else
            return C_Geq < K;
    }

    bool containsConstraint(VarAtom *ToFind) {
        return lhs->containsConstraint(ToFind) || rhs->containsConstraint(ToFind);
    }

private:
  Atom *lhs;
  Atom *rhs;
  bool isCheckedConstraint;
};

// a => b
class Implies : public Constraint {
public:

  Implies(Geq *Premise, Geq *Conclusion)
    : Constraint(C_Imp), premise(Premise), conclusion(Conclusion) {}

  static bool classof(const Constraint *C) {
    return C->getKind() == C_Imp;
  }

  Geq *getPremise() { return premise; }
  Geq *getConclusion() { return conclusion; }

  void print(llvm::raw_ostream &O) const {
    premise->print(O);
    O << " => ";
    conclusion->print(O);
  }

  void dump(void) const {
    print(llvm::errs());
  }

  void dump_json(llvm::raw_ostream &O) const {
    O << "{\"Implies\":{\"Premise\":";
    premise->dump_json(O);
    O << ", \"Conclusion\":";
    conclusion->dump_json(O);
    O << "}}";
  }

  bool operator==(const Constraint &Other) const {
    if (const Implies *I = llvm::dyn_cast<Implies>(&Other))
      return *premise == *I->premise && *conclusion == *I->conclusion;
    else 
      return false;
  }

  bool operator!=(const Constraint &Other) const {
    return !(*this == Other);
  }
 
  bool operator<(const Constraint &Other) const {
    ConstraintKind K = Other.getKind();
    if (K == C_Imp) {
      const Implies *I = llvm::dyn_cast<Implies>(&Other);
      assert(I != nullptr);

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

  bool containsConstraint(VarAtom *ToFind) {
    return premise->containsConstraint(ToFind) ||
           conclusion->containsConstraint(ToFind);
  }

private:
  Geq *premise;
  Geq *conclusion;
};

// This is the solution, the first item is Checked Solution and the second
// is Ptr solution.
typedef std::pair<ConstAtom *, ConstAtom *> VarSolTy;
typedef std::map<VarAtom *, VarSolTy, PComp<VarAtom *>> EnvironmentMap;

typedef uint32_t ConstraintKey;

class ConstraintsEnv {

public:
  ConstraintsEnv() : consFreeKey(0) { environment.clear(); }
  EnvironmentMap &getVariables() { return environment; }
  void dump() const;
  void print(llvm::raw_ostream &) const;
  void dump_json(llvm::raw_ostream &) const;
  VarAtom *getFreshVar(VarSolTy InitC);
  VarAtom *getOrCreateVar(ConstraintKey V, VarSolTy InitC);
  VarAtom *getVar(ConstraintKey V) const;
  ConstAtom *getAssignment(Atom *A, bool IsChecked);
  bool assign(VarAtom *V, ConstAtom *C, bool IsChecked);
  void mergePtrTypes();
  void resetSolution(VarSolTy InitC);
  bool checkAssignment(ConstAtom *C);

private:
  EnvironmentMap environment;
  // Next available integer to assign to a variable.
  uint32_t consFreeKey;
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
  typedef std::map<std::string, std::set<ConstraintVariable *>> FuncKeyToConsMap;

  bool removeConstraint(Constraint *C);
  bool addConstraint(Constraint *c);
  // It's important to return these by reference. Programs can have 
  // 10-100-100000 constraints and variables, and copying them each time
  // a client wants to examine the environment is untenable.
  ConstraintSet &getConstraints() { return constraints; }
  EnvironmentMap &getVariables() { return environment.getVariables(); }
  
  EnvironmentMap &getitypeVarMap() { return itypeConstraintVars; }

  void editConstraintHook(Constraint *C);

  // Solve the system of constraints. Return true in the second position if
  // the system is solved. If the system is solved, the first position is 
  // an empty. If the system could not be solved, the constraints in conflict
  // are returned in the first position.
  // TODO: this functionality is not implemented yet.
  std::pair<ConstraintSet, bool> solve(unsigned &NumOfIter);
  void dump() const;
  void print(llvm::raw_ostream &) const;
  void dump_json(llvm::raw_ostream &) const;

  Geq *createGeq(Atom *Lhs, Atom *Rhs, bool isCheckedConstraint = true);
  Geq *createGeq(Atom *Lhs, Atom *Rhs, std::string &Rsn, bool isCheckedConstraint = true);
  Geq *createGeq(Atom *Lhs, Atom *Rhs, std::string &Rsn, PersistentSourceLoc *PL, bool isCheckedConstraint = true);
  Implies *createImplies(Geq *Premise, Geq *Conclusion);

  VarAtom *getFreshVar();
  VarAtom *getOrCreateVar(ConstraintKey V);
  VarAtom *getVar(ConstraintKey V) const;
  PtrAtom *getPtr() const;
  ArrAtom *getArr() const;
  NTArrAtom *getNTArr() const;
  WildAtom *getWild() const;
  ConstAtom *getAssignment(Atom *A);

  // Reset all constraint variables to Ptrs.
  void resetEnvironment();

  // Check the sanity of environment map before solving the constraints.
  bool checkInitialEnvSanity();

  // Remove all constraints that were generated because of the
  // provided reason.
  bool removeAllConstraintsOnReason(std::string &Reason,
                                    ConstraintSet &RemovedCons);
  bool graph_based_solve(unsigned &Niter, ConstraintSet &Conflicts);
private:
  ConstraintSet constraints;
  std::map<std::string, ConstraintSet> constraintsByReason;
  ConstraintsEnv environment;
  // Map of constraint variables, which are identified
  // as itype pointers
  // These should be the constraint variables of only
  // function parameters or returns.
  EnvironmentMap itypeConstraintVars;

  // Map of function unique key to it declaration FVConstraintVariable.
  FuncKeyToConsMap FuncDeclConstraints;
  // Map of function unique key to it definition FVConstraintVariable.
  FuncKeyToConsMap FuncDefnConstraints;

  bool check(Constraint *C);

  // Managing constraints based on the underlying reason.
  // add constraint to the map.
  bool addReasonBasedConstraint(Constraint *C);
  // Remove constraint from the map.
  bool removeReasonBasedConstraint(Constraint *C);

  VarSolTy getDefaultSolution();

  // These atoms can be singletons, so we'll store them in the 
  // Constraints class.
  PtrAtom *PrebuiltPtr;
  ArrAtom *PrebuiltArr;
  NTArrAtom *PrebuiltNTArr;
  WildAtom *PrebuiltWild;
};


#endif
