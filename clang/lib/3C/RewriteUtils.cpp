//=--RewriteUtils.cpp---------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This class contains implementation of the functions and
// classes of RewriteUtils.h
//===----------------------------------------------------------------------===//

#include "clang/3C/RewriteUtils.h"
#include "clang/3C/3CGlobalOptions.h"
#include "clang/3C/CastPlacement.h"
#include "clang/3C/CheckedRegions.h"
#include "clang/3C/DeclRewriter.h"
#include "clang/3C/TypeVariableAnalysis.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Tooling/Transformer/SourceCode.h"

using namespace llvm;
using namespace clang;

SourceLocation DComp::getDeclBegin(DeclReplacement *D) const {
  SourceLocation Begin =
      (*D->getStatement()->decls().begin())->getSourceRange().getBegin();
  for (const auto &DT : D->getStatement()->decls()) {
    if (DT == D->getDecl())
      return Begin;
    Begin = DT->getSourceRange().getEnd();
  }
  llvm_unreachable("Declaration not found in DeclStmt.");
}

SourceRange DComp::getReplacementSourceRange(DeclReplacement *D) const {
  SourceRange Range = D->getSourceRange(SM);

  // Also take into account whether or not there is a multi-statement
  // decl, because the generated ranges will overlap.
  DeclStmt *LhStmt = D->getStatement();
  if (LhStmt && !LhStmt->isSingleDecl()) {
    SourceLocation NewBegin = getDeclBegin(D);
    Range.setBegin(NewBegin);
    // This is needed to make the subsequent test inclusive.
    Range.setEnd(Range.getEnd().getLocWithOffset(-1));
  }

  return Range;
}

bool DComp::operator()(DeclReplacement *Lhs, DeclReplacement *Rhs) const {
  // Does the source location of the Decl in lhs overlap at all with
  // the source location of rhs?
  SourceRange SrLhs = getReplacementSourceRange(Lhs);
  SourceRange SrRhs = getReplacementSourceRange(Rhs);

  SourceLocation X1 = SrLhs.getBegin();
  SourceLocation X2 = SrLhs.getEnd();
  SourceLocation Y1 = SrRhs.getBegin();
  SourceLocation Y2 = SrRhs.getEnd();

  if (Lhs->getStatement() == nullptr && Rhs->getStatement() == nullptr) {
    // These are global declarations. Get the source location
    // and compare them lexicographically.
    PresumedLoc LHsPLocB = SM.getPresumedLoc(X2);
    PresumedLoc RHsPLocE = SM.getPresumedLoc(Y2);

    // Are both the source location valid?
    if (LHsPLocB.isValid() && RHsPLocE.isValid()) {
      // They are in same fine?
      if (!strcmp(LHsPLocB.getFilename(), RHsPLocE.getFilename())) {
        // Are they in same line?
        if (LHsPLocB.getLine() == RHsPLocE.getLine())
          return LHsPLocB.getColumn() < RHsPLocE.getColumn();

        return LHsPLocB.getLine() < RHsPLocE.getLine();
      }
      return strcmp(LHsPLocB.getFilename(), RHsPLocE.getFilename()) > 0;
    }
    return LHsPLocB.isValid();
  }

  bool Contained = SM.isBeforeInTranslationUnit(X1, Y2) &&
                   SM.isBeforeInTranslationUnit(Y1, X2);

  if (Contained)
    return false;
  return SM.isBeforeInTranslationUnit(X2, Y1);
}

void GlobalVariableGroups::addGlobalDecl(Decl *VD, std::vector<Decl *> *VDVec) {
  if (VD && GlobVarGroups.find(VD) == GlobVarGroups.end()) {
    if (VDVec == nullptr)
      VDVec = new std::vector<Decl *>();
    assert("Decls in group are not ordered correctly." &&
           (VDVec->empty() ||
            SM.isBeforeInTranslationUnit(VDVec->back()->getEndLoc(),
                                         VD->getEndLoc())));
    VDVec->push_back(VD);
    GlobVarGroups[VD] = VDVec;
    // Process the next decl.
    Decl *NDecl = VD->getNextDeclInContext();
    if (isa_and_nonnull<VarDecl>(NDecl) || isa_and_nonnull<FieldDecl>(NDecl))
      if (VD->getBeginLoc() == NDecl->getBeginLoc())
        addGlobalDecl(dyn_cast<Decl>(NDecl), VDVec);
  }
}

