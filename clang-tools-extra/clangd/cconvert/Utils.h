//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Type declarations for map data structures.
//===----------------------------------------------------------------------===//
#ifndef _UTILS_H
#define _UTILS_H
#include <set>
#include <ctime>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include "llvm/Support/CommandLine.h"
#include "clang/AST/Type.h"

#include "PersistentSourceLoc.h"

class ConstraintVariable;
class ProgramInfo;

// Maps a Decl to the set of constraint variables for that Decl.
typedef std::map<PersistentSourceLoc, 
  std::set<ConstraintVariable*>> VariableMap;

// Maps a Decl to the DeclStmt that defines the Decl.
typedef std::map<clang::Decl*, clang::DeclStmt*> VariableDecltoStmtMap;
#ifdef CCCONVSTANDALONE

extern llvm::cl::opt<bool> Verbose;
extern llvm::cl::opt<bool> DumpIntermediate;
extern llvm::cl::opt<bool> handleVARARGS;
extern llvm::cl::opt<bool> mergeMultipleFuncDecls;
extern llvm::cl::opt<bool> enablePropThruIType;
extern llvm::cl::opt<bool> considerAllocUnsafe;
extern llvm::cl::opt<std::string> BaseDir;
extern llvm::cl::opt<bool> allTypes;
#else

extern bool Verbose;
extern bool DumpIntermediate;
extern bool handleVARARGS;
extern bool mergeMultipleFuncDecls;
extern bool enablePropThruIType;
extern bool considerAllocUnsafe;
extern bool allTypes;
extern std::string BaseDir;
#endif


extern std::set<std::string> inputFilePaths;

const clang::Type *getNextTy(const clang::Type *Ty);

ConstraintVariable *getHighest(std::set<ConstraintVariable*> Vs, ProgramInfo &Info);

clang::FunctionDecl *getDeclaration(clang::FunctionDecl *FD);

clang::FunctionDecl *getDefinition(clang::FunctionDecl *FD);

clang::CheckedPointerKind getCheckedPointerKind(clang::InteropTypeExpr *itypeExpr);

bool hasFunctionBody(clang::Decl *param);

std::string getStorageQualifierString(clang::Decl *D);

bool getAbsoluteFilePath(std::string fileName, std::string &absoluteFP);

bool isNULLExpression(clang::Expr *expr, clang::ASTContext &Ctx);

// get the time spent in seconds since the provided time stamp.
float getTimeSpentInSeconds(clock_t startTime);

// check if the function has varargs i.e., foo(<named_arg>,...)
bool functionHasVarArgs(clang::FunctionDecl *FD);

// check if the function is a allocator.
bool isFunctionAllocator(std::string funcName);

// Is the given variable built  in type?
bool isPointerType(clang::VarDecl *VD);

// check if the variable is of a structure or union type.
bool isStructOrUnionType(clang::VarDecl *VD);

// Helper method to print a Type in a way that can be represented in the source.
std::string tyToStr(const clang::Type *T);

// get the end source location of the end of the provided function.
clang::SourceLocation getFunctionDeclarationEnd(clang::FunctionDecl *FD, clang::SourceManager &S);

// remove auxillary casts from the provided expression.
clang::Expr* removeAuxillaryCasts(clang::Expr *srcExpr);

// check if the provided file path belongs to the input project
// and can be rewritten
bool canWrite(const std::string &filePath);

// Simple Bimap implementation, with only STL dependency
// Inspired from: https://github.com/Mazyod/Bimap/tree/master/Bimap
template <typename KeyType, typename ValueType>
class Bimap {

  typedef std::unordered_set<KeyType> Keys;

  typedef std::unordered_map<KeyType, ValueType> KeyMap;
  typedef std::map<ValueType, Keys> ValueMap;

  typedef std::pair<ValueType, Keys> ValueMapEntry;

public:
  void set(const KeyType& key, const ValueType& value) {

    _normalMap.insert(std::make_pair(key, value));
    _transposeMap[value].insert(key);
  }

  bool removeKey(const KeyType& key) {

    auto has = hasKey(key);
    if (has) {

      auto &value = valueForKey(key);
      auto &keys = keysForValue(value);
      keys.erase(key);
      if (keys.empty()) {
        _transposeMap.erase(value);
      }

      _normalMap.erase(key);
    }

    return has;
  }

  bool removeValue(const ValueType& value) {

    auto has = hasValue(value);
    if (has) {

      auto &keys = keysForValue(value);
      for (auto item : keys) {
        _normalMap.erase(item);
      }

      _transposeMap.erase(value);
    }

    return has;
  }

  bool hasKey(const KeyType& key) const {
    return _normalMap.find(key) != _normalMap.end();
  }

  bool hasValue(const ValueType& value) const {
    return _transposeMap.find(value) != _transposeMap.end();
  }

  unsigned long size() const {
    return _normalMap.size();
  }

  const ValueMap& valueMap() const {
    return _transposeMap;
  };

  const KeyMap& keyMap() const {
    return _normalMap;
  };

private:

  KeyMap _normalMap;
  ValueMap _transposeMap;
};

#endif
