//=--MultiDecls.h-------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Code to deal with "multi-decls": constructs in which one or more identifiers
// are declared in a comma-separated list based on a single type "on the left".
// A simple example:
//
// struct my_struct x, *p;
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_3C_MULTIDECLS_H
#define LLVM_CLANG_3C_MULTIDECLS_H

#include "clang/3C/PersistentSourceLoc.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Type.h"
#include "llvm/ADT/Optional.h"

using namespace clang;

// Some more information about multi-decls in the context of 3C:
//
// The "members" of a given multi-decl may be ordinary variables (VarDecls),
// struct/union fields (FieldDecls), or typedefs (TypedefDecls), but all members
// of a given multi-decl are of the same kind.
//
// If the "left type" of a multi-decl is a TagDecl, it may have an inline
// definition; if it does, then the TagDecl may be unnamed. Examples:
//
// struct my_struct { int *y; } x, *p;
// struct { int *y; } x, *p;
//
// Multi-decls (especially those with inline TagDecls) have historically been
// tricky for 3C to rewrite. If the type of one member becomes a _Ptr (or
// similar), then the left type of the members is no longer the same, so the
// multi-decl must be broken up, for example:
//
// struct my_struct x;
// _Ptr<struct my_struct> p;
//
// To keep the logic simpler, if 3C needs to change the type of at least one
// member of a multi-decl, it breaks up all members of the multi-decl into
// separate declarations. To preserve SourceLocations as much as possible and
// avoid interfering with rewrites to any other constructs in the multi-decl
// (e.g., within existing initializer expressions), this breakup is performed by
// replacing the commas with semicolons in place and inserting additional
// occurrences of the left type and any common qualifiers as needed.
//
// If there is an inline TagDecl, it is separated too and moved out of any
// containing RecordDecl to avoid a compiler warning, and if the TagDecl is
// unnamed, it is given an automatically generated name so that it can be
// referenced by the new, separate declarations of the multi-decl members.
// Example:
//
// static struct { int *y; } x, *p:
//
// ->
//
// struct x_struct_1 { _Ptr<int> y; };
// static struct x_struct_1 x;
// static _Ptr<struct x_struct_1> p;
//
// Exception: In a typedef multi-decl, if the _first_ member refers to the
// TagDecl itself (not a pointer to it, etc.), then 3C uses that name for the
// TagDecl rather than generating a new one. This produces nicer output for the
// idiom:
//
// typedef struct { int *y; } FOO, *PFOO;
//
// ->
//
// typedef struct { _Ptr<int> y; } FOO;
// typedef _Ptr<FOO> PFOO;
//
// The multi-decl code is used even for "multi-decls" of VarDecls, FieldDecls,
// or TypedefDecls that have only a single member to avoid having to maintain a
// separate code path for them. But a multi-decl always has at least one member;
// a pure TagDecl such as `struct my_struct { int *y; };` is _not_ considered a
// multi-decl. ParmVarDecls are handled differently. In fact, ParmVarDecls with
// inline TagDecls are known to be handled poorly, but that's a rare and poor
// practice and it's not easy to handle them better.

// Currently, we automatically generate a name for every unnamed TagDecl defined
// in a multi-decl and use the name in ConstraintVariables, but we only insert
// the name into the definition if the multi-decl gets rewritten for some other
// reason. This solves the common case of allowing the types of all the
// multi-decl members to refer to the TagDecl, but it doesn't address cases in
// which 3C might need to insert a reference to the unnamed TagDecl elsewhere
// even if the multi-decl isn't being rewritten. In these cases, 3C typically
// uses the generated name even though it is not defined, causing a compile
// error that the user has to correct manually. The problematic cases include:
//
// - Type argument insertion. TypeVariableEntry has a check for
//   `isTypeAnonymous`, but it has at least one bug (it misses double pointers).
//
// - Cast insertion, potentially. I was unable to find an example, but that
//   doesn't mean it will never happen, especially with future changes to the
//   code.
//
// - Typedef itype insertion.
//
// One approach to try to rule out all of these bugs at once would be to
// preemptively rewrite all multi-decls containing unnamed TagDecls, but those
// changes might be undesirable or could even cause errors in the presence of
// macros, etc. Or we could try to add the necessary code so that insertion of a
// reference to an unnamed TagDecl would trigger insertion of the name into the
// definition. For now, we don't deal with the problem.

// Implementation note: The Clang AST does not represent multi-decls explicitly
// (except in functions, where they are represented by DeclStmts). In other
// contexts, we detect them based on the property that the beginning
// SourceLocation of all the members is the same. And as long as we are making
// this assumption, we use it in functions too rather than having a separate
// code path that looks for DeclStmts.

