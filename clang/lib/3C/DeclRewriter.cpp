//=--DeclRewriter.cpp---------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/3C/DeclRewriter.h"
#include "clang/3C/3CGlobalOptions.h"
#include "clang/3C/MappingVisitor.h"
#include "clang/3C/RewriteUtils.h"
#include "clang/3C/StructInit.h"
#include "clang/3C/Utils.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <sstream>

#ifdef FIVE_C
#include "clang/3C/DeclRewriter_5C.h"
#endif

using namespace llvm;
using namespace clang;

static bool checkNeedsFreshLowerBound(PVConstraint *Defn, std::string UseName,
                                      ProgramInfo &Info,
                                      std::string &DeclName) {
  bool NeedsFreshLowerBound = Info.getABoundsInfo().needsFreshLowerBound(Defn);

  if (NeedsFreshLowerBound) {
    BoundsKey FreshLB = Info.getABoundsInfo()
                            .getBounds(Defn->getBoundsKey())
                            ->getLowerBoundKey();
    DeclName = Info.getABoundsInfo().getProgramVar(FreshLB)->getVarName();
  } else {
    DeclName = UseName;
  }

  return NeedsFreshLowerBound;
}

static std::string buildSupplementaryDecl(PVConstraint *Defn,
                                          DeclaratorDecl *Decl,
                                          ArrayBoundsRewriter &ABR,
                                          ProgramInfo &Info, bool SDeclChecked,
                                          std::string DeclName) {
  std::string SDecl = Defn->mkString(
      Info.getConstraints(), MKSTRING_OPTS(ForItypeBase = !SDeclChecked));
  if (SDeclChecked)
    SDecl += ABR.getBoundsString(Defn, Decl);
  SDecl += " = " + DeclName + ";";
  return SDecl;
}

// Generate a new declaration for the PVConstraint using an itype where the
// unchecked portion of the type is the original type, and the checked portion
// is the taken from the constraint graph solution.
//
// If SDeclChecked = true, then any generated supplementary declaration uses the
// checked type; that's generally what you want if an itype is being used only
// because of -itypes-for-extern. If SDeclChecked = false, then the unchecked
// type is used; that's what you want when the supplementary declaration is
// standing in for a function parameter that got an itype because it is used
// unsafely inside the function. TODO: Instead of using an ad-hoc boolean
// parameter for this, maybe we could just pass in the internal PVConstraint and
// look at that
// (https://github.com/correctcomputation/checkedc-clang/issues/704).
RewrittenDecl
DeclRewriter::buildItypeDecl(PVConstraint *Defn, DeclaratorDecl *Decl,
                             std::string UseName, ProgramInfo &Info,
                             ArrayBoundsRewriter &ABR, bool GenerateSDecls,
                             bool SDeclChecked) {
  std::string DeclName;
  bool NeedsFreshLowerBound =
      checkNeedsFreshLowerBound(Defn, UseName, Info, DeclName);

  const EnvironmentMap &Env = Info.getConstraints().getVariables();
  // True when the type of this variable is defined by a typedef, and the
  // constraint variable representing the typedef solved to an unchecked type.
  // In these cases, the typedef should be used in the unchecked part of the
  // itype. The typedef is expanded using checked pointer types for the checked
  // portion. In ItypesForExtern mode, typedefs are treated as unchecked because
  // 3C will not rewrite the typedef to a checked type. Even if it solves to a
  // checked type, it is not rewritten, so it remains unchecked in the converted
  // code.
  bool IsUncheckedTypedef =
    Defn->isTypedef() && (_3COpts.ItypesForExtern ||
                          !Defn->getTypedefVar()->isSolutionChecked(Env));
  // True if this variable is defined by a typedef, and the constraint variable
  // representing the typedef solves to a checked type. Notably not the negation
  // of IsUncheckedTypedef because both require the variable type be defined
  // by a typedef. The checked typedef is expanded using unchecked types in the
  // unchecked portion of the itype. The typedef is used directly in the checked
  // portion of the itype. TODO: Maybe we shouldn't do that if the solution for
  // the typedef doesn't fully equal the solution for the variable
  // (https://github.com/correctcomputation/checkedc-clang/issues/705)?
  bool IsCheckedTypedef = Defn->isTypedef() && !IsUncheckedTypedef;

  bool BaseTypeRenamed =
      Decl && Info.TheMultiDeclsInfo.wasBaseTypeRenamed(Decl);

  // It should in principle be possible to always generate the unchecked portion
  // of the itype by going through mkString. However, mkString has bugs that
  // lead to incorrect output in some less common cases
  // (https://github.com/correctcomputation/checkedc-clang/issues/703). So we
  // use the original type string generated by Clang (via qtyToStr or
  // getOriginalTypeWithName) unless we know we have a special requirement that
  // it doesn't meet, in which case we use mkString. Those cases are:
  // - Unmasking a typedef. The expansion of the typedef does not exist in the
  //   original source, so it must be constructed. (TODO: Couldn't we just get
  //   the underlying type with TypedefDecl::getUnderlyingType and then use
  //   qtyToStr?)
  // - A function pointer. For a function pointer with an itype to parse
  //   correctly, it needs an extra set of parentheses (e.g.,
  //   `void ((*f)()) : itype(...)` instead of `void (*f)() : itype(...)`), and
  //   Clang won't know to add them.
  // - When the base type is an unnamed TagDecl that 3C has renamed, Clang won't
  //   know the new name.
  // - Possible future change: if the internal PVConstraint is partially checked
  //   and we want to use it
  //   (https://github.com/correctcomputation/checkedc-clang/issues/704).
  std::string Type;
  if (IsCheckedTypedef || Defn->getFV() || BaseTypeRenamed) {
    Type = Defn->mkString(Info.getConstraints(),
                          MKSTRING_OPTS(UnmaskTypedef = IsCheckedTypedef,
                                        ForItypeBase = true,
                                        UseName = DeclName));
  } else {
    // In the remaining cases, the unchecked portion of the itype is just the
    // original type of the pointer. The first branch tries to generate the type
    // using the type and name for this specific declaration. This is important
    // because it avoids changing parameter names, particularly in cases where
    // multiple functions sharing the same name are defined in different
    // translation units.
    if (isa_and_nonnull<ParmVarDecl>(Decl) && !DeclName.empty())
      Type = qtyToStr(Decl->getType(), DeclName);
    else
      Type = Defn->getOriginalTypeWithName();
  }

  std::string IType = " : itype(" +
    Defn->mkString(Info.getConstraints(),
                   MKSTRING_OPTS(EmitName = false, ForItype = true,
                                 UnmaskTypedef = IsUncheckedTypedef)) + ")";
  IType += ABR.getBoundsString(Defn, Decl, true, NeedsFreshLowerBound);

  std::string SDecl;
  if (GenerateSDecls && NeedsFreshLowerBound)
    SDecl =
        buildSupplementaryDecl(Defn, Decl, ABR, Info, SDeclChecked, DeclName);
  return RewrittenDecl(Type, IType, SDecl);
}

