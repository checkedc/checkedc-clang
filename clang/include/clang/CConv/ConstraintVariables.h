//=--ConstraintVariables.h----------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// The class allocates constraint variables and maps program locations
// (specified by PersistentSourceLocs) to constraint variables.
//
// The allocation of constraint variables is a little nuanced. For a given
// variable, there might be multiple constraint variables. For example, some
// declaration of the form:
//
//  int **p = ... ;
//
// would be given two constraint variables, visualized like this:
//
//  int * q_(i+1) * q_i p = ... ;
//
// The constraint variable at the "highest" or outer-most level of the type
// is the lowest numbered constraint variable for a given declaration.
//===----------------------------------------------------------------------===//

#ifndef _CONSTRAINTVARIABLES_H
#define _CONSTRAINTVARIABLES_H

#include "llvm/ADT/StringSwitch.h"
#include "clang/Lex/Lexer.h"
#include "clang/AST/ASTContext.h"

#include "ProgramVar.h"
#include "Constraints.h"

using namespace clang;

class ProgramInfo;

// Holds integers representing constraint variables, with semantics as
// defined in the text above
typedef std::set<ConstraintKey> CVars;
// Holds Atoms, one for each of the pointer (*) declared in the program.
typedef std::vector<Atom*> CAtoms;

// Base class for ConstraintVariables. A ConstraintVariable can either be a
// PointerVariableConstraint or a FunctionVariableConstraint. The difference
// is that FunctionVariableConstraints have constraints on the return value
// and on each parameter.
class ConstraintVariable {
public:
  enum ConstraintVariableKind {
      PointerVariable,
      FunctionVariable
  };

  ConstraintVariableKind getKind() const { return Kind; }

private:
  ConstraintVariableKind Kind;
protected:
  std::string OriginalType;
  // Underlying name of the C variable this ConstraintVariable represents.
  std::string Name;
  // Set of constraint variables that have been constrained due to a
  // bounds-safe interface (itype). They are remembered as being constrained
  // so that later on we do not introduce a spurious constraint
  // making those variables WILD.
  std::set<ConstraintKey> ConstrainedVars;
  // A flag to indicate that we already forced argConstraints to be equated
  // Avoids infinite recursive calls.
  bool HasEqArgumentConstraints;
  // Flag to indicate if this Constraint Variable has a bounds key.
  bool ValidBoundsKey;
  // Bounds key of this Constraint Variable.
  BoundsKey BKey;
  // Is this Constraint Variable for a declaration?
  bool IsForDecl;


  // Only subclasses should call this
  ConstraintVariable(ConstraintVariableKind K, std::string T, std::string N) :
      Kind(K),OriginalType(T),Name(N), HasEqArgumentConstraints(false),
      ValidBoundsKey(false), IsForDecl(false) {}

public:
  // Create a "for-rewriting" representation of this ConstraintVariable.
  // The 'emitName' parameter is true when the generated string should include
  // the name of the variable, false for just the type.
  // The 'forIType' parameter is true when the generated string is expected
  // to be used inside an itype
  virtual std::string mkString(const EnvironmentMap &E,
                               bool emitName=true,
                               bool forItype=false,
                               bool emitPointee=false,
                               bool UnmaskTypedef = false)
    const = 0;




  // Debug printing of the constraint variable.
  virtual void print(llvm::raw_ostream &O) const = 0;
  virtual void dump() const = 0;
  virtual void dump_json(llvm::raw_ostream &O) const = 0;

  virtual bool hasItype() const = 0;
  bool hasBoundsKey() const { return ValidBoundsKey; }
  BoundsKey getBoundsKey() const {
    assert(ValidBoundsKey && "No valid Bkey");
    return BKey;
  }
  void setBoundsKey(BoundsKey NK) {
    ValidBoundsKey = true;
    BKey = NK;
  }

  virtual bool solutionEqualTo(Constraints &, const ConstraintVariable *) const
    = 0;

  // Constrain all pointers in this ConstraintVariable to be Wild.
  virtual void constrainToWild(Constraints &CS) const = 0;
  virtual void constrainToWild(Constraints &CS,
                               const std::string &Rsn) const = 0;
  virtual void constrainToWild(Constraints &CS, const std::string &Rsn,
                               PersistentSourceLoc *PL) const = 0;

