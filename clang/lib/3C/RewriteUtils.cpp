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

#include "clang/3C/3CGlobalOptions.h"
#include "clang/3C/CastPlacement.h"
#include "clang/3C/CheckedRegions.h"
#include "clang/3C/DeclRewriter.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Tooling/Refactoring/SourceCode.h"

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
  else
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
  for (auto &currV : GlobVarGroups) {
    // Avoid double free by caching deleted sets.
    if (VVisited.find(currV.second) != VVisited.end())
      continue;
    VVisited.insert(currV.second);
    delete (currV.second);
  }
  GlobVarGroups.clear();
}

// Test to see if we can rewrite a given SourceRange.
// Note that R.getRangeSize will return -1 if SR is within
// a macro as well. This means that we can't re-write any
// text that occurs within a macro.
bool canRewrite(Rewriter &R, SourceRange &SR) {
  return SR.isValid() && (R.getRangeSize(SR) != -1);
}

static void emit(Rewriter &R, ASTContext &C, std::string &OutputPostfix) {
  if (Verbose)
    errs() << "Writing files out\n";

  // Check if we are outputing to stdout or not, if we are, just output the
  // main file ID to stdout.
  SourceManager &SM = C.getSourceManager();
  if (OutputPostfix == "-") {
    if (const RewriteBuffer *B = R.getRewriteBufferFor(SM.getMainFileID()))
      B->write(outs());
  } else {
    // Iterate over each modified rewrite buffer
    for (auto Buffer = R.buffer_begin(); Buffer != R.buffer_end(); ++Buffer) {
      if (const FileEntry *FE = SM.getFileEntryForID(Buffer->first)) {
        assert(FE->isValid());

        // Produce a path/file name for the rewritten source file.
        // That path should be the same as the old one, with a
        // suffix added between the file name and the extension.
        // For example \foo\bar\a.c should become \foo\bar\a.checked.c
        // if the OutputPostfix parameter is "checked" .
        std::string PfName = sys::path::filename(FE->getName()).str();
        std::string DirName = sys::path::parent_path(FE->getName()).str();
        std::string FileName = sys::path::remove_leading_dotslash(PfName).str();
        std::string Ext = sys::path::extension(FileName).str();
        std::string Stem = sys::path::stem(FileName).str();
        std::string NFile = Stem + "." + OutputPostfix + Ext;
        if (!DirName.empty())
          NFile = DirName + sys::path::get_separator().str() + NFile;

        // Write this file if it was specified as a file on the command line.
        std::string FeAbsS = "";
        if (getAbsoluteFilePath(FE->getName(), FeAbsS))
          FeAbsS = sys::path::remove_leading_dotslash(FeAbsS);

        if (canWrite(FeAbsS)) {
          std::error_code EC;
          raw_fd_ostream Out(NFile, EC, sys::fs::F_None);

          if (!EC) {
            if (Verbose)
              outs() << "writing out " << NFile << "\n";
            Buffer->second.write(Out);
          } else
            errs() << "could not open file " << NFile << "\n";
          // This is awkward. What to do? Since we're iterating, we could have
          // created other files successfully. Do we go back and erase them? Is
          // that surprising? For now, let's just keep going.
        }
      }
    }
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
    const CVarSet &CVSingleton = Info.getPersistentConstraints(E, Context);
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
      // Only rewrite if the type has changed.
      if (CV->anyChanges(Vars)) {
        // Replace the original type with this new one
        if (canRewrite(Writer, Range))
          Writer.ReplaceText(Range, CV->mkString(Vars, false));
      }
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
        for (auto Entry : Info.getTypeParamBindings(CE, Context))
          if (Entry.second != nullptr) {
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

        SourceLocation TypeParamLoc = getTypeArgLocation(CE);
        Writer.InsertTextAfter(TypeParamLoc, "<" + TypeParamString + ">");
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

  // Check if type arguments have already been provided for this function
  // call so that we don't mess with anything already there.
  bool typeArgsProvided(CallExpr *Call) {
    Expr *Callee = Call->getCallee()->IgnoreImpCasts();
    if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(Callee)) {
      // ArgInfo is null if there are no type arguments anywhere in the program
      if (auto *ArgInfo = DRE->GetTypeArgumentInfo())
        for (auto Arg : ArgInfo->typeArgumentss()) {
          if (!Arg.typeName->isVoidType()) {
            // Found a non-void type argument. No doubt type args are provided.
            return true;
          } else if (Arg.sourceInfo->getTypeLoc().getSourceRange().isValid()) {
            // The type argument is void, but with a valid source range. This
            // means an explict void type argument was provided.
            return true;
          }
          // A void type argument without a source location. The type argument
          // is implicit so, we're good to insert a new one.
        }
      return false;
    }
    // We only handle direct calls, so there must be a DeclRefExpr.
    llvm_unreachable("Callee of function call is not DeclRefExpr.");
  }
};

std::string ArrayBoundsRewriter::getBoundsString(PVConstraint *PV, Decl *D,
                                                 bool Isitype) {
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
  if (BString.empty() && PV->hasBoundsStr()) {
    BString = Pfix + PV->getBoundsStr();
  }
  return BString;
}

bool ArrayBoundsRewriter::hasNewBoundsString(PVConstraint *PV, Decl *D,
                                             bool Isitype) {
  std::string BStr = getBoundsString(PV, D, Isitype);
  // There is a bounds string but has nothing declared?
  return !BStr.empty() && !PV->hasBoundsStr();
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
      const auto *File = SM.getFileManager().getFile(PSL.getFileName());
      if (File != nullptr) {
        SourceLocation SL =
            SM.translateFileLineCol(File, PSL.getLineNo(), PSL.getColSNo());
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
  CastPlacementVisitor ECPV(&Context, Info, R);
  TypeExprRewriter TER(&Context, Info, R);
  TypeArgumentAdder TPA(&Context, Info, R);
  TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
  for (const auto &D : TUD->decls()) {
    if (AddCheckedRegions) {
      // Adding checked regions enabled!?
      // TODO: Should checked region finding happen somewhere else? This is
      //       supposed to be rewriting.
      CRF.TraverseDecl(D);
      CRA.TraverseDecl(D);
    }
    TER.TraverseDecl(D);
    // Cast placement must happen after type expression rewriting (i.e. cast and
    // compound literal) so that casts to unchecked pointer on itype function
    // calls can override rewritings of casts to checked types.
    ECPV.TraverseDecl(D);
    TPA.TraverseDecl(D);
  }

  // Output files.
  emit(R, Context, OutputPostfix);

  Info.exitCompilationUnit();
  return;
}