RewrittenDecl
DeclRewriter::buildCheckedDecl(PVConstraint *Defn, DeclaratorDecl *Decl,
                               std::string UseName, ProgramInfo &Info,
                               ArrayBoundsRewriter &ABR, bool GenerateSDecls) {
  std::string DeclName;
  bool NeedsFreshLowerBound =
      checkNeedsFreshLowerBound(Defn, UseName, Info, DeclName);

  std::string Type =
    Defn->mkString(Info.getConstraints(), MKSTRING_OPTS(UseName = DeclName));
  std::string IType =
    ABR.getBoundsString(Defn, Decl, false, NeedsFreshLowerBound);
  std::string SDecl;
  if (GenerateSDecls && NeedsFreshLowerBound)
    SDecl = buildSupplementaryDecl(Defn, Decl, ABR, Info, true, DeclName);
  return RewrittenDecl(Type, IType, SDecl);
}

// This function is the public entry point for declaration rewriting.
void DeclRewriter::rewriteDecls(ASTContext &Context, ProgramInfo &Info,
                                Rewriter &R) {
  // Compute the bounds information for all the array variables.
  ArrayBoundsRewriter ABRewriter(Info);

  // Collect function and record declarations that need to be rewritten in a set
  // as well as their rewriten types in a map.
  RSet RewriteThese;

  FunctionDeclBuilder *TRV = nullptr;
#ifdef FIVE_C
  auto TRV5C = FunctionDeclBuilder5C(&Context, Info, RewriteThese, ABRewriter);
  TRV = &TRV5C;
#else
  auto TRV3C = FunctionDeclBuilder(&Context, Info, RewriteThese, ABRewriter);
  TRV = &TRV3C;
#endif
  StructVariableInitializer SVI =
      StructVariableInitializer(&Context, Info, RewriteThese);
  for (const auto &D : Context.getTranslationUnitDecl()->decls()) {
    TRV->TraverseDecl(D);
    SVI.TraverseDecl(D);
    const auto &TD = dyn_cast<TypedefDecl>(D);
    // Don't convert typedefs when -itype-for-extern is passed. Typedefs will
    // keep their unchecked type but function using the typedef will be given a
    // checked itype.
    if (!_3COpts.ItypesForExtern && TD) {
      auto PSL = PersistentSourceLoc::mkPSL(TD, Context);
      // Don't rewrite base types like int
      if (!TD->getUnderlyingType()->isBuiltinType()) {
        const auto O = Info.lookupTypedef(PSL);
        if (O.hasValue()) {
          const auto &Var = O.getValue();
          const auto &Env = Info.getConstraints().getVariables();
          if (Var.anyChanges(Env)) {
            std::string NewTy =
                getStorageQualifierString(D) +
                Var.mkString(Info.getConstraints(),
                             MKSTRING_OPTS(UnmaskTypedef = true));
            RewriteThese.insert(
                std::make_pair(TD, new MultiDeclMemberReplacement(TD, NewTy, {})));
          }
        }
      }
    }
  }

  // Build a map of all of the PersistentSourceLoc's back to some kind of
  // Stmt, Decl, or Type.
  TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
  std::set<PersistentSourceLoc> Keys;
  for (const auto &I : Info.getVarMap())
    Keys.insert(I.first);
  MappingVisitor MV(Keys, Context);
  for (const auto &D : TUD->decls()) {
    MV.TraverseDecl(D);
  }
  SourceToDeclMapType PSLMap;
  PSLMap = MV.getResults();

  // Add declarations from this map into the rewriting set
  for (const auto &V : Info.getVarMap()) {
    // PLoc specifies the location of the variable whose type it is to
    // re-write, but not where the actual type storage is. To get that, we
    // need to turn PLoc into a Decl and then get the SourceRange for the
    // type of the Decl. Note that what we need to get is the ExpansionLoc
    // of the type specifier, since we want where the text is printed before
    // the variable name, not the typedef or #define that creates the
    // name of the type.
    PersistentSourceLoc PLoc = V.first;
    if (Decl *D = PSLMap[PLoc]) {
      ConstraintVariable *CV = V.second;
      PVConstraint *PV = dyn_cast<PVConstraint>(CV);
      bool PVChanged =
          PV && (PV->anyChanges(Info.getConstraints().getVariables()) ||
                 ABRewriter.hasNewBoundsString(PV, D));
      if (PVChanged && !PV->isPartOfFunctionPrototype()) {
        // Rewrite a declaration, only if it is not part of function prototype.
        assert(!isa<ParmVarDecl>(D) &&
               "Got a PVConstraint for a ParmVarDecl where "
               "isPartOfFunctionPrototype returns false?");
        MultiDeclMemberDecl *MMD = getAsMultiDeclMember(D);
        assert(MMD && "Unrecognized declaration type.");

        RewrittenDecl RD = mkStringForPVDecl(MMD, PV, Info);
        std::string ReplacementText = RD.Type + RD.IType;
        std::vector<std::string> SDecl;
        if (!RD.SupplementaryDecl.empty())
          SDecl.push_back(RD.SupplementaryDecl);
        RewriteThese.insert(std::make_pair(
            MMD, new MultiDeclMemberReplacement(MMD, ReplacementText, SDecl)));
      }
    }
  }

  // Do the declaration rewriting
  DeclRewriter DeclR(R, Info, Context);
  DeclR.rewrite(RewriteThese);

  for (auto Pair : RewriteThese)
    delete Pair.second;

  DeclR.denestTagDecls();
}