  // Returns true if any of the constraint variables 'within' this instance
  // have a binding in E other than top. E should be the EnvironmentMap that
  // results from running unification on the set of constraints and the
  // environment.
  bool isChecked(const EnvironmentMap &E) const;

  // Returns true if this constraint variable has a different checked type after
  // running unification. Note that if the constraint variable had a checked
  // type in the input program, it will have the same checked type after solving
  // so, the type will not have changed. To test if the type is checked, use
  // isChecked instead.
  virtual bool anyChanges(const EnvironmentMap &E) const = 0;

  // Here, AIdx is the pointer level which needs to be checked.
  // By default, we check for all pointer levels (or VarAtoms)
  virtual bool hasWild(const EnvironmentMap &E, int AIdx = -1) const = 0;
  virtual bool hasArr(const EnvironmentMap &E, int AIdx = -1) const = 0;
  virtual bool hasNtArr(const EnvironmentMap &E, int AIdx = -1) const = 0;

  // Force use of equality constraints in function calls for this CV
  virtual void equateArgumentConstraints(ProgramInfo &I) = 0;

  // Update this CV with information from duplicate declaration CVs
  virtual void brainTransplant(ConstraintVariable *, ProgramInfo &) = 0;
  virtual void mergeDeclaration(ConstraintVariable *, ProgramInfo &) = 0;

  std::string getOriginalTy() const { return OriginalType; }
  // Get the original type string that can be directly
  // used for rewriting.
  std::string getRewritableOriginalTy() const;
  std::string getName() const { return Name; }

  void setValidDecl() { IsForDecl = true; }
  bool isForValidDecl() const { return IsForDecl; }

  virtual ConstraintVariable *getCopy(Constraints &CS) = 0;

  virtual ~ConstraintVariable() {};

  virtual bool getIsOriginallyChecked() const = 0;
};

typedef std::set<ConstraintVariable *> CVarSet;
typedef Option<ConstraintVariable> CVarOption;

enum ConsAction {
  Safe_to_Wild,
  Wild_to_Safe,
  Same_to_Same
};

void constrainConsVarGeq(const std::set<ConstraintVariable *> &LHS,
                         const std::set<ConstraintVariable *> &RHS,
                         Constraints &CS, PersistentSourceLoc *PL,
                         ConsAction CA, bool doEqType,
                         ProgramInfo *Info,
                         bool HandleBoundsKey = true);
void constrainConsVarGeq(ConstraintVariable *LHS, const CVarSet &RHS,
                         Constraints &CS, PersistentSourceLoc *PL,
                         ConsAction CA, bool doEqType,
                         ProgramInfo *Info,
                         bool HandleBoundsKey = true);
void constrainConsVarGeq(ConstraintVariable *LHS,
                         ConstraintVariable *RHS,
                         Constraints &CS, PersistentSourceLoc *PL,
                         ConsAction CA, bool doEqType,
                         ProgramInfo *Info,
                         bool HandleBoundsKey = true);

// True if [C] is a PVConstraint that contains at least one Atom (i.e.,
//   it represents a C pointer)
bool isAValidPVConstraint(const ConstraintVariable *C);

class PointerVariableConstraint;
class FunctionVariableConstraint;

// Represents an individual constraint on a pointer variable.
// This could contain a reference to a FunctionVariableConstraint
// in the case of a function pointer declaration.
class PointerVariableConstraint : public ConstraintVariable {
public:
  enum Qualification {
      ConstQualification,
      VolatileQualification,
      RestrictQualification
  };