std::vector<Decl *> &GlobalVariableGroups::getVarsOnSameLine(Decl *D) {
  assert(GlobVarGroups.find(D) != GlobVarGroups.end() &&
         "Expected to find the group.");
  return *(GlobVarGroups[D]);
}

GlobalVariableGroups::~GlobalVariableGroups() {
  std::set<std::vector<Decl *> *> VVisited;
  // Free each of the group.
  for (auto &CurrV : GlobVarGroups) {
    // Avoid double free by caching deleted sets.
    if (VVisited.find(CurrV.second) != VVisited.end())
      continue;
    VVisited.insert(CurrV.second);
    delete (CurrV.second);
  }
  GlobVarGroups.clear();
}

// Test to see if we can rewrite a given SourceRange.
// Note that R.getRangeSize will return -1 if SR is within
// a macro as well. This means that we can't re-write any
// text that occurs within a macro.
bool canRewrite(Rewriter &R, const CharSourceRange &SR) {
  return SR.isValid() && (R.getRangeSize(SR) != -1);
}

void rewriteSourceRange(Rewriter &R, const SourceRange &Range,
                        const std::string &NewText, bool ErrFail) {
  rewriteSourceRange(R, CharSourceRange::getTokenRange(Range), NewText,
                     ErrFail);
}

void rewriteSourceRange(Rewriter &R, const CharSourceRange &Range,
                        const std::string &NewText, bool ErrFail) {
  // Attempt to rewrite the source range. First use the source range directly
  // from the parameter.
  bool RewriteSuccess = false;
  if (canRewrite(R, Range))
    RewriteSuccess = !R.ReplaceText(Range, NewText);

  // If initial rewriting attempt failed (either because canRewrite returned
  // false or because ReplaceText failed (returning true), try rewriting again
  // with the source range expanded to be outside any macros used in the range.
  if (!RewriteSuccess) {
    CharSourceRange Expand = clang::Lexer::makeFileCharRange(
        Range, R.getSourceMgr(), R.getLangOpts());
    if (canRewrite(R, Expand))
      RewriteSuccess = !R.ReplaceText(Expand, NewText);
  }

  // Emit an error if we were unable to rewrite the source range. This is more
  // likely to be a bug in 3C than an issue with the input, but emitting a
  // diagnostic here with the intended rewriting is much more useful than
  // crashing with an assert fail.
  if (!RewriteSuccess) {
    clang::DiagnosticsEngine &DE = R.getSourceMgr().getDiagnostics();
    bool ReportError = ErrFail && !AllowRewriteFailures;
    {
      // Put this in a block because Clang only allows one DiagnosticBuilder to
      // exist at a time.
      unsigned ErrorId = DE.getCustomDiagID(
          ReportError ? DiagnosticsEngine::Error : DiagnosticsEngine::Warning,
          "Unable to rewrite converted source range. Intended rewriting: "
          "\"%0\"");
      auto ErrorBuilder = DE.Report(Range.getBegin(), ErrorId);
      ErrorBuilder.AddSourceRange(R.getSourceMgr().getExpansionRange(Range));
      ErrorBuilder.AddString(NewText);
    }
    if (ReportError) {
      unsigned NoteId = DE.getCustomDiagID(
          DiagnosticsEngine::Note,
          "you can use the -allow-rewrite-failures option to temporarily "
          "downgrade this error to a warning");
      // If we pass the location here, the macro call stack gets dumped again,
      // which looks silly.
      DE.Report(NoteId);
    }
  }
}