void DeclRewriter::rewrite(RSet &ToRewrite) {
  for (auto Pair : ToRewrite) {
    DeclReplacement *N = Pair.second;
    assert(N->getDecl() != nullptr);

    if (_3COpts.Verbose) {
      errs() << "Replacing type of decl:\n";
      N->getDecl()->dump();
      errs() << "with " << N->getReplacement() << "\n";
    }

    // Exact rewriting procedure depends on declaration type
    if (auto *MR = dyn_cast<MultiDeclMemberReplacement>(N)) {
      MultiDeclInfo *MDI =
          Info.TheMultiDeclsInfo.findContainingMultiDecl(MR->getDecl());
      assert("Missing MultiDeclInfo for multi-decl member" && MDI);
      // A multi-decl can only be rewritten as a unit. If at least one member
      // needs rewriting, then the first MultiDeclMemberReplacement in iteration
      // order of ToRewrite (which need not have anything to do with member
      // order of the multi-decl) triggers rewriting of the entire multi-decl,
      // and rewriteMultiDecl checks ToRewrite for a MultiDeclMemberReplacement
      // for each member of the multi-decl and applies it if found.
      if (!MDI->AlreadyRewritten)
        rewriteMultiDecl(*MDI, ToRewrite);
    } else if (auto *FR = dyn_cast<FunctionDeclReplacement>(N)) {
      rewriteFunctionDecl(FR);
    } else {
      assert(false && "Unknown replacement type");
    }
  }
}

void DeclRewriter::denestTagDecls() {
  // When there are multiple levels of nested TagDecls, we need to process
  // all the children of a TagDecl TD before TD itself so that (1) the
  // definitions of the children end up before the definition of TD (since the
  // rewriter preserves order of insertions) and (2) the definitions of the
  // children have been removed from the body of TD before we read the body of
  // TD to move it. In effect, we want to process the TagDecls in postorder.
  // The easiest way to achieve this is to process them in order of their _end_
  // locations.
  std::sort(TagDeclsToDenest.begin(), TagDeclsToDenest.end(),
            [&](TagDecl *TD1, TagDecl *TD2) {
              return A.getSourceManager().isBeforeInTranslationUnit(
                  TD1->getEndLoc(), TD2->getEndLoc());
            });
  for (TagDecl *TD : TagDeclsToDenest) {
    // rewriteMultiDecl replaced the final "}" in the original source range with
    // "};\n", so the new content of the source range should include the ";\n",
    // which is what we want here. Except the rewriter has a bug where it
    // adjusts the token range to include the final token _after_ mapping the
    // offset to account for previous edits (it should be before). We work
    // around the bug by adjusting the token range before calling the rewriter
    // at all.
    CharSourceRange CSR = Lexer::makeFileCharRange(
        CharSourceRange::getTokenRange(TD->getSourceRange()), R.getSourceMgr(),
        R.getLangOpts());
    std::string DefinitionStr = R.getRewrittenText(CSR);
    // Delete the definition from the old location.
    rewriteSourceRange(R, CSR, "");
    // We want to find the highest ancestor DeclContext of TD that is a TagDecl
    // (call it TopTagDecl) and insert TD just before TopTagDecl.
    //
    // As of this writing, it seems that if TD is named, `TD->getDeclContext()`
    // returns the parent of TopTagDecl due to the code at
    // https://github.com/correctcomputation/checkedc-clang/blob/fd4d8af4383d40af10ee8bc92b7bf88061a11035/clang/lib/Sema/SemaDecl.cpp#L16980-L16981,
    // But that code doesn't run if TD is unnamed (which makes some sense
    // because name visibility isn't an issue for TagDecls that have no name),
    // and we want to de-nest TagDecls with names we assigned just like ones
    // that were originally named, so we can't just use `TD->getDeclContext()`.
    // In any event, maybe we wouldn't want to rely on this kind of internal
    // Clang behavior.
    TagDecl *TopTagDecl = TD;
    while (TagDecl *ParentTagDecl =
               dyn_cast<TagDecl>(TopTagDecl->getLexicalDeclContext()))
      TopTagDecl = ParentTagDecl;
    // If TopTagDecl is preceded by qualifiers, ideally we'd like to insert TD
    // before those qualifiers. If TopTagDecl is actually part of a multi-decl
    // with at least one member, then we can just use the begin location of that
    // multi-decl as the insertion point.
    //
    // If there are no members (so the qualifiers have no effect and generate a
    // compiler warning), then there isn't an easy way for us to find the source
    // location before the qualifiers. In that case, we just insert TD at the
    // begin location of TopTagDecl (after the qualifiers) and hope for the
    // best. In the cases we're aware of so far (storage qualifiers, including
    // `typedef`), this just means that the qualifiers end up applying to the
    // first TagDecl de-nested from TopTagDecl instead of to TopTagDecl itself,
    // and they generate the same compiler warning as before but on a different
    // TagDecl. However, we haven't confirmed that there aren't any obscure
    // cases that could lead to an error, such as if a qualifier is valid on one
    // kind of TagDecl but not another.
    SourceLocation InsertLoc;
    if (MultiDeclInfo *MDI =
            Info.TheMultiDeclsInfo.findContainingMultiDecl(TopTagDecl))
      InsertLoc = MDI->Members[0]->getBeginLoc();
    else
      InsertLoc = TopTagDecl->getBeginLoc();
    // TODO: Use a wrapper like rewriteSourceRange that tries harder with
    // macros, reports failure, etc.
    // (https://github.com/correctcomputation/checkedc-clang/issues/739)
    R.InsertText(InsertLoc, DefinitionStr);
  }
}

