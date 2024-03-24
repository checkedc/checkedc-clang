//=--MultiDecls.cpp-----------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/3C/MultiDecls.h"
#include "clang/3C/Utils.h"

MultiDeclMemberDecl *getAsMultiDeclMember(Decl *D) {
  // XXX: Is this the best place for this check?
  if (D->getLocation().isInvalid())
    return nullptr;

  // A FunctionDecl can be part of a multi-decl in C, but 3C currently doesn't
  // handle this
  // (https://github.com/correctcomputation/checkedc-clang/issues/659 part (a)).

  // While K&R parameter declarations can be in multi-decls in the input
  // program, we don't use any of the regular multi-decl infrastructure for
  // them; the rewriter blows them away and generates a prototype.
  if (isa<ParmVarDecl>(D))
    return nullptr;

  if (VarDecl *VD = dyn_cast<VarDecl>(D))
    return VD;
  if (FieldDecl *FD = dyn_cast<FieldDecl>(D))
    return FD;
  if (TypedefDecl *TD = dyn_cast<TypedefDecl>(D))
    return TD;
  return nullptr;
}

QualType getTypeOfMultiDeclMember(MultiDeclMemberDecl *MMD) {
  if (DeclaratorDecl *DD = dyn_cast<DeclaratorDecl>(MMD))
    return DD->getType();
  if (TypedefDecl *TD = dyn_cast<TypedefDecl>(MMD))
    return TD->getUnderlyingType();
  llvm_unreachable("Unexpected declaration type");
}

TypeSourceInfo *getTypeSourceInfoOfMultiDeclMember(MultiDeclMemberDecl *MMD) {
  if (DeclaratorDecl *DD = dyn_cast<DeclaratorDecl>(MMD))
    return DD->getTypeSourceInfo();
  if (TypedefDecl *TD = dyn_cast<TypedefDecl>(MMD))
    return TD->getTypeSourceInfo();
  llvm_unreachable("Unexpected declaration type");
}

void ProgramMultiDeclsInfo::findUsedTagNames(DeclContext *DC) {
  // We do our own traversal via `decls` rather than using RecursiveASTVisitor.
  // This has the advantage of visiting TagDecls in function parameters, which
  // RecursiveASTVisitor doesn't do by default, though such TagDecls are
  // potentially problematic for 3C anyway.
  for (Decl *D : DC->decls()) {
    if (TagDecl *TD = dyn_cast<TagDecl>(D)) {
      if (!TD->getName().empty()) {
        // Multiple TagDecls may have the same name if the same physical
        // declaration is seen in multiple translation units or different
        // TagDecls with the same name are used in different scopes. That is not
        // a problem for us here: we're simply making a list of all the names we
        // don't want to collide with.
        UsedTagNames.insert(std::string(TD->getName()));
      }
    }
    if (DeclContext *NestedDC = dyn_cast<DeclContext>(D)) {
      findUsedTagNames(NestedDC);
    }
  }
}

void ProgramMultiDeclsInfo::findUsedTagNames(ASTContext &Context) {
  findUsedTagNames(Context.getTranslationUnitDecl());
}

static const Type *unelaborateType(const Type *Ty) {
  if (const ElaboratedType *ETy = dyn_cast<ElaboratedType>(Ty)) {
    QualType QT = ETy->getNamedType();
    Ty = QT.getTypePtr();
    // Can an ElaboratedType add qualifiers to its underlying type in C? I don't
    // think so, but if it does, we don't want to silently lose them.
    NONFATAL_ASSERT_PLACEHOLDER_UNUSED(QualType(Ty, 0) == QT);
  }
  return Ty;
}

