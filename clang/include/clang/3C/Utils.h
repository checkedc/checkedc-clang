//=--Utils.h------------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Type declarations for map data structures and other general helper methods.
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_3C_UTILS_H
#define LLVM_CLANG_3C_UTILS_H

#include "clang/3C/PersistentSourceLoc.h"
#include "clang/AST/Type.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include <ctime>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

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

  const std::map<KeyT, ValueT> &left() const { return KtoVal; }
  const std::map<ValueT, KeyT> &right() const { return ValToK; }

private:
  std::map<KeyT, ValueT> KtoVal;
  std::map<ValueT, KeyT> ValToK;
};

extern std::set<std::string> FilePaths;

template <typename T> T getOnly(const std::set<T> &SingletonSet) {
  assert(SingletonSet.size() == 1);
  return (*SingletonSet.begin());
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

std::string getStorageQualifierString(clang::Decl *D);

std::error_code tryGetCanonicalFilePath(const std::string &FileName,
                                        std::string &AbsoluteFp);

// This compares entire path components: it's smart enough to know that "foo.c"
// does not start with "foo". It's not smart about anything else, so you should
// probably put both paths through tryGetCanonicalFilePath first.
bool filePathStartsWith(const std::string &Path, const std::string &Prefix);

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

// Is this an array type? Note that this includes pointer types decayed from
// array types (i.e., arrays in function parameters) but does not include
// Checked C checked array pointers (unless they decayed from a checked array).
bool isArrayType(const clang::QualType &QT);

// Is this a type that can go inside an _Nt_array_ptr?
bool isNullableType(const clang::QualType &QT);

// Is this type capable of being an NT Array?
bool canBeNtArray(const clang::QualType &QT);

// Check if provided type is a var arg type?
bool isVarArgType(const std::string &TypeName);

// Check if the variable is of a structure or union type.
bool isStructOrUnionType(clang::DeclaratorDecl *VD);

// Helper method to print a Type in a way that can be represented in the source.
// If Name is given, it is included as the variable name (which otherwise isn't
// trivial to do with function pointers, etc.).
std::string tyToStr(const clang::Type *T, const std::string &Name = "");

// Same as tyToStr with a QualType.
std::string qtyToStr(clang::QualType QT, const std::string &Name = "");

// Get the end source location of the end of the provided function.
clang::SourceLocation getFunctionDeclRParen(clang::FunctionDecl *FD,
                                            clang::SourceManager &S);

clang::SourceLocation locationPrecedingChar(clang::SourceLocation SL,
                                            clang::SourceManager &S, char C);

// Remove auxillary casts from the provided expression.
clang::Expr *removeAuxillaryCasts(clang::Expr *SrcExpr);

// Get normalized expression by removing clang syntactic sugar
// clang::Expr *getNormalizedExpr(clang::Expr *CE);

// OK to cast from Src to Dst?
bool isCastSafe(clang::QualType DstType, clang::QualType SrcType);

// Check if the provided file path belongs to the input project
// and can be rewritten.
//
// For accurate results, the path must be canonical. The file name of a
// PersistentSourceLoc can normally be assumed to be canonical.
bool canWrite(const std::string &FilePath);

// Check if the provided variable has void as one of its type.
bool hasVoidType(clang::ValueDecl *D);
// Check if the provided type has void as one of its type
bool isTypeHasVoid(clang::QualType QT);

// Check if the provided declaration is in system header.
bool isInSysHeader(clang::Decl *D);

std::string getSourceText(const clang::SourceRange &SR,
                          const clang::ASTContext &C);
std::string getSourceText(const clang::CharSourceRange &SR,
                          const clang::ASTContext &C);

// Find the longest common subsequence.
unsigned longestCommonSubsequence(const char *Str1, const char *Str2,
                                  unsigned long Str1Len, unsigned long Str2Len);

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
// The base type is the type after removing all.
clang::TypeLoc getBaseTypeLoc(clang::TypeLoc T);

// Ignore all CheckedC temporary and clang implicit expression on E. This
// combines the behavior of IgnoreExprTmp and IgnoreImplicit.
clang::Expr *ignoreCheckedCImplicit(clang::Expr *E);

// Get a FunctionTypeLoc object from the declaration/type location. This is a
// little complicated due to various clang wrapper types that come from
// parenthesised types and function attributes.
clang::FunctionTypeLoc getFunctionTypeLoc(clang::TypeLoc TLoc);
clang::FunctionTypeLoc getFunctionTypeLoc(clang::DeclaratorDecl *Decl);

bool isKAndRFunctionDecl(clang::FunctionDecl *FD);

void getPrintfStringArgIndices(const clang::CallExpr *CE,
                               const clang::FunctionDecl *Callee,
                               const clang::ASTContext &Context,
                               std::set<unsigned> &StringArgIndices);

// Use instead of Stmt::getID since Stmt::getID fails an assertion on long
// string literals (https://bugs.llvm.org/show_bug.cgi?id=49926).
int64_t getStmtIdWorkaround(const clang::Stmt *St,
                            const clang::ASTContext &Context);

clang::SourceLocation getCheckedCAnnotationsEnd(const clang::Decl *D);


clang::SourceLocation
getLocationAfterToken(clang::SourceLocation SL, const clang::SourceManager &SM,
                      const clang::LangOptions &LO);


// Get the source range for a declaration including Checked C annotations.
// Optionally, any initializer can be excluded from the range in order to avoid
// interfering with other rewrites inside an existing initializer
// (https://github.com/correctcomputation/checkedc-clang/issues/267). If the
// declaration has no initializer, then IncludeInitializer has no effect.
clang::SourceRange getDeclSourceRangeWithAnnotations(const clang::Decl *D,
                                                     bool IncludeInitializer);

// Shortcut for the getCustomDiagID + Report sequence to report a custom
// diagnostic as we currently do in 3C.
//
// Unlike DiagnosticEngine::Report, to make it harder to forget to provide a
// source location when we intend to, we don't provide a version that doesn't
// take a source location; instead, the caller should just pass
// SourceLocation().

template <unsigned N>
inline clang::DiagnosticBuilder reportCustomDiagnostic(
    clang::DiagnosticsEngine &DE,
    clang::DiagnosticsEngine::Level Level,
    const char (&FormatString)[N],
    clang::SourceLocation Loc) {
  return DE.Report(Loc, DE.getCustomDiagID(Level, FormatString));
}

// For whatever reason, Clang provides << equivalents for many other
// DiagnosticBuilder::Add* methods but not this one, and we want it in a few
// places.
inline const clang::DiagnosticBuilder &operator<<(
    const clang::DiagnosticBuilder &DB, clang::NamedDecl *ND) {
  DB.AddTaggedVal(reinterpret_cast<intptr_t>(ND),
                  clang::DiagnosticsEngine::ArgumentKind::ak_nameddecl);
  return DB;
}

// Marker for conditions that we might want to make into non-fatal assertions
// once we have an API design for them
// (https://github.com/correctcomputation/checkedc-clang/issues/745). An inline
// function would work just as well, but macros have an LLVM naming convention
// and syntax highlighting that make call sites easier to read, in Matt's
// opinion.
#define NONFATAL_ASSERT_PLACEHOLDER(_cond) (_cond)

// Variant for conditions that the caller doesn't actually test because no
// separate recovery path is currently implemented. We want to check that the
// condition compiles but not evaluate it at runtime (until non-fatal assertions
// are actually implemented, and then only when assertions are enabled in the
// build configuration), and we don't want "unused code" compiler warnings.
// TODO: Is there a better way to achieve this?
#define NONFATAL_ASSERT_PLACEHOLDER_UNUSED(_cond) ((void)sizeof(_cond))

#endif