void DeclRewriter::rewriteMultiDecl(MultiDeclInfo &MDI, RSet &ToRewrite) {
  // Rewrite a "multi-decl" consisting of one or more variables, fields, or
  // typedefs declared in a comma-separated list based on a single type "on the
  // left". See the comment at the top of clang/include/clang/3C/MultiDecls.h
  // for a detailed description of the design that is implemented here. As
  // mentioned in MultiDecls.h, this code is used even for "multi-decls" that
  // have only a single member to avoid having to maintain a separate code path
  // for them.
  //
  // Due to the overlap between members, a multi-decl can only be rewritten as a
  // unit, visiting the members in source code order from left to right. For
  // each member, we check whether it has a replacement in ToRewrite. If so, we
  // use it; if not, we generate a declaration equivalent to the original.
  // Existing initializers are preserved, and declarations that need an
  // initializer to be valid Checked C are given one.

  SourceManager &SM = A.getSourceManager();
  bool IsFirst = true;
  SourceLocation PrevEnd;

  if (MDI.TagDefToSplit != nullptr) {
    TagDecl *TD = MDI.TagDefToSplit;
    // `static struct T { ... } x;` -> `struct T { ... }; static struct T x;`
    // A storage qualifier such as `static` applies to the members but is not
    // meaningful on TD after it is split, and we need to remove it to avoid a
    // compiler warning. The beginning location of the first member should be
    // the `static` and the beginning location of TD should be the `struct`, so
    // we just remove anything between those locations. (Can other things appear
    // there? We hope it makes sense to remove them too.)
    //
    // We use `getCharRange` to get a range that excludes the first token of TD,
    // unlike the default conversion of a SourceRange to a "token range", which
    // would include it.
    rewriteSourceRange(R,
                       CharSourceRange::getCharRange(
                           MDI.Members[0]->getBeginLoc(), TD->getBeginLoc()),
                       "");
    if (TD->getName().empty()) {
      // If the record is unnamed, insert the name that we assigned it:
      // `struct {` -> `struct T {`
      PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(TD, A);
      // This will assert if we can't find the new name. Is that what we want?
      std::string NewTypeStr = *Info.TheMultiDeclsInfo.getTypeStrOverride(
          A.getTagDeclType(TD).getTypePtr(), A);
      // This token should be the tag kind, e.g., `struct`.
      std::string ExistingToken =
          getSourceText(SourceRange(TD->getBeginLoc()), A);
      if (NONFATAL_ASSERT_PLACEHOLDER(ExistingToken == TD->getKindName())) {
        rewriteSourceRange(R, TD->getBeginLoc(), NewTypeStr);
      }
    }
    // Make a note if the TagDecl needs to be de-nested later.
    if (isa<TagDecl>(TD->getLexicalDeclContext()))
      TagDeclsToDenest.push_back(TD);
    // `struct T { ... } foo;` -> `struct T { ... };\nfoo;`
    rewriteSourceRange(R, TD->getEndLoc(), "};\n");
    IsFirst = false;
    // Offset by one to skip past what we've just added so it isn't overwritten.
    PrevEnd = TD->getEndLoc().getLocWithOffset(1);
  }

  for (auto MIt = MDI.Members.begin(); MIt != MDI.Members.end(); MIt++) {
    MultiDeclMemberDecl *DL = *MIt;

    // If we modify this member in any way, this is the original source range
    // for the member that we expect to overwrite, before PrevEnd adjustment.
    //
    // We do want to overwrite existing Checked C annotations. Other than
    // ItypesForExtern, 3C currently doesn't have real itype support for
    // multi-decl members as opposed to function parameters and returns
    // (https://github.com/correctcomputation/checkedc-clang/issues/744), but we
    // are probably still better off overwriting the annotations with a
    // complete, valid declaration than mixing and matching a declaration
    // generated by 3C with existing annotations.
    //
    // If the variable has an initializer, we want this rewrite to end
    // before the initializer to avoid interfering with any other rewrites
    // that 3C needs to make inside the initializer expression
    // (https://github.com/correctcomputation/checkedc-clang/issues/267).
    SourceRange ReplaceSR =
        getDeclSourceRangeWithAnnotations(DL, /*IncludeInitializer=*/false);

    // Look for a declaration replacement object for the current declaration.
    MultiDeclMemberReplacement *Replacement = nullptr;
    auto TRIt = ToRewrite.find(DL);
    if (TRIt != ToRewrite.end()) {
      Replacement = cast<MultiDeclMemberReplacement>(TRIt->second);
      // We can't expect multi-decl rewriting to work properly on a source range
      // different from ReplaceSR above; for example, doDeclRewrite might insert
      // an initializer in the wrong place. This assertion should pass as long
      // as the implementation of DeclReplacement::getSourceRange matches
      // ReplaceSR above. If someone changes DeclReplacement::getSourceRange,
      // thinking that they can replace a different source range that way, we
      // want to fail fast.
      //
      // This is awkward and makes me wonder if we should just remove
      // DeclReplacement::getSourceRange since 3C currently only calls
      // getSourceRange on an object already known to be a
      // FunctionDeclReplacement. But after drafting that, I wasn't convinced
      // that it was better than the status quo.
      assert(Replacement->getSourceRange(SM) == ReplaceSR);
    }

    if (IsFirst) {
      // Rewriting the first declaration is easy. Nothing should change if its
      // type does not to be rewritten.
      IsFirst = false;
      if (Replacement) {
        doDeclRewrite(ReplaceSR, Replacement);
      }
    } else {
      // ReplaceSR.getBegin() is the beginning of the whole multi-decl. We only
      // want to replace the text starting after the previous multi-decl member,
      // which is given by PrevEnd.
      ReplaceSR.setBegin(PrevEnd);

      // The subsequent decls are more complicated because we need to insert a
      // type string even if the variables type hasn't changed.
      if (Replacement) {
        // If the type has changed, the DeclReplacement object has a replacement
        // string stored in it that should be used.
        doDeclRewrite(ReplaceSR, Replacement);
      } else {
        // When the type hasn't changed, we still need to insert the original
        // type for the variable.
        std::string NewDeclStr = mkStringForDeclWithUnchangedType(DL, Info);
        rewriteSourceRange(R, ReplaceSR, NewDeclStr);
      }
    }

    // Processing related to the comma or semicolon ("terminator") that follows
    // the multi-decl member. Members are separated by commas, and the last
    // member is terminated by a semicolon. The rewritten decls are each
    // terminated by a semicolon and are separated by newlines.
    bool IsLast = (MIt + 1 == MDI.Members.end());
    bool HaveSupplementaryDecls =
        (Replacement && !Replacement->getSupplementaryDecls().empty());
    // Unlike in ReplaceSR, we want to start searching for the terminator after
    // the entire multi-decl member, including any existing initializer.
    SourceRange FullSR =
        getDeclSourceRangeWithAnnotations(DL, /*IncludeInitializer=*/true);
    // Search for the terminator.
    //
    // FIXME: If the terminator is hidden inside a macro,
    // getNextCommaOrSemicolon will continue scanning and may return a comma or
    // semicolon later in the file (which has bizarre consequences if we try to
    // use it to rewrite this multi-decl) or fail an assertion if it doesn't
    // find one. As a stopgap for the existing regression test in
    // macro_rewrite_error.c that has a semicolon inside a macro, we only search
    // for the terminator if we actually need it.
    SourceLocation Terminator;
    if (!IsLast || HaveSupplementaryDecls) {
      Terminator = getNextCommaOrSemicolon(FullSR.getEnd());
    }
    if (!IsLast) {
      // We expect the terminator to be a comma. Change it to a semicolon.
      rewriteSourceRange(R, SourceRange(Terminator, Terminator), ";");
    }
    if (HaveSupplementaryDecls) {
      emitSupplementaryDeclarations(Replacement->getSupplementaryDecls(),
                                    Terminator);
    }
    if (!IsLast) {
      // Insert a newline between this multi-decl member and the next. The
      // Rewriter preserves the order of insertions at the same location, so if
      // there are supplementary declarations, this newline will go between them
      // and the next member, which is what we want because
      // emitSupplementaryDeclarations by itself doesn't add a newline after the
      // supplementary declarations.
      SourceLocation AfterTerminator =
          getLocationAfterToken(Terminator, A.getSourceManager(),
                                A.getLangOpts());
      R.InsertText(AfterTerminator, "\n");
      // When rewriting the next member, start after the terminator. The
      // Rewriter is smart enough not to mess with anything we already inserted
      // at that location.
      PrevEnd = AfterTerminator;
    }
  }

  MDI.AlreadyRewritten = true;
}