void ProgramMultiDeclsInfo::findMultiDecls(DeclContext *DC,
                                           ASTContext &Context) {
  // This will automatically create a new, empty map for the TU if needed.
  TUMultiDeclsInfo &TUInfo = TUInfos[&Context];
  TagDecl *LastTagDef = nullptr;

  // Variables related to the current multi-decl.
  MultiDeclInfo *CurrentMultiDecl = nullptr;
  SourceLocation CurrentBeginLoc;
  PersistentSourceLoc TagDefPSL;
  bool TagDefNeedsName;
  llvm::Optional<RenamedTagDefInfo> RenameInfo;
  bool AppliedRenameInfo;

  for (Decl *D : DC->decls()) {
    TagDecl *TagD = dyn_cast<TagDecl>(D);
    if (TagD && TagD->isCompleteDefinition() &&
        // With -fms-extensions (default on Windows), Clang injects an implicit
        // `struct _GUID` with an invalid location.
        TagD->getBeginLoc().isValid()) {
      LastTagDef = TagD;
    }
    if (MultiDeclMemberDecl *MMD = getAsMultiDeclMember(D)) {
      if (CurrentMultiDecl == nullptr ||
          MMD->getBeginLoc() != CurrentBeginLoc) {
        // We are starting a new multi-decl.
        CurrentBeginLoc = MMD->getBeginLoc();
        CurrentMultiDecl = &TUInfo.MultiDeclsByBeginLoc[CurrentBeginLoc];
        assert(CurrentMultiDecl->Members.empty() &&
               "Multi-decl members are not consecutive in traversal order");
        TagDefNeedsName = false;
        RenameInfo = llvm::None;
        AppliedRenameInfo = false;

        // Check for an inline tag definition.
        // Wanted: CurrentBeginLoc <= LastTagDef->getBeginLoc().
        // Implemented as: !(LastTagDef->getBeginLoc() < CurrentBeginLoc).
        if (LastTagDef != nullptr &&
            !Context.getSourceManager().isBeforeInTranslationUnit(
                LastTagDef->getBeginLoc(), CurrentBeginLoc)) {
          CurrentMultiDecl->TagDefToSplit = LastTagDef;
          TUInfo.ContainingMultiDeclOfTagDecl[LastTagDef] = CurrentMultiDecl;

          // Do we need to automatically name the TagDefToSplit?
          if (LastTagDef->getName().empty()) {
            // A RecordDecl that is declared as the type of one or more
            // variables shouldn't be "anonymous", but if it somehow is, we
            // don't want to try to give it a name.
            NONFATAL_ASSERT_PLACEHOLDER_UNUSED(
                !(isa<RecordDecl>(LastTagDef) &&
                  cast<RecordDecl>(LastTagDef)->isAnonymousStructOrUnion()));
            TagDefPSL = PersistentSourceLoc::mkPSL(LastTagDef, Context);
            auto Iter = RenamedTagDefs.find(TagDefPSL);
            if (Iter != RenamedTagDefs.end())
              RenameInfo = Iter->second;
            else if (canWrite(TagDefPSL.getFileName()))
              TagDefNeedsName = true;
          }
        }
      } else {
        // Adding another member to an existing multi-decl.
        assert(Context.getSourceManager().isBeforeInTranslationUnit(
                   CurrentMultiDecl->Members.back()->getEndLoc(),
                   MMD->getEndLoc()) &&
               "Multi-decl traversal order inconsistent "
               "with source location order");
      }

      CurrentMultiDecl->Members.push_back(MMD);

      std::string MemberName;
      if (TagDefNeedsName &&
          NONFATAL_ASSERT_PLACEHOLDER(
              !(MemberName = std::string(MMD->getName())).empty())) {
        // Special case: If the first member of the multi-decl is a typedef
        // whose type is exactly the TagDecl type (`typedef struct { ... } T`),
        // then we refer to the TagDecl via that typedef. (The typedef must be
        // the first member so that it is defined in time for other members to
        // refer to it.)
        //
        // An argument could be made for using the typedef name in the types of
        // other multi-decl members even if the TagDecl has a name:
        // `typedef struct T_struct { ... } T, *PT;` would convert to
        // `typedef struct T_struct { ... } T; typedef _Ptr<T> PT;` instead of
        // `struct T_struct { ... }; typedef struct T_struct T;
        // typedef _Ptr<struct T_struct> PT;`. But it would be tricky to ensure
        // that any existing references to `struct T_struct` aren't accidentally
        // replaced with `T`, so absent a decision that this feature is
        // important enough to justify either solving or ignoring this problem,
        // we don't try to implement the feature.
        TypedefDecl *TyD;
        QualType Underlying;
        if (CurrentMultiDecl->Members.size() == 1 &&
            (TyD = dyn_cast<TypedefDecl>(MMD)) != nullptr &&
            // XXX: This is a terrible mess. Figure out how we should be
            // handling the difference between Type and QualType.
            !(Underlying = TyD->getUnderlyingType()).hasLocalQualifiers() &&
            QualType(unelaborateType(Underlying.getTypePtr()), 0) ==
                Context.getTagDeclType(LastTagDef)) {
          // In this case, ShouldSplit = false: the tag definition should not be
          // moved out of the typedef.
          RenameInfo = RenamedTagDefInfo{MemberName, false};
        } else {
          // Otherwise, just generate a new tag name based on the member name.
          // Example: `struct { ... } foo;` ->
          // `struct foo_struct_1 { ... }; struct foo_struct_1 foo;`
          // If `foo_struct_1` is already taken, use `foo_struct_2`, etc.
          std::string KindName = std::string(LastTagDef->getKindName());
          std::string NewName;
          for (int Num = 1;; Num++) {
            NewName = MemberName + "_" + KindName + "_" + std::to_string(Num);
            if (UsedTagNames.find(NewName) == UsedTagNames.end())
              break;
          }
          RenameInfo = RenamedTagDefInfo{KindName + " " + NewName, true};
          // Consider this name taken and ensure that other automatically
          // generated names do not collide with it.
          //
          // If the multi-decl doesn't end up getting rewritten, this name
          // ultimately may not be used, creating a gap in the numbering in 3C's
          // output. But this cosmetic inconsistency is a small price to pay for
          // the architectural convenience of being able to store the assigned
          // names in the PointerVariableConstraints when they are constructed
          // rather than trying to assign and store the names after we know
          // which multi-decls will be rewritten.
          UsedTagNames.insert(NewName);
        }
        RenamedTagDefs.insert(std::make_pair(TagDefPSL, *RenameInfo));
        TagDefNeedsName = false;
      }

      // To help avoid bugs, use the same code whether the RenameInfo was just
      // assigned or was saved from a previous translation unit.
      if (RenameInfo && !AppliedRenameInfo) {
        CurrentMultiDecl->BaseTypeRenamed = true;
        if (!RenameInfo->ShouldSplit)
          CurrentMultiDecl->TagDefToSplit = nullptr;
        AppliedRenameInfo = true;
      }
    }

    if (DeclContext *NestedDC = dyn_cast<DeclContext>(D)) {
      findMultiDecls(NestedDC, Context);
    }
  }
}