static void emit(Rewriter &R, ASTContext &C) {
  if (Verbose)
    errs() << "Writing files out\n";

  bool StdoutMode = (OutputPostfix == "-" && OutputDir.empty());
  bool StdoutModeSawMainFile = false;
  SourceManager &SM = C.getSourceManager();
  // Iterate over each modified rewrite buffer.
  for (auto Buffer = R.buffer_begin(); Buffer != R.buffer_end(); ++Buffer) {
    if (const FileEntry *FE = SM.getFileEntryForID(Buffer->first)) {
      assert(FE->isValid());

      DiagnosticsEngine::Level UnwritableChangeDiagnosticLevel =
          AllowUnwritableChanges ? DiagnosticsEngine::Warning
                                 : DiagnosticsEngine::Error;
      auto PrintExtraUnwritableChangeInfo = [&]() {
        DiagnosticsEngine &DE = C.getDiagnostics();
        // With -dump-unwritable-changes and not -allow-unwritable-changes, we
        // want the -allow-unwritable-changes note before the dump.
        if (!DumpUnwritableChanges) {
          unsigned DumpNoteId = DE.getCustomDiagID(
              DiagnosticsEngine::Note,
              "use the -dump-unwritable-changes option to see the new version "
              "of the file");
          DE.Report(DumpNoteId);
        }
        if (!AllowUnwritableChanges) {
          unsigned AllowNoteId = DE.getCustomDiagID(
              DiagnosticsEngine::Note,
              "you can use the -allow-unwritable-changes option to temporarily "
              "downgrade this error to a warning");
          DE.Report(AllowNoteId);
        }
        if (DumpUnwritableChanges) {
          errs() << "=== Beginning of new version of " << FE->getName()
                 << " ===\n";
          Buffer->second.write(errs());
          errs() << "=== End of new version of " << FE->getName() << " ===\n";
        }
      };

      // Check whether we are allowed to write this file.
      std::string FeAbsS = "";
      getCanonicalFilePath(std::string(FE->getName()), FeAbsS);
      if (!canWrite(FeAbsS)) {
        DiagnosticsEngine &DE = C.getDiagnostics();
        unsigned ID =
            DE.getCustomDiagID(UnwritableChangeDiagnosticLevel,
                               "3C internal error: 3C generated changes to "
                               "this file even though it is not allowed to "
                               "write to the file "
                               "(https://github.com/correctcomputation/"
                               "checkedc-clang/issues/387)");
        DE.Report(SM.translateFileLineCol(FE, 1, 1), ID);
        PrintExtraUnwritableChangeInfo();
        continue;
      }

      if (StdoutMode) {
        if (Buffer->first == SM.getMainFileID()) {
          // This is the new version of the main file. Print it to stdout.
          Buffer->second.write(outs());
          StdoutModeSawMainFile = true;
        } else {
          DiagnosticsEngine &DE = C.getDiagnostics();
          unsigned ID = DE.getCustomDiagID(
              UnwritableChangeDiagnosticLevel,
              "3C generated changes to this file, which is under the base dir "
              "but is not the main file and thus cannot be written in stdout "
              "mode");
          DE.Report(SM.translateFileLineCol(FE, 1, 1), ID);
          PrintExtraUnwritableChangeInfo();
        }
        continue;
      }

      // Produce a path/file name for the rewritten source file.
      std::string NFile;
      std::error_code EC;
      // We now know that we are using either OutputPostfix or OutputDir mode
      // because stdout mode is handled above. OutputPostfix defaults to "-"
      // when it's not provided, so any other value means that we should use
      // OutputPostfix. Otherwise, we must be in OutputDir mode.
      if (OutputPostfix != "-") {
        // That path should be the same as the old one, with a
        // suffix added between the file name and the extension.
        // For example \foo\bar\a.c should become \foo\bar\a.checked.c
        // if the OutputPostfix parameter is "checked" .
        std::string PfName = sys::path::filename(FE->getName()).str();
        std::string DirName = sys::path::parent_path(FE->getName()).str();
        std::string FileName = sys::path::remove_leading_dotslash(PfName).str();
        std::string Ext = sys::path::extension(FileName).str();
        std::string Stem = sys::path::stem(FileName).str();
        NFile = Stem + "." + OutputPostfix + Ext;
        if (!DirName.empty())
          NFile = DirName + sys::path::get_separator().str() + NFile;
      } else {
        assert(!OutputDir.empty());
        // If this does not hold when OutputDir is set, it should have been a
        // fatal error in the _3CInterface constructor.
        assert(filePathStartsWith(FeAbsS, BaseDir));
        // replace_path_prefix is not smart about separators, but this should be
        // OK because getCanonicalFilePath should ensure that neither BaseDir
        // nor OutputDir has a trailing separator.
        SmallString<255> Tmp(FeAbsS);
        llvm::sys::path::replace_path_prefix(Tmp, BaseDir, OutputDir);
        NFile = std::string(Tmp.str());
        EC = llvm::sys::fs::create_directories(sys::path::parent_path(NFile));
        if (EC) {
          DiagnosticsEngine &DE = C.getDiagnostics();
          unsigned ID = DE.getCustomDiagID(
              DiagnosticsEngine::Error,
              "failed to create parent directory of output file \"%0\"");
          auto DiagBuilder = DE.Report(SM.translateFileLineCol(FE, 1, 1), ID);
          DiagBuilder.AddString(NFile);
          continue;
        }
      }

      raw_fd_ostream Out(NFile, EC, sys::fs::F_None);

      if (!EC) {
        if (Verbose)
          errs() << "writing out " << NFile << "\n";
        Buffer->second.write(Out);
      } else {
        DiagnosticsEngine &DE = C.getDiagnostics();
        unsigned ID = DE.getCustomDiagID(DiagnosticsEngine::Error,
                                         "failed to write output file \"%0\"");
        auto DiagBuilder = DE.Report(SM.translateFileLineCol(FE, 1, 1), ID);
        DiagBuilder.AddString(NFile);
        // This is awkward. What to do? Since we're iterating, we could have
        // created other files successfully. Do we go back and erase them? Is
        // that surprising? For now, let's just keep going.
      }
    }
  }

  if (StdoutMode && !StdoutModeSawMainFile) {
    // The main file is unchanged. Write out its original content.
    outs() << SM.getBuffer(SM.getMainFileID())->getBuffer();
  }
}