// Common rewriting logic used to replace a single decl either on its own or as
// part of a multi decl. The primary responsibility of this method (aside from
// invoking the rewriter) is to add any required initializer expression.
void DeclRewriter::doDeclRewrite(SourceRange &SR, DeclReplacement *N) {
  std::string Replacement = N->getReplacement();
  if (auto *VD = dyn_cast<VarDecl>(N->getDecl())) {
    if (!VD->hasInit()) {
      // There is no initializer. Add it if we need one.
      // MWH -- Solves issue 43. Should make it so we insert NULL if stdlib.h or
      // stdlib_checked.h is included
      // TODO: Centralize initialization logic for all types:
      // https://github.com/correctcomputation/checkedc-clang/issues/645#issuecomment-876474200
      // TODO: Don't add unnecessary initializers to global variables:
      // https://github.com/correctcomputation/checkedc-clang/issues/741
      if (VD->getStorageClass() != StorageClass::SC_Extern) {
        const std::string NullPtrStr = "((void *)0)";
        if (isPointerType(VD)) {
          Replacement += " = " + NullPtrStr;
        } else if (VD->getType()->isArrayType()) {
          const auto *ElemType = VD->getType()->getPointeeOrArrayElementType();
          if (ElemType->isPointerType())
            Replacement += " = {" + NullPtrStr + "}";
        }
      }
    }
  }

  rewriteSourceRange(R, SR, Replacement);
}

void DeclRewriter::rewriteFunctionDecl(FunctionDeclReplacement *N) {
  rewriteSourceRange(R, N->getSourceRange(A.getSourceManager()),
                     N->getReplacement());
  if (N->getDecl()->isThisDeclarationADefinition()) {
    Stmt *S = N->getDecl()->getBody();
    assert("Supplementary declarations should only exist on rewritings for "
           "function definitions." && S != nullptr);
    // Insert supplementary declarations after the opening curly brace of the
    // function body.
    emitSupplementaryDeclarations(N->getSupplementaryDecls(),
                                  S->getBeginLoc());
  }
}