  static PointerVariableConstraint *getWildPVConstraint(Constraints &CS);
  static PointerVariableConstraint *getPtrPVConstraint(Constraints &CS);
  static PointerVariableConstraint *getNonPtrPVConstraint(Constraints &CS);
  static PointerVariableConstraint *getNamedNonPtrPVConstraint(StringRef name, Constraints &CS);

private:
  std::string BaseType;
  CAtoms vars;
  FunctionVariableConstraint *FV;
  std::map<uint32_t, std::set<Qualification>> QualMap;
  enum OriginalArrType {
      O_Pointer,
      O_SizedArray,
      O_UnSizedArray
  };
  // Map from pointer idx to original type and size.
  // If the original variable U was:
  //  * A pointer, then U -> (a,b) , a = O_Pointer, b has no meaning.
  //  * A sized array, then U -> (a,b) , a = O_SizedArray, b is static size.
  //  * An unsized array, then U -(a,b) , a = O_UnSizedArray, b has no meaning.
  std::map<uint32_t ,std::pair<OriginalArrType,uint64_t>> arrSizes;
  // If for all U in arrSizes, any U -> (a,b) where a = O_SizedArray or
  // O_UnSizedArray, arrPresent is true.
  bool ArrPresent;
  // Is there an itype associated with this constraint? If there is, how was it
  // originally stored in the program?
  std::string ItypeStr;
  // Get the qualifier string (e.g., const, etc) for the provided
  // pointer type into the provided string stream (ss).
  void getQualString(uint32_t TypeIdx, std::ostringstream &Ss) const;
  void insertQualType(uint32_t TypeIdx, QualType &QTy);
  // This function tries to emit an array size for the variable.
  // and returns true if the variable is an array and a size is emitted.
  bool emitArraySize(std::stack<std::string> &CheckedArrs, uint32_t TypeIdx,
                     bool &AllArray, bool &ArrayRun, bool Nt) const;
  void addArrayAnnotations(std::stack<std::string> &CheckedArrs,
                           std::deque<std::string> &EndStrs) const;

  // Utility used by the constructor to extract string representation of the
  // base type that preserves macros where possible.
  static std::string extractBaseType(DeclaratorDecl *D, QualType QT,
                                     const Type *Ty, const ASTContext &C);

  // Flag to indicate that this constraint is a part of function prototype
  // e.g., Parameters or Return.
  bool partOFFuncPrototype;
  // For the function parameters and returns,
  // this set contains the constraint variable of
  // the values used as arguments.
  std::set<ConstraintVariable *> argumentConstraints;
  // Get solution for the atom of a pointer.
  const ConstAtom *getSolution(const Atom *A, const EnvironmentMap &E) const;

  PointerVariableConstraint(PointerVariableConstraint *Ot, Constraints &CS);
  PointerVariableConstraint *Parent;
  // String representing declared bounds expression.
  std::string BoundsAnnotationStr;

  // Does this variable represent a generic type?
  // Generic types can be used with fewer restrictions, so this field is used
  // stop assignments wth generic variables from forcing constraint variables
  // to be wild.
  bool IsGeneric;

  // Empty array pointers are represented the same as standard pointers. This
  // lets pointers be passed to functions expecting a zero width array. This
  // flag is used to discriminate between standard pointer and zero width array
  // pointers.
  bool IsZeroWidthArray;

  // Was this variable a checked pointer in the input program?
  // This is important for two reasons: (1) externs that are checked should be
  // kept that way during solving, (2) nothing that was originally checked
  // should be modified during rewriting.
  bool OriginallyChecked;

  bool IsTypedef = false;
  TypedefNameDecl* TDT;
  std::string typedefString;

public:
  // Constructor for when we know a CVars and a type string.
  PointerVariableConstraint(CAtoms V, std::string T, std::string Name,
                            FunctionVariableConstraint *F, bool isArr,
                            bool isItype, std::string is, bool Generic=false) :
          ConstraintVariable(PointerVariable, "" /*not used*/, Name),
          BaseType(T),vars(V),FV(F), ArrPresent(isArr), ItypeStr(is),
          partOFFuncPrototype(false), Parent(nullptr),
          BoundsAnnotationStr(""), IsGeneric(Generic), IsZeroWidthArray(false),
          OriginallyChecked(false) {}

  std::string getTy() const { return BaseType; }
  bool getArrPresent() const { return ArrPresent; }
  // Check if the outermost pointer is an unsized array.
  bool isTopCvarUnsizedArr() const;
  // Check if any of the pointers is either a sized or unsized arr.
  bool hasSomeSizedArr() const;

  bool isTypedef(void);
  void setTypedef(TypedefNameDecl *TypedefType, std::string);

