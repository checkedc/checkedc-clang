//=--Utils.h------------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Type declarations for map data structures.
//===----------------------------------------------------------------------===//

#ifndef _UTILS_H
#define _UTILS_H
#include "PersistentSourceLoc.h"
#include "clang/AST/Type.h"
#include "llvm/Support/CommandLine.h"

#include <ctime>
#include <set>

class ConstraintVariable;
class ProgramInfo;

// Maps a Decl to the set of constraint variables for that Decl.
typedef std::map<PersistentSourceLoc, 
  std::set<ConstraintVariable *>> VariableMap;

// Maps a Decl to the DeclStmt that defines the Decl.
typedef std::map<clang::Decl *, clang::DeclStmt *> VariableDecltoStmtMap;

extern llvm::cl::opt<bool> Verbose;
extern llvm::cl::opt<bool> DumpIntermediate;
extern llvm::cl::opt<bool> HandleVARARGS;
extern llvm::cl::opt<bool> MergeMultipleFuncDecls;
extern llvm::cl::opt<bool> EnablePropThruIType;
extern llvm::cl::opt<bool> AllocUnsafe;
extern llvm::cl::opt<bool> AddCheckedRegions;

const clang::Type *getNextTy(const clang::Type *Ty);

ConstraintVariable *getHighest(std::set<ConstraintVariable *> Vs,
                               ProgramInfo &Info);

clang::FunctionDecl *getDeclaration(clang::FunctionDecl *FD);

clang::FunctionDecl *getDefinition(clang::FunctionDecl *FD);

clang::CheckedPointerKind getCheckedPointerKind(clang::InteropTypeExpr
                                                    *ItypeExpr);

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
bool isPointerType(clang::VarDecl *VD);

// Check if provided type is a var arg type?
bool isVarArgType(const std::string &TypeName);

// Check if the variable is of a structure or union type.
bool isStructOrUnionType(clang::VarDecl *VD);

// Helper method to print a Type in a way that can be represented in the source.
std::string tyToStr(const clang::Type *T);

clang::SourceLocation getFunctionDeclarationEnd(clang::FunctionDecl *FD,
                                                clang::SourceManager &S);

clang::Expr *removeAuxillaryCasts(clang::Expr *SrcExpr);

// Find the longest common subsequence.
unsigned longestCommonSubsequence(const char *str1, const char *str2,
                                  unsigned str1Len, unsigned str2Len);

#endif