void DeclRewriter::emitSupplementaryDeclarations(
  const std::vector<std::string> &SDecls, SourceLocation Loc) {
  // There are no supplementary declarations to emit. The AllDecls String
  // will remain empty, so insertText should no-op, but it's still an error to
  // insert an empty string at an invalid source location, so short circuit here
  // to be safe.
  if (SDecls.empty())
    return;

  std::string AllDecls;
  for (std::string D : SDecls)
    AllDecls += "\n" + D;

  R.InsertText(getLocationAfterToken(Loc, R.getSourceMgr(), R.getLangOpts()),
               AllDecls);
}

// Uses clangs lexer to find the location of the next comma or semicolon after
// the given source location. This is used to find the end of each declaration
// within a multi-declaration.
SourceLocation DeclRewriter::getNextCommaOrSemicolon(SourceLocation L) {
  SourceManager &SM = A.getSourceManager();
  auto Tok = Lexer::findNextToken(L, SM, A.getLangOpts());
  while (Tok.has_value() && !Tok->is(clang::tok::eof)) {
    if (Tok->is(clang::tok::comma) || Tok->is(clang::tok::semi))
      return Tok->getLocation();
    Tok = Lexer::findNextToken(Tok->getEndLoc(), A.getSourceManager(),
                               A.getLangOpts());
  }
  llvm_unreachable("Unable to find comma or semicolon at source location.");
}