  // Is an itype present for this constraint? If yes,
  // what is the text of that itype?
  bool hasItype() const override { return ItypeStr.size() > 0; }
  std::string getItype() const { return ItypeStr; }
  // Check if this variable has bounds annotation.
  bool hasBoundsStr() const { return !BoundsAnnotationStr.empty(); }
  // Get bounds annotation.
  std::string getBoundsStr() const { return BoundsAnnotationStr; }

  bool getIsGeneric() const { return IsGeneric; }

  bool getIsOriginallyChecked() const override { return OriginallyChecked; }

  bool solutionEqualTo(Constraints &CS,
                       const ConstraintVariable *CV) const override;

  // Construct a PVConstraint when the variable is generated by a specific
  // declaration (D). This constructor calls the next constructor below with
  // QT and N instantiated with information in D.
  PointerVariableConstraint(clang::DeclaratorDecl *D,
                            ProgramInfo &I, const clang::ASTContext &C);

  // QT: Defines the type for the constraint variable. One atom is added for
  //     each level of pointer (or array) indirection in the type.
  // D: If this constraint is generated because of a variable declaration, this
  //    should be a pointer to the the declaration AST node. May be null if the
  //    constraint is not generated by a declaration.
  // N: Name for the constraint variable. This may be chosen arbitrarily, as it
  //    it is only used for labeling graph nodes. Equality is based on generated
  //    unique ids.
  // I and C: Context objects required for this construction. It is expected
  //          that all constructor calls will take the same global objects here.
  // inFunc: If this variable is part of a function prototype, this string is
  //         the name of the function. nullptr otherwise.
  // IsGeneric: CheckedC supports generic types (_Itype_for_any) which need less
  //            restrictive constraints. Set to true to indicate that this
  //            variable is generic.
  PointerVariableConstraint(const clang::QualType &QT,
                            clang::DeclaratorDecl *D, std::string N,
                            ProgramInfo &I,
                            const clang::ASTContext &C,
                            std::string *inFunc = nullptr,
                            bool IsGeneric = false);

  const CAtoms &getCvars() const { return vars; }

  void brainTransplant(ConstraintVariable *From, ProgramInfo &I) override;
  void mergeDeclaration(ConstraintVariable *From, ProgramInfo &I) override;

  static bool classof(const ConstraintVariable *S) {
    return S->getKind() == PointerVariable;
  }

  std::string mkString(const EnvironmentMap &E,
                       bool EmitName =true,
                       bool ForItype = false,
                       bool EmitPointee = false,
                       bool UnmaskTypedef = false)
    const override;

  FunctionVariableConstraint *getFV() const { return FV; }

  void print(llvm::raw_ostream &O) const override ;
  void dump() const override { print(llvm::errs()); }
  void dump_json(llvm::raw_ostream &O) const override;
  void constrainToWild(Constraints &CS) const override;
  void constrainToWild(Constraints &CS, const std::string &Rsn) const override;
  void constrainToWild(Constraints &CS, const std::string &Rsn,
                       PersistentSourceLoc *PL) const override;
  void constrainOuterTo(Constraints &CS, ConstAtom *C, bool doLB = false);
  bool anyChanges(const EnvironmentMap &E) const override;
  bool anyArgumentIsWild(const EnvironmentMap &E);
  bool hasWild(const EnvironmentMap &E, int AIdx = -1) const override;
  bool hasArr(const EnvironmentMap &E, int AIdx = -1) const override;
  bool hasNtArr(const EnvironmentMap &E, int AIdx = -1) const override;

  void equateArgumentConstraints(ProgramInfo &I) override;

  bool isPartOfFunctionPrototype() const  { return partOFFuncPrototype; }
  // Add the provided constraint variable as an argument constraint.
  bool addArgumentConstraint(ConstraintVariable *DstCons, ProgramInfo &Info);
  // Get the set of constraint variables corresponding to the arguments.
  const std::set<ConstraintVariable *> &getArgumentConstraints() const;

  ConstraintVariable *getCopy(Constraints &CS) override;

  // Retrieve the atom at the specified index. This function includes special
  // handling for generic constraint variables to create deeper pointers as
  // they are needed.
  Atom *getAtom(unsigned int AtomIdx, Constraints &CS);

