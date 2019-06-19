//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
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

#include "Constraints.h"

using namespace clang;

class ProgramInfo;

// Holds integers representing constraint variables, with semantics as
// defined in the text above
typedef std::set<ConstraintKey> CVars;

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
  std::string BaseType;
  // Underlying name of the C variable this ConstraintVariable represents.
  std::string Name;
  // Set of constraint variables that have been constrained due to a
  // bounds-safe interface. They are remembered as being constrained
  // so that later on we do not introduce a spurious constraint
  // making those variables WILD.
  std::set<ConstraintKey> ConstrainedVars;

public:
  ConstraintVariable(ConstraintVariableKind K, std::string T, std::string N) :
          Kind(K),BaseType(T),Name(N) {}

  // Create a "for-rewriting" representation of this ConstraintVariable.
  // The 'emitName' parameter is true when the generated string should include
  // the name of the variable, false for just the type.
  // The 'forIType' parameter is true when the generated string is expected
  // to be used inside an itype
  virtual std::string mkString(Constraints::EnvironmentMap &E, bool emitName=true, bool forItype=false) = 0;

  // Debug printing of the constraint variable.
  virtual void print(llvm::raw_ostream &O) const = 0;
  virtual void dump() const = 0;

  // Constrain everything 'within' this ConstraintVariable to be equal to C.
  // Set checkSkip to true if you would like constrainTo to consider the
  // ConstrainedVars when applying constraints. This should be set when
  // applying constraints due to external symbols, during linking.
  virtual void constrainTo(Constraints &CS, ConstAtom *C, bool checkSkip=false) = 0;

  // Returns true if any of the constraint variables 'within' this instance
  // have a binding in E other than top. E should be the EnvironmentMap that
  // results from running unification on the set of constraints and the
  // environment.
  virtual bool anyChanges(Constraints::EnvironmentMap &E) = 0;
  virtual bool hasWild(Constraints::EnvironmentMap &E) = 0;
  virtual bool hasArr(Constraints::EnvironmentMap &E) = 0;

  std::string getTy() { return BaseType; }
  std::string getName() { return Name; }

  virtual ~ConstraintVariable() {};

  // Constraint atoms may be either constants or variables. The constants are
  // trivial to compare, but the variables can only really be compared under
  // a specific valuation. That valuation is stored in the ProgramInfo data
  // structure, so these functions (isLt, isEq) compare two ConstraintVariables
  // with a specific assignment to the variables in mind.
  virtual bool isLt(const ConstraintVariable &other, ProgramInfo &I) const = 0;
  virtual bool isEq(const ConstraintVariable &other, ProgramInfo &I) const = 0;
  // Sometimes, constraint variables can be produced that are empty. This
  // tests for the existence of those constraint variables.
  virtual bool isEmpty(void) const = 0;

  // A helper function for isLt and isEq where the last parameter is a lambda
  // for the specific comparison operation to perform.
  virtual bool liftedOnCVars(const ConstraintVariable &O,
                             ProgramInfo &Info,
                             llvm::function_ref<bool (ConstAtom *, ConstAtom *)>) const = 0;

};

class PointerVariableConstraint;
class FunctionVariableConstraint;

// Represents an individual constraint on a pointer variable.
// This could contain a reference to a FunctionVariableConstraint
// in the case of a function pointer declaration.
class PointerVariableConstraint : public ConstraintVariable {
public:
  enum Qualification {
      ConstQualification,
      StaticQualification
  };
private:
  CVars vars;
  FunctionVariableConstraint *FV;
  std::map<ConstraintKey, Qualification> QualMap;
  enum OriginalArrType {
      O_Pointer,
      O_SizedArray,
      O_UnSizedArray
  };
  // Map from constraint variable to original type and size.
  // If the original variable U was:
  //  * A pointer, then U -> (a,b) , a = O_Pointer, b has no meaning.
  //  * A sized array, then U -> (a,b) , a = O_SizedArray, b is static size.
  //  * An unsized array, then U -(a,b) , a = O_UnSizedArray, b has no meaning.
  std::map<ConstraintKey,std::pair<OriginalArrType,uint64_t>> arrSizes;
  // If for all U in arrSizes, any U -> (a,b) where a = O_SizedArray or
  // O_UnSizedArray, arrPresent is true.
  bool arrPresent;
  // Is there an itype associated with this constraint? If there is, how was it
  // originally stored in the program?
  std::string itypeStr;
  // get the qualifier string (e.g., const, etc) for the provided constraint var (targetCvar)
  // into the provided string stream (ss)
  void getQualString(ConstraintKey targetCVar, std::ostringstream &ss);
public:
  // Constructor for when we know a CVars and a type string.
  PointerVariableConstraint(CVars V, std::string T, std::string Name,
                            FunctionVariableConstraint *F, bool isArr, bool isItype, std::string is) :
          ConstraintVariable(PointerVariable, T, Name)
          ,vars(V),FV(F),arrPresent(isArr), itypeStr(is) {}