// This function checks how to re-write a function declaration.
bool FunctionDeclBuilder::VisitFunctionDecl(FunctionDecl *FD) {

  // Get the constraint variable for the function.
  // For the return value and each of the parameters, do the following:
  //   1. Get a constraint variable representing the definition (def) and the
  //      uses ("arguments").
  //   2. If arguments could be wild but def is not, we insert a bounds-safe
  //      interface.
  // If we don't have a definition in scope, we can assert that all of
  // the constraint variables are equal.
  // Finally, we need to note that we've visited this particular function, and
  // that we shouldn't make one of these visits again.

  auto FuncName = FD->getNameAsString();

  FVConstraint *FDConstraint = Info.getFuncConstraint(FD, Context);
  if (!FDConstraint)
    return true;

  // If this is an external function, there is no need to rewrite the
  // declaration. We cannot change the signature of external functions.
  // Under the flag -infer-types-for-undef, however, undefined functions do need
  // to be rewritten. If the rest of the 3c inference and rewriting code is
  // correct, short-circuiting here shouldn't be necessary; the rest of the
  // logic in this function should successfully not rewrite undefined functions
  // when -infer-types-for-undef is not passed. This assumption could be
  // transformed into an assertion if we're confident it won't fail in too many
  // places.
  if (!_3COpts.InferTypesForUndefs && !FDConstraint->hasBody())
    return true;

  // RewriteParams and RewriteReturn track if we will need to rewrite the
  // parameter and return type declarations on this function. They are first
  // set to true if any changes are made to the types of the parameter and
  // return. If a type has changed, then it must be rewritten. There are then
  // some special circumstances which require rewriting the parameter or return
  // even when the type as not changed.
  bool RewriteParams = false;
  bool RewriteReturn = false;

  // RewriteGeneric is similar to the above, but we need to further check
  // if the potential generic variables were set to wild by the constraint
  // resolver. In that case don't rewrite.
  bool RewriteGeneric = false;

  bool DeclIsTypedef = false;
  TypeSourceInfo *TS = FD->getTypeSourceInfo();
  if (TS != nullptr) {
    // This still could possibly be a typedef type if TS was NULL.
    // TypeSourceInfo is null for implicit function declarations, so if a
    // implicit declaration uses a typedef, it will be missed. That's fine
    // since an implicit declaration can't be rewritten anyways.
    // There might be other ways it can be null that I'm not aware of.
    QualType QT = TS->getType();
    if (auto *EType = dyn_cast_or_null<ElaboratedType>(QT)) {
      QualType TypedefTy = EType->desugar();
      DeclIsTypedef = isa<TypedefType>(TypedefTy);
    }
  }

  // If we've made this generic we need add "_For_any" or "_Itype_for_any"
  if (FDConstraint->getGenericParams() > 0
      && !FD->isGenericFunction() && !FD->isItypeGenericFunction())
    RewriteGeneric = true;

  // This will keep track of the supplementary declarations that are required by
  // function parameters. The new declarations will be emitted inside the
  // function body in the order of function parameters that generated them.
  std::vector<std::string> SDecls;
  bool GenerateSDecls = FD->isThisDeclarationADefinition();

  // Get rewritten parameter variable declarations. Try to use
  // the source for as much as possible.
  std::vector<std::string> ParmStrs;

  // Needed to distinguish between Itype_for_any and For_any
  bool ProtoHasItype = false;

  // Typedefs must be expanded for now, so allow interpret them as rewritable
  // by ignoring their special case code.
  // See the FIXME below for more info.
  //  if (DeclIsTypedef) {
  //    // typedef: don't rewrite
  //  } else
  if (FD->getParametersSourceRange().isValid()) {
    // has its own params: alter them as necessary
    for (unsigned I = 0; I < FD->getNumParams(); ++I) {
      ParmVarDecl *PVDecl = FD->getParamDecl(I);
      const FVComponentVariable *CV = FDConstraint->getCombineParam(I);
      RewrittenDecl RD =
        this->buildDeclVar(CV, PVDecl, PVDecl->getQualifiedNameAsString(),
                           RewriteGeneric, RewriteParams, RewriteReturn,
                           FD->isStatic(), GenerateSDecls);
      ParmStrs.push_back(RD.Type + RD.IType);
      if (!RD.SupplementaryDecl.empty())
        SDecls.push_back(RD.SupplementaryDecl);
      ProtoHasItype |= !RD.IType.empty();
    }
  } else if (FDConstraint->numParams() != 0) {
    // lacking params but the constraint has them: mirror the constraint
    for (unsigned I = 0; I < FDConstraint->numParams(); ++I) {
      ParmVarDecl *PVDecl = nullptr;
      const FVComponentVariable *CV = FDConstraint->getCombineParam(I);
      RewrittenDecl RD =
        this->buildDeclVar(CV, PVDecl, "", RewriteGeneric, RewriteParams,
                           RewriteReturn, FD->isStatic(), GenerateSDecls);
      ParmStrs.push_back(RD.Type + RD.IType);
      if (!RD.SupplementaryDecl.empty())
        SDecls.push_back(RD.SupplementaryDecl);
      ProtoHasItype |= !RD.IType.empty();
      // FIXME: when the above FIXME is changed this condition will always
      // be true. This is correct, always rewrite if there were no params
      // in source but they exist in the constraint variable.
      if (!DeclIsTypedef)
        RewriteParams = true;
    }
  } else {
    // Functions in CheckedC need prototypes, so replace empty parameter lists
    // with an explict (void). This updates the parameter list; the rewrite flag
    // will be set once it is known if the return needs to be rewritten.
    ParmStrs.push_back("void");
  }

  // Get rewritten return variable.
  // For now we still need to check if this needs rewriting, see FIXME below
  // if (!DeclIsTypedef)
  RewrittenDecl RewrittenReturn =
    this->buildDeclVar(FDConstraint->getCombineReturn(), FD, "", RewriteGeneric,
                       RewriteParams, RewriteReturn, FD->isStatic(),
                       GenerateSDecls);
  assert("Supplementary declarations should not be generated for return." &&
         RewrittenReturn.SupplementaryDecl.empty());

  ProtoHasItype |= !RewrittenReturn.IType.empty();

  // Generic forany and return are in the same rewrite location, so
  // we must rewrite the return if rewriting generic
  if (RewriteGeneric)
    RewriteReturn = true;

  // If the return is a function pointer, we need to rewrite the whole
  // declaration even if no actual changes were made to the parameters because
  // the parameter for the function pointer type appear later in the source than
  // the parameters for the function declaration. It could probably be done
  // better, but getting the correct source locations is painful.
  if (FD->getReturnType()->isFunctionPointerType() && RewriteReturn)
    RewriteParams = true;

  // If we're making this into a generic function, we'll
  // rewrite parameters in case there's an itype in there that won't trigger
  // a normal rewrite. Temp fix for #678 in generics case.
  if (RewriteGeneric) {
    RewriteParams = true;
  }

  // If this function was declared without a prototype, then we must add one
  // to be able to give it a checked return type. This was done by adding "void"
  // to the parameter list above. Here we indicate the parameter list should be
  // rewritten to include "void" only if the return is already being rewritten.
  // This avoids unnecessarily adding void to empty parameter lists on unchecked
  // functions.
  if (TS && !TS->getType()->isFunctionProtoType() && RewriteReturn)
    RewriteParams = true;

  // If the function is declared using a typedef for the function type, then we
  // need to rewrite parameters and the return if either would have been
  // rewritten. What this does is expand the typedef to the full function type
  // to avoid the problem of rewriting inside the typedef.
  // FIXME: If issue #437 is fixed in way that preserves typedefs on function
  //        declarations, then this conditional should be removed to enable
  //        separate rewriting of return type and parameters on the
  //        corresponding definition.
  //        https://github.com/correctcomputation/checkedc-clang/issues/437
  if ((RewriteReturn || RewriteParams) && DeclIsTypedef) {
    RewriteParams = true;
    RewriteReturn = true;
  }

  // Mirrors the check above that sets RewriteGeneric to true.
  // If we've decided against making this generic, remove the generic params
  // so later rewrites (of typeparams) don't happen
  if (!RewriteGeneric && FDConstraint->getGenericParams() > 0
      && !FD->isGenericFunction() && !FD->isItypeGenericFunction())
    FDConstraint->resetGenericParams();

  // If this was an itype but is now checked, we'll be changing
  // "_Itype_for_any" to "_For_any"
  if (!RewriteGeneric && FD->isItypeGenericFunction() && !ProtoHasItype) {
    RewriteGeneric = true;
    RewriteReturn = true;
  }

  // Combine parameter and return variables rewritings into a single rewriting
  // for the entire function declaration.
  std::string NewSig = "";
  if (RewriteGeneric) {
    if (ProtoHasItype)
      NewSig += "_Itype_for_any(T";
    else
      NewSig += "_For_any(T";
    for (int i = 0; i < FDConstraint->getGenericParams() - 1; i++) {
      assert(i < 2 &&
             "Need an unexpected number of type variables");
      NewSig += std::begin({",U",",V"})[i];
    }
    NewSig += ") ";
  }
  if (RewriteReturn)
    NewSig += getStorageQualifierString(FD) + RewrittenReturn.Type;

  if (RewriteReturn && RewriteParams)
    NewSig += FDConstraint->getName();

  if (RewriteParams && !ParmStrs.empty()) {
    // Gather individual parameter strings into a single buffer
    std::ostringstream ConcatParamStr;
    copy(ParmStrs.begin(), ParmStrs.end() - 1,
         std::ostream_iterator<std::string>(ConcatParamStr, ", "));
    ConcatParamStr << ParmStrs.back();

    NewSig += "(" + ConcatParamStr.str();
    // Add varargs.
    if (functionHasVarArgs(FD))
      NewSig += ", ...";
    NewSig += ")";
  }
  NewSig = NewSig + RewrittenReturn.IType;

  // Add new declarations to RewriteThese if it has changed
  if (RewriteReturn || RewriteParams) {
    auto *FDR = new FunctionDeclReplacement(FD, NewSig, SDecls, RewriteReturn,
                                            RewriteParams, RewriteGeneric);
    RewriteThese.insert(std::make_pair(FD, FDR));
  }

  return true;
}

