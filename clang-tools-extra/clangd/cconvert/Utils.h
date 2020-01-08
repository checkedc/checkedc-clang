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

extern bool Verbose;
extern bool DumpIntermediate;
extern bool handleVARARGS;
extern bool mergeMultipleFuncDecls;
extern bool enablePropThruIType;
extern bool considerAllocUnsafe;
extern std::set<std::string> inputFilePaths;
extern std::string BaseDir;

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

#endif
