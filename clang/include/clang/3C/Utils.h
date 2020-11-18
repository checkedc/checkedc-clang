//=--Utils.h------------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Type declarations for map data structures and other general helper methods.
//===----------------------------------------------------------------------===//

#ifndef _UTILS_H
#define _UTILS_H
#include "clang/AST/Type.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include <ctime>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "PersistentSourceLoc.h"

class ConstraintVariable;
class ProgramInfo;

// Maps a Decl to the set of constraint variables for that Decl.
typedef std::map<PersistentSourceLoc, ConstraintVariable *> VariableMap;

// Maps a Decl to the DeclStmt that defines the Decl.
typedef std::map<clang::Decl *, clang::DeclStmt *> VariableDecltoStmtMap;

template <typename ValueT> class Option {
public:
  Option() : Value(nullptr), HasValue(false) {}
  Option(ValueT &V) : Value(&V), HasValue(true) {}

  ValueT &getValue() const {
    assert("Inconsistent option!" && HasValue && Value != nullptr);
    return *Value;
  }

  bool hasValue() const {
    assert("Inconsistent option!" && HasValue == (Value != nullptr));
    return HasValue;
  }

private:
  ValueT *Value;
  bool HasValue;
};

// Replacement for boost:bimap. A wrapper class around two std::maps to enable
// map lookup from key to value or from value to key.
template <typename KeyT, typename ValueT> class BiMap {
public:
  BiMap() = default;
  ~BiMap() { clear(); }

  void insert(KeyT KO, ValueT VO) {
    KtoVal[KO] = VO;
    ValToK[VO] = KO;
  }

  void clear() {
    KtoVal.clear();
    ValToK.clear();
  }

  const std::map<KeyT, ValueT> &left() { return KtoVal; }
  const std::map<ValueT, KeyT> &right() { return ValToK; }

private:
  std::map<KeyT, ValueT> KtoVal;
  std::map<ValueT, KeyT> ValToK;
};

extern std::set<std::string> FilePaths;

template <typename T> T getOnly(const std::set<T> &singletonSet) {
  assert(singletonSet.size() == 1);
  return (*singletonSet.begin());
}

template <typename T>
void findIntersection(const std::set<T> &Set1, const std::set<T> &Set2,
                      std::set<T> &Out) {
  Out.clear();
  std::set_intersection(Set1.begin(), Set1.end(), Set2.begin(), Set2.end(),
                        std::inserter(Out, Out.begin()));
}

const clang::Type *getNextTy(const clang::Type *Ty);

clang::FunctionDecl *getDeclaration(clang::FunctionDecl *FD);

clang::FunctionDecl *getDefinition(clang::FunctionDecl *FD);

clang::CheckedPointerKind
getCheckedPointerKind(clang::InteropTypeExpr *ItypeExpr);

bool hasFunctionBody(clang::Decl *D);

std::string getStorageQualifierString(clang::Decl *D);

bool getAbsoluteFilePath(std::string FileName, std::string &AbsoluteFp);

bool isNULLExpression(clang::Expr *E, clang::ASTContext &C);

// Get the time spent in seconds since the provided time stamp.
float getTimeSpentInSeconds(clock_t StartTime);

// Check if the function has varargs i.e., foo(<named_arg>,...)
bool functionHasVarArgs(clang::FunctionDecl *FD);

// Check if the function is a allocator.
bool isFunctionAllocator(std::string FuncName);

// Is the given variable built  in type?
bool isPointerType(clang::ValueDecl *VD);

// Is this a pointer or array type?
bool isPtrOrArrayType(const clang::QualType &QT);

// Check if provided type is a var arg type?
bool isVarArgType(const std::string &TypeName);

// Check if the variable is of a structure or union type.
bool isStructOrUnionType(clang::VarDecl *VD);

// Helper method to print a Type in a way that can be represented in the source.
std::string tyToStr(const clang::Type *T);

// Get the end source location of the end of the provided function.
clang::SourceLocation getFunctionDeclarationEnd(clang::FunctionDecl *FD,
                                                clang::SourceManager &S);

// Remove auxillary casts from the provided expression.
clang::Expr *removeAuxillaryCasts(clang::Expr *SrcExpr);

// Get normalized expression by removing clang syntactic sugar
// clang::Expr *getNormalizedExpr(clang::Expr *CE);

// OK to cast from Src to Dst?
bool isCastSafe(clang::QualType DstType, clang::QualType SrcType);

// Check if the provided file path belongs to the input project
// and can be rewritten.
bool canWrite(const std::string &FilePath);

// Check if the provided variable has void as one of its type.
bool hasVoidType(clang::ValueDecl *D);
// Check if the provided type has void as one of its type
bool isTypeHasVoid(clang::QualType QT);

// Check if the provided declaration is in system header.
bool isInSysHeader(clang::Decl *D);

std::string getSourceText(const clang::SourceRange &SR,
                          const clang::ASTContext &C);

// Find the longest common subsequence.
unsigned longestCommonSubsequence(const char *Str1, const char *Str2,
                                  unsigned long Str1Len, unsigned long Str2Len);

const clang::TypeVariableType *getTypeVariableType(clang::DeclaratorDecl *Decl);

bool isTypeAnonymous(const clang::Type *T);

// Find the index of parameter PV in the parameter list of function FD.
unsigned int getParameterIndex(clang::ParmVarDecl *PV, clang::FunctionDecl *FD);

// If E can be evaluated to a constant integer, the result is stored in Result,
// and true is returned. Otherwise, Result is not modified and, false is
// returned.
bool evaluateToInt(clang::Expr *E, const clang::ASTContext &C, int &Result);

// Check if the bounds expression BE is zero width. Arrays with zero width
// bounds can be treated as pointers.
bool isZeroBoundsExpr(clang::BoundsExpr *BE, const clang::ASTContext &C);

// Find the range in the source code for the base type of a type location.
// The base type is the type after removing all
clang::TypeLoc getBaseTypeLoc(clang::TypeLoc T);
#endif