RewrittenDecl
FunctionDeclBuilder::buildCheckedDecl(PVConstraint *Defn, DeclaratorDecl *Decl,
                                      std::string UseName, bool &RewriteParm,
                                      bool &RewriteRet, bool GenerateSDecls) {
  RewrittenDecl RD = DeclRewriter::buildCheckedDecl(Defn, Decl, UseName, Info,
                                                    ABRewriter, GenerateSDecls);
  RewriteParm |= getExistingIType(Defn).empty() != RD.IType.empty() ||
                 isa_and_nonnull<ParmVarDecl>(Decl);
  RewriteRet |= isa_and_nonnull<FunctionDecl>(Decl);
  return RD;
}

RewrittenDecl
FunctionDeclBuilder::buildItypeDecl(PVConstraint *Defn, DeclaratorDecl *Decl,
                                    std::string UseName, bool &RewriteParm,
                                    bool &RewriteRet, bool GenerateSDecls,
                                    bool SDeclChecked) {
  Info.getPerfStats().incrementNumITypes();
  RewrittenDecl RD = DeclRewriter::buildItypeDecl(
      Defn, Decl, UseName, Info, ABRewriter, GenerateSDecls, SDeclChecked);
  RewriteParm = true;
  RewriteRet |= isa_and_nonnull<FunctionDecl>(Decl);
  return RD;
}

// Note: For a parameter, Type + IType will give the full declaration (including
// the name) but the breakdown between Type and IType is not guaranteed. For a
// return, Type will be what goes before the name and IType will be what goes
// after the parentheses.
RewrittenDecl
FunctionDeclBuilder::buildDeclVar(const FVComponentVariable *CV,
                                  DeclaratorDecl *Decl, std::string UseName,
                                  bool &RewriteGen, bool &RewriteParm,
                                  bool &RewriteRet, bool StaticFunc,
                                  bool GenerateSDecls) {

  bool CheckedSolution = CV->hasCheckedSolution(Info.getConstraints());
  bool ItypeSolution = CV->hasItypeSolution(Info.getConstraints());
  if (ItypeSolution ||
      (CheckedSolution && _3COpts.ItypesForExtern && !StaticFunc)) {
    return buildItypeDecl(CV->getExternal(), Decl, UseName, RewriteParm,
                          RewriteRet, GenerateSDecls, CheckedSolution);
  }
  if (CheckedSolution) {
    return buildCheckedDecl(CV->getExternal(), Decl, UseName, RewriteParm,
                            RewriteRet, GenerateSDecls);
  }

  // Don't add generics if one of the potential generic params is wild,
  // even if it could have an itype
  if (!CheckedSolution && CV->getExternal()->isGenericChanged())
    RewriteGen = false;

// If the type of the pointer hasn't changed, then neither of the above
  // branches will be taken, but it's still possible for the bounds of an array
  // pointer to change.
  if (ABRewriter.hasNewBoundsString(CV->getExternal(), Decl)) {
    RewriteParm = true;
    RewriteRet |= isa_and_nonnull<FunctionDecl>(Decl);
  }
  std::string BoundsStr = ABRewriter.getBoundsString(
      CV->getExternal(), Decl, !getExistingIType(CV->getExternal()).empty());

  // Variables that do not need to be rewritten fall through to here.
  // Try to use the source.
  std::string Type, IType;
  ParmVarDecl *PVD = dyn_cast_or_null<ParmVarDecl>(Decl);
  if (PVD && !PVD->getName().empty()) {
    SourceRange Range = PVD->getSourceRange();
    if (PVD->hasBoundsExpr())
      Range.setEnd(PVD->getBoundsExpr()->getEndLoc());
    if (Range.isValid() && !inParamMultiDecl(PVD))
      Type = getSourceText(Range, *Context);
    // Otherwise, reconstruct the name and type, and reuse the code below for
    // the itype and bounds.
    // TODO: Do we care about `register` or anything else this doesn't handle?
    if (Type.empty())
      Type = qtyToStr(PVD->getOriginalType(), PVD->getNameAsString());
  } else {
    Type = CV->mkTypeStr(Info.getConstraints(), true,
                         CV->getExternal()->getName());
  }
  IType = getExistingIType(CV->getExternal()) + BoundsStr;
  return RewrittenDecl(Type, IType, "");
}

std::string FunctionDeclBuilder::getExistingIType(ConstraintVariable *DeclC) {
  auto *PVC = dyn_cast<PVConstraint>(DeclC);
  if (PVC != nullptr && !PVC->getItype().empty())
    return " : " + PVC->getItype();
  return "";
}

// Check if the function is handled by this visitor.
bool FunctionDeclBuilder::isFunctionVisited(std::string FuncName) {
  return VisitedSet.find(FuncName) != VisitedSet.end();
}

// K&R style function declarations can declare multiple parameter variables in
// a single declaration statement. The source ranges for these parameters
// overlap, so we cannot copy the declaration from source code to output code
bool FunctionDeclBuilder::inParamMultiDecl(const ParmVarDecl *PVD) {
  const DeclContext *DCtx = PVD->getDeclContext();
  if (DCtx) {
    SourceRange SR = PVD->getSourceRange();
    SourceManager &SM = Context->getSourceManager();
    for (auto *D : DCtx->decls())
      if (D != PVD && D->getBeginLoc().isValid() &&
          SM.isPointWithin(D->getBeginLoc(), SR.getBegin(), SR.getEnd()))
        return true;
  }
  return false;
}