  bool getArrPresent() { return arrPresent; }

  // Is an itype present for this constraint? If yes, what is the text of that itype?
  bool getItypePresent() { return itypeStr.size() > 0; }
  std::string getItype() { return itypeStr; }

  // Constructor for when we have a Decl. K is the current free
  // constraint variable index. We don't need to explicitly pass
  // the name because it's available in 'D'.
  PointerVariableConstraint(clang::DeclaratorDecl *D, ConstraintKey &K,
                            Constraints &CS, const clang::ASTContext &C);

  // Constructor for when we only have a Type. Needs a string name
  // N for the name of the variable that this represents.
  PointerVariableConstraint(const clang::QualType &QT, ConstraintKey &K,
                            clang::DeclaratorDecl *D, std::string N, Constraints &CS, const clang::ASTContext &C);

  const CVars &getCvars() const { return vars; }

  static bool classof(const ConstraintVariable *S) {
    return S->getKind() == PointerVariable;
  }

  std::string mkString(Constraints::EnvironmentMap &E, bool emitName=true, bool forItype=false);

  FunctionVariableConstraint *getFV() { return FV; }

  void print(llvm::raw_ostream &O) const ;
  void dump() const { print(llvm::errs()); }
  void constrainTo(Constraints &CS, ConstAtom *C, bool checkSkip=false);
  bool anyChanges(Constraints::EnvironmentMap &E);
  bool hasWild(Constraints::EnvironmentMap &E);
  bool hasArr(Constraints::EnvironmentMap &E);

  bool isLt(const ConstraintVariable &other, ProgramInfo &P) const;
  bool isEq(const ConstraintVariable &other, ProgramInfo &P) const;
  bool isEmpty(void) const { return vars.size() == 0; }

  bool liftedOnCVars(const ConstraintVariable &O,
                     ProgramInfo &Info,
                     llvm::function_ref<bool (ConstAtom *, ConstAtom *)>) const;

  virtual ~PointerVariableConstraint() {};
};

typedef PointerVariableConstraint PVConstraint;

// Constraints on a function type. Also contains a 'name' parameter for
// when a re-write of a function pointer is needed.
class FunctionVariableConstraint : public ConstraintVariable {
private:
  // N constraints on the return value of the function.
  std::set<ConstraintVariable*> returnVars;
  // A vector of K sets of N constraints on the parameter values, for
  // K parameters accepted by the function.
  std::vector<std::set<ConstraintVariable*>> paramVars;
  // Name of the function or function variable. Used by mkString.
  std::string name;
  bool hasproto;
  bool hasbody;
public:
  FunctionVariableConstraint() :
          ConstraintVariable(FunctionVariable, "", ""),name(""),hasproto(false),hasbody(false) { }

  FunctionVariableConstraint(clang::DeclaratorDecl *D, ConstraintKey &K,
                             Constraints &CS, const clang::ASTContext &C);
  FunctionVariableConstraint(const clang::Type *Ty, ConstraintKey &K,
                             clang::DeclaratorDecl *D, std::string N, Constraints &CS, const clang::ASTContext &C);

  std::set<ConstraintVariable*> &
  getReturnVars() { return returnVars; }

  size_t numParams() { return paramVars.size(); }
  std::string getName() { return name; }

  bool hasProtoType() { return hasproto; }
  bool hasBody() { return hasbody; }

  static bool classof(const ConstraintVariable *S) {
    return S->getKind() == FunctionVariable;
  }

  std::set<ConstraintVariable*> &
  getParamVar(unsigned i) {
    assert(i < paramVars.size());
    return paramVars.at(i);
  }

  std::string mkString(Constraints::EnvironmentMap &E, bool emitName=true, bool forItype=false);
  void print(llvm::raw_ostream &O) const;
  void dump() const { print(llvm::errs()); }
  void constrainTo(Constraints &CS, ConstAtom *C, bool checkSkip=false);
  bool anyChanges(Constraints::EnvironmentMap &E);
  bool hasWild(Constraints::EnvironmentMap &E);
  bool hasArr(Constraints::EnvironmentMap &E);

  bool isLt(const ConstraintVariable &other, ProgramInfo &P) const;
  bool isEq(const ConstraintVariable &other, ProgramInfo &P) const;
  // An FVConstraint is empty if every constraint associated is empty.
  bool isEmpty(void) const {

    if (returnVars.size() > 0)
      return false;

    for (const auto &u : paramVars)
      for (const auto &v : u)
        if (!v->isEmpty())
          return false;

    return true;
  }

  bool liftedOnCVars(const ConstraintVariable &O,
                     ProgramInfo &Info,
                     llvm::function_ref<bool (ConstAtom *, ConstAtom *)>) const;

  virtual ~FunctionVariableConstraint() {};
};

typedef FunctionVariableConstraint FVConstraint;

#endif //_CONSTRAINTVARIABLES_H