// Rewrites types that inside other expressions. This includes cast expression
// and compound literal expressions.
class TypeExprRewriter : public clang::RecursiveASTVisitor<TypeExprRewriter> {
public:
  explicit TypeExprRewriter(ASTContext *C, ProgramInfo &I, Rewriter &R)
      : Context(C), Info(I), Writer(R) {}

  bool VisitCompoundLiteralExpr(CompoundLiteralExpr *CLE) {
    SourceRange TypeSrcRange(
        CLE->getBeginLoc().getLocWithOffset(1),
        CLE->getTypeSourceInfo()->getTypeLoc().getEndLoc());
    rewriteType(CLE, TypeSrcRange);
    return true;
  }

  bool VisitCStyleCastExpr(CStyleCastExpr *ECE) {
    SourceRange TypeSrcRange(
        ECE->getBeginLoc().getLocWithOffset(1),
        ECE->getTypeInfoAsWritten()->getTypeLoc().getEndLoc());
    rewriteType(ECE, TypeSrcRange);
    return true;
  }

private:
  ASTContext *Context;
  ProgramInfo &Info;
  Rewriter &Writer;

  void rewriteType(Expr *E, SourceRange &Range) {
    if (!Info.hasPersistentConstraints(E, Context))
      return;
    const CVarSet &CVSingleton = Info.getPersistentConstraintsSet(E, Context);
    if (CVSingleton.empty())
      return;

    // Macros wil sometimes cause a single expression to have multiple
    // constraint variables. These should have been constrained to wild, so
    // there shouldn't be any rewriting required.
    const EnvironmentMap &Vars = Info.getConstraints().getVariables();
    assert(CVSingleton.size() == 1 ||
           llvm::none_of(CVSingleton, [&Vars](ConstraintVariable *CV) {
             return CV->anyChanges(Vars);
           }));

    for (auto *CV : CVSingleton)
      // Replace the original type with this new one if the type has changed.
      if (CV->anyChanges(Vars))
        rewriteSourceRange(Writer, Range, CV->mkString(Vars, false));
  }
};