  ~PointerVariableConstraint() override {};
};

typedef PointerVariableConstraint PVConstraint;
// Name for function return, for debugging
#define RETVAR "$ret"

typedef struct {
  PersistentSourceLoc PL;
  std::vector<CVarSet> PS;
} ParamDeferment;


// Constraints on a function type. Also contains a 'name' parameter for
// when a re-write of a function pointer is needed.
class FunctionVariableConstraint : public ConstraintVariable {
private:
  FunctionVariableConstraint(FunctionVariableConstraint *Ot,
                             Constraints &CS);
  // N constraints on the return value of the function.
  ConstraintVariable *ReturnVar;
  // A vector of K sets of N constraints on the parameter values, for
  // K parameters accepted by the function.
  std::vector<ConstraintVariable *> ParamVars;
  // Storing of parameters in the case of untyped prototypes
  std::vector<ParamDeferment> deferredParams;
  // File name in which this declaration is found.
  std::string FileName;
  bool Hasproto;
  bool Hasbody;
  bool IsStatic;
  FunctionVariableConstraint *Parent;
  // Flag to indicate whether this is a function pointer or not.
  bool IsFunctionPtr;

  void equateFVConstraintVars(ConstraintVariable *CV, ProgramInfo &Info) const;

public:
  FunctionVariableConstraint() :
          ConstraintVariable(FunctionVariable, "", ""),
                                 FileName(""), Hasproto(false),
        Hasbody(false), IsStatic(false), Parent(nullptr),
                                 IsFunctionPtr(false) { }

  FunctionVariableConstraint(clang::DeclaratorDecl *D,
                             ProgramInfo &I, const clang::ASTContext &C);
  FunctionVariableConstraint(const clang::Type *Ty,
                             clang::DeclaratorDecl *D, std::string N,
                             ProgramInfo &I, const clang::ASTContext &C);

  ConstraintVariable *getReturnVar() const {
    return ReturnVar;
  }

  const std::vector<ParamDeferment> &getDeferredParams() const {
    return deferredParams;
  }

  void addDeferredParams(PersistentSourceLoc PL,
                         std::vector<CVarSet> Ps);

  size_t numParams() const { return ParamVars.size(); }

  bool hasProtoType() const { return Hasproto; }
  bool hasBody() const { return Hasbody; }
  void setHasBody(bool hbody) { this->Hasbody = hbody; }

  static bool classof(const ConstraintVariable *S) {
    return S->getKind() == FunctionVariable;
  }

  void brainTransplant(ConstraintVariable *From, ProgramInfo &I) override;
  void mergeDeclaration(ConstraintVariable *FromCV, ProgramInfo &I) override;

  ConstraintVariable *getParamVar(unsigned i) const {
    assert(i < ParamVars.size());
    return ParamVars.at(i);
  }

  bool hasItype() const override;
  bool solutionEqualTo(Constraints &CS,
                       const ConstraintVariable *CV) const override;

  std::string mkString(const EnvironmentMap &E, bool EmitName =true,
                       bool ForItype = false,
                       bool EmitPointee = false,
                       bool UnmaskTypedef = false)
    const override;
  void print(llvm::raw_ostream &O) const override;
  void dump() const override { print(llvm::errs()); }
  void dump_json(llvm::raw_ostream &O) const override;
  void constrainToWild(Constraints &CS) const override;
  void constrainToWild(Constraints &CS, const std::string &Rsn) const override;
  void constrainToWild(Constraints &CS, const std::string &Rsn,
                       PersistentSourceLoc *PL) const override;
  bool anyChanges(const EnvironmentMap &E) const override;
  bool hasWild(const EnvironmentMap &E, int AIdx = -1) const override;
  bool hasArr(const EnvironmentMap &E, int AIdx = -1) const override;
  bool hasNtArr(const EnvironmentMap &E, int AIdx = -1) const override;

  void equateArgumentConstraints(ProgramInfo &P) override;

  ConstraintVariable *getCopy(Constraints &CS) override;

  bool getIsOriginallyChecked() const override;

  ~FunctionVariableConstraint() override {};
};

typedef FunctionVariableConstraint FVConstraint;

#endif //_CONSTRAINTVARIABLES_H