// NamedDecl is the nearest common superclass of all Decl subtypes that can be
// multi-decl members. There is no enforcement that a MultiDeclMemberDecl is
// actually one of the allowed subtypes, so use of the MultiDeclMemberDecl
// typedef serves as documentation only. (If we wanted to enforce it, we'd need
// a wrapper object of some kind, which currently seems to be more trouble than
// it's worth.)
typedef NamedDecl MultiDeclMemberDecl;

// Returns D if it can be a multi-decl member, otherwise null.
MultiDeclMemberDecl *getAsMultiDeclMember(Decl *D);

// Helpers to cope with the different APIs to do corresponding things with a
// TypedefDecl or DeclaratorDecl.
QualType getTypeOfMultiDeclMember(MultiDeclMemberDecl *MMD);
TypeSourceInfo *getTypeSourceInfoOfMultiDeclMember(MultiDeclMemberDecl *MMD);

struct MultiDeclInfo {
  // The TagDecl that is defined inline in the multi-decl and needs to be split
  // from it during rewriting, if any, otherwise null. In a case like
  // `typedef struct { ... } T`, there is an inline tag definition but we don't
  // need to split it out, so this will be null.
  TagDecl *TagDefToSplit = nullptr;

  // True if the base type was an unnamed TagDecl defined inline for which we
  // are using a new name. Note that TagDefToSplit can be nonnull and
  // BaseTypeRenamed can be false if the inline TagDecl was named, and the
  // reverse can occur in the `typedef struct { ... } T` case.
  bool BaseTypeRenamed = false;

  // The members of the multi-decl in their original order.
  std::vector<MultiDeclMemberDecl *> Members;

  // Set by DeclRewriter::rewriteMultiDecl after it rewrites the entire
  // multi-decl to ensure that it doesn't try to do so more than once if
  // multiple members needed changes.
  bool AlreadyRewritten = false;
};

struct TUMultiDeclsInfo {
  // All multi-decls, keyed by the common beginning source location of their
  // members. Note that the beginning source location of TagDefToSplit may be
  // later if there is a keyword such as `static` or `typedef` in between.
  std::map<SourceLocation, MultiDeclInfo> MultiDeclsByBeginLoc;

  // Map from a tag definition to its containing multi-decl (if it is part of
  // one). Note that the TagDefToSplit of the MultiDeclInfo is not guaranteed to
  // equal the TagDecl: it may be null in the `typedef struct { ... } T` case.
  //
  // Note that the MultiDeclInfo pointers remain valid for as long as the
  // MultiDeclInfo objects remain in MultiDeclsByBeginLoc: see
  // https://en.cppreference.com/w/cpp/container#Iterator_invalidation.
  std::map<TagDecl *, MultiDeclInfo *> ContainingMultiDeclOfTagDecl;
};

class ProgramMultiDeclsInfo {
private:
  // Set of TagDecl names already used at least once in the program, so we can
  // avoid colliding with them.
  std::set<std::string> UsedTagNames;

  // Information about an originally unnamed tag definition in a multi-decl for
  // which we're using a new name.
  struct RenamedTagDefInfo {
    // The new string that should be used to refer to the type of the TagDecl.
    // Unlike UsedTagNames, this includes the tag kind keyword (such as
    // `struct`), except when we use an existing typedef (which doesn't require
    // a tag keyword).
    std::string AssignedTypeStr;
    // Whether the TagDecl should be split from the multi-decl. True except when
    // we use an existing typedef.
    bool ShouldSplit;
  };

  // Map from PSL of a TagDecl to its RenamedTagDefInfo, to ensure that we
  // handle the TagDecl consistently when 3C naively rewrites the same header
  // file multiple times as part of different translation units (see
  // https://github.com/correctcomputation/checkedc-clang/issues/374#issuecomment-804283984).
  std::map<PersistentSourceLoc, RenamedTagDefInfo> RenamedTagDefs;

  std::map<ASTContext *, TUMultiDeclsInfo> TUInfos;

  // Recursive helpers.
  void findUsedTagNames(DeclContext *DC);
  void findMultiDecls(DeclContext *DC, ASTContext &Context);

public:
  void findUsedTagNames(ASTContext &Context);
  void findMultiDecls(ASTContext &Context);
  llvm::Optional<std::string> getTypeStrOverride(const Type *Ty,
                                                 const ASTContext &C);
  MultiDeclInfo *findContainingMultiDecl(MultiDeclMemberDecl *MMD);
  MultiDeclInfo *findContainingMultiDecl(TagDecl *TD);
  bool wasBaseTypeRenamed(Decl *D);
  void clear();
};

#endif // LLVM_CLANG_3C_MULTIDECLS_H