// Adds type parameters to calls to alloc functions.
// The basic assumption this makes is that an alloc function will be surrounded
// by a cast expression giving its type when used as a type other than void*.
class TypeArgumentAdder : public clang::RecursiveASTVisitor<TypeArgumentAdder> {
public:
  explicit TypeArgumentAdder(ASTContext *C, ProgramInfo &I, Rewriter &R)
      : Context(C), Info(I), Writer(R) {}

  bool VisitCallExpr(CallExpr *CE) {
    if (isa_and_nonnull<FunctionDecl>(CE->getCalleeDecl())) {
      // If the function call already has type arguments, we'll trust that
      // they're correct and not add anything else.
      if (typeArgsProvided(CE))
        return true;

      if (Info.hasTypeParamBindings(CE, Context)) {
        // Construct a string containing concatenation of all type arguments for
        // the function call.
        std::string TypeParamString;
        bool AllInconsistent = true;
        for (auto Entry : Info.getTypeParamBindings(CE, Context))
          if (Entry.second != nullptr) {
            AllInconsistent = false;
            std::string TyStr = Entry.second->mkString(
                Info.getConstraints().getVariables(), false, false, true);
            if (TyStr.back() == ' ')
              TyStr.pop_back();
            TypeParamString += TyStr + ",";
          } else {
            // If it's null, then the type variable was not used consistently,
            // so we can only put void here instead of useful type.
            TypeParamString += "void,";
          }
        TypeParamString.pop_back();

        // don't rewrite to malloc<void>(...), etc, just do malloc(...)
        if (!AllInconsistent) {
          SourceLocation TypeParamLoc = getTypeArgLocation(CE);
          Writer.InsertTextAfter(TypeParamLoc, "<" + TypeParamString + ">");
        }
      }
    }
    return true;
  }

private:
  ASTContext *Context;
  ProgramInfo &Info;
  Rewriter &Writer;

  // Attempt to find the right spot to insert the type arguments. This should be
  // directly after the name of the function being called.
  SourceLocation getTypeArgLocation(CallExpr *Call) {
    Expr *Callee = Call->getCallee()->IgnoreImpCasts();
    if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(Callee)) {
      size_t NameLength = DRE->getNameInfo().getAsString().length();
      return Call->getBeginLoc().getLocWithOffset(NameLength);
    }
    llvm_unreachable("Could find SourceLocation for type arguments!");
  }
};

std::string ArrayBoundsRewriter::getBoundsString(const PVConstraint *PV,
                                                 Decl *D, bool Isitype) {
  auto &ABInfo = Info.getABoundsInfo();

  // Try to find a bounds key for the constraint variable. If we can't,
  // ValidBKey is set to false, indicating that DK has not been initialized.
  BoundsKey DK;
  bool ValidBKey = true;
  if (PV->hasBoundsKey())
    DK = PV->getBoundsKey();
  else if (!ABInfo.tryGetVariable(D, DK))
    ValidBKey = false;

  std::string BString = "";
  // For itype we do not want to add a second ":".
  std::string Pfix = Isitype ? " " : " : ";

  if (ValidBKey && !PV->hasSomeSizedArr()) {
    ABounds *ArrB = ABInfo.getBounds(DK);
    // Only we we have bounds and no pointer arithmetic on the variable.
    if (ArrB != nullptr && !ABInfo.hasPointerArithmetic(DK)) {
      BString = ArrB->mkString(&ABInfo);
      if (!BString.empty())
        BString = Pfix + BString;
    }
  }
  if (BString.empty() && PV->srcHasBounds()) {
    BString = Pfix + PV->getBoundsStr();
  }
  return BString;
}

bool ArrayBoundsRewriter::hasNewBoundsString(const PVConstraint *PV, Decl *D,
                                             bool Isitype) {
  std::string BStr = getBoundsString(PV, D, Isitype);
  // There is a bounds string but has nothing declared?
  return !BStr.empty() && !PV->srcHasBounds();
}