void ProgramMultiDeclsInfo::findMultiDecls(ASTContext &Context) {
  findMultiDecls(Context.getTranslationUnitDecl(), Context);
}

llvm::Optional<std::string>
ProgramMultiDeclsInfo::getTypeStrOverride(const Type *Ty, const ASTContext &C) {
  Ty = unelaborateType(Ty);
  if (const TagType *TTy = dyn_cast<TagType>(Ty)) {
    TagDecl *TD = TTy->getDecl();
    if (TD->getName().empty()) {
      PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(TD, C);
      auto Iter = RenamedTagDefs.find(PSL);
      if (Iter != RenamedTagDefs.end())
        return Iter->second.AssignedTypeStr;
      // We should have named all unnamed TagDecls in writable code.
      NONFATAL_ASSERT_PLACEHOLDER_UNUSED(!canWrite(PSL.getFileName()));
    }
  }
  return llvm::None;
}

MultiDeclInfo *
ProgramMultiDeclsInfo::findContainingMultiDecl(MultiDeclMemberDecl *MMD) {
  auto &MultiDeclsByLoc = TUInfos[&MMD->getASTContext()].MultiDeclsByBeginLoc;
  // Look for a MultiDeclInfo for the beginning location of MMD, then check that
  // the MultiDeclInfo actually contains MMD.
  auto It = MultiDeclsByLoc.find(MMD->getBeginLoc());
  if (It == MultiDeclsByLoc.end())
    return nullptr;
  MultiDeclInfo &MDI = It->second;
  // Hope we don't have multi-decls with so many members that this becomes a
  // performance problem.
  if (std::find(MDI.Members.begin(), MDI.Members.end(), MMD) !=
      MDI.Members.end())
    return &MDI;
  return nullptr;
}

MultiDeclInfo *ProgramMultiDeclsInfo::findContainingMultiDecl(TagDecl *TD) {
  auto &MDOfTD = TUInfos[&TD->getASTContext()].ContainingMultiDeclOfTagDecl;
  auto It = MDOfTD.find(TD);
  if (It == MDOfTD.end())
    return nullptr;
  return It->second;
}

bool ProgramMultiDeclsInfo::wasBaseTypeRenamed(Decl *D) {
  // We assume that the base type was renamed if and only if D belongs to a
  // multi-decl marked as having the base type renamed. It might be better to
  // actually extract the base type from D and look it up in RenamedTagDefs,
  // but that's more work.
  MultiDeclMemberDecl *MMD = getAsMultiDeclMember(D);
  if (!MMD)
    return false;
  MultiDeclInfo *MDI = findContainingMultiDecl(MMD);
  // We expect to have a MultiDeclInfo for every MultiDeclMemberDecl in the
  // program.
  if (!NONFATAL_ASSERT_PLACEHOLDER(MDI))
    return false;
  return MDI->BaseTypeRenamed;
}