std::set<PersistentSourceLoc> RewriteConsumer::EmittedDiagnostics;
void RewriteConsumer::emitRootCauseDiagnostics(ASTContext &Context) {
  clang::DiagnosticsEngine &DE = Context.getDiagnostics();
  unsigned ID = DE.getCustomDiagID(
      DiagnosticsEngine::Warning, "Root cause for %0 unchecked pointer%s0: %1");
  auto I = Info.getInterimConstraintState();
  SourceManager &SM = Context.getSourceManager();
  for (auto &WReason : I.RootWildAtomsWithReason) {
    // Avoid emitting the same diagnostic message twice.
    WildPointerInferenceInfo PtrInfo = WReason.second;
    PersistentSourceLoc PSL = PtrInfo.getLocation();

    if (PSL.valid() &&
        EmittedDiagnostics.find(PSL) == EmittedDiagnostics.end()) {
      // Convert the file/line/column triple into a clang::SourceLocation that
      // can be used with the DiagnosticsEngine.
      llvm::ErrorOr<const clang::FileEntry *> File =
          SM.getFileManager().getFile(PSL.getFileName());
      if (!File.getError()) {
        SourceLocation SL =
            SM.translateFileLineCol(*File, PSL.getLineNo(), PSL.getColSNo());
        // Limit emitted root causes to those that effect more than one pointer
        // or are in the main file of the TU. Alternatively, don't filter causes
        // if -warn-all-root-cause is passed.
        int PtrCount = I.getNumPtrsAffected(WReason.first);
        if (WarnAllRootCause || SM.isInMainFile(SL) || PtrCount > 1) {
          // SL is invalid when the File is not in the current translation unit.
          if (SL.isValid()) {
            EmittedDiagnostics.insert(PSL);
            auto DiagBuilder = DE.Report(SL, ID);
            DiagBuilder.AddTaggedVal(PtrCount,
                                     DiagnosticsEngine::ArgumentKind::ak_uint);
            DiagBuilder.AddString(WReason.second.getWildPtrReason());
          }
        }
      }
    }
  }
}

void RewriteConsumer::HandleTranslationUnit(ASTContext &Context) {
  Info.enterCompilationUnit(Context);

  Info.getPerfStats().startRewritingTime();

  if (WarnRootCause)
    emitRootCauseDiagnostics(Context);

  // Rewrite Variable declarations
  Rewriter R(Context.getSourceManager(), Context.getLangOpts());
  DeclRewriter::rewriteDecls(Context, Info, R);

  // Take care of some other rewriting tasks
  std::set<llvm::FoldingSetNodeID> Seen;
  std::map<llvm::FoldingSetNodeID, AnnotationNeeded> NodeMap;
  CheckedRegionFinder CRF(&Context, R, Info, Seen, NodeMap, WarnRootCause);
  CheckedRegionAdder CRA(&Context, R, NodeMap);
  CastLocatorVisitor CLV(&Context);
  CastPlacementVisitor ECPV(&Context, Info, R, CLV.getExprsWithCast());
  TypeExprRewriter TER(&Context, Info, R);
  TypeArgumentAdder TPA(&Context, Info, R);
  TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
  for (const auto &D : TUD->decls()) {
    if (AddCheckedRegions) {
      // Adding checked regions enabled?
      // TODO: Should checked region finding happen somewhere else? This is
      //       supposed to be rewriting.
      CRF.TraverseDecl(D);
      CRA.TraverseDecl(D);
    }
    TER.TraverseDecl(D);
    // Cast placement must happen after type expression rewriting (i.e. cast and
    // compound literal) so that casts to unchecked pointer on itype function
    // calls can override rewritings of casts to checked types.
    // The cast locator must also run before the cast placement visitor so that
    // the cast placement visitor is aware of all existing cast expressions.
    CLV.TraverseDecl(D);
    ECPV.TraverseDecl(D);
    TPA.TraverseDecl(D);
  }

  // Output files.
  emit(R, Context);

  Info.getPerfStats().endRewritingTime();

  Info.exitCompilationUnit();
  return;
}
