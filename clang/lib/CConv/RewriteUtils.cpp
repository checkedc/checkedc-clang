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

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/CConv/CastPlacement.h"
#include "clang/CConv/CCGlobalOptions.h"
#include "clang/CConv/CheckedRegions.h"
#include "clang/CConv/ConstraintResolver.h"
#include "clang/CConv/DeclRewriter.h"
#include "clang/CConv/MappingVisitor.h"
#include "clang/CConv/RewriteUtils.h"
#include "clang/CConv/StructInit.h"
#include "clang/CConv/Utils.h"
#include "clang/Tooling/Refactoring/SourceCode.h"
#include "llvm/Support/raw_ostream.h"
#include <sstream>

using namespace llvm;
using namespace clang;

SourceRange DComp::getWholeSR(SourceRange Orig, DAndReplace Dr) const {
  SourceRange newSourceRange(Orig);

  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(Dr.Declaration)) {
    newSourceRange.setEnd(getFunctionDeclarationEnd(FD, SM));
    if (!Dr.FullDecl)
      newSourceRange = FD->getReturnTypeSourceRange();
  }

  return newSourceRange;
}

bool DComp::operator()(const DAndReplace Lhs, const DAndReplace Rhs) const {
  // Does the source location of the Decl in lhs overlap at all with
  // the source location of rhs?
  SourceRange SrLhs = Lhs.Declaration->getSourceRange();
  SourceRange SrRhs = Rhs.Declaration->getSourceRange();

  // Take into account whether or not a FunctionDeclaration specifies
  // the "whole" declaration or not. If it does not, it just specifies
  // the return position.
  SrLhs = getWholeSR(SrLhs, Lhs);
  SrRhs = getWholeSR(SrRhs, Rhs);

  // Also take into account whether or not there is a multi-statement
  // decl, because the generated ranges will overlap.
  DeclStmt *St = Lhs.Statement;

  if (St && !St->isSingleDecl()) {
    SourceLocation NewBegin =
        (*St->decls().begin())->getSourceRange().getBegin();
    bool Found;
    for (const auto &DT : St->decls()) {
      if (DT == Lhs.Declaration) {
        Found = true;
        break;
      }
      NewBegin = DT->getSourceRange().getEnd();
    }
    assert (Found);
    SrLhs.setBegin(NewBegin);
    // This is needed to make the subsequent test inclusive.
    SrLhs.setEnd(SrLhs.getEnd().getLocWithOffset(-1));
  }

  DeclStmt *RhStmt = Rhs.Statement;
  if (RhStmt && !RhStmt->isSingleDecl()) {
    SourceLocation NewBegin =
        (*RhStmt->decls().begin())->getSourceRange().getBegin();
    bool Found;
    for (const auto &DT : RhStmt->decls()) {
      if (DT == Rhs.Declaration) {
        Found = true;
        break;
      }
      NewBegin = DT->getSourceRange().getEnd();
    }
    assert (Found);
    SrRhs.setBegin(NewBegin);
    // This is needed to make the subsequent test inclusive.
    SrRhs.setEnd(SrRhs.getEnd().getLocWithOffset(-1));
  }

  SourceLocation X1 = SrLhs.getBegin();
  SourceLocation X2 = SrLhs.getEnd();
  SourceLocation Y1 = SrRhs.getBegin();
  SourceLocation Y2 = SrRhs.getEnd();

  if (St == nullptr && RhStmt == nullptr) {
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

  bool Contained =  SM.isBeforeInTranslationUnit(X1, Y2) &&
                    SM.isBeforeInTranslationUnit(Y1, X2);

  if (Contained)
    return false;
  else
    return SM.isBeforeInTranslationUnit(X2, Y1);
}

void GlobalVariableGroups::addGlobalDecl(VarDecl *VD,
                                         std::set<VarDecl *> *VDSet) {
  if (VD && GlobVarGroups.find(VD) == GlobVarGroups.end()) {
    if (VDSet == nullptr)
      VDSet = new std::set<VarDecl *>();
    VDSet->insert(VD);
    GlobVarGroups[VD] = VDSet;
    // Process the next decl.
    Decl *NDecl = VD->getNextDeclInContext();
    if (isa_and_nonnull<VarDecl>(NDecl)) {
      PresumedLoc OrigDeclLoc =
          SM.getPresumedLoc(VD->getSourceRange().getBegin());
      PresumedLoc NewDeclLoc =
          SM.getPresumedLoc(NDecl->getSourceRange().getBegin());
      // Check if both declarations are on the same line.
      if (OrigDeclLoc.isValid() && NewDeclLoc.isValid() &&
          !strcmp(OrigDeclLoc.getFilename(), NewDeclLoc.getFilename()) &&
          OrigDeclLoc.getLine() == NewDeclLoc.getLine())
        addGlobalDecl(dyn_cast<VarDecl>(NDecl), VDSet);
    }
  }
}

std::set<VarDecl *> &GlobalVariableGroups::getVarsOnSameLine(VarDecl *VD) {
  assert (GlobVarGroups.find(VD) != GlobVarGroups.end() &&
         "Expected to find the group.");
  return *(GlobVarGroups[VD]);
}

GlobalVariableGroups::~GlobalVariableGroups() {
  std::set<std::set<VarDecl *> *> Visited;
  // Free each of the group.
  for (auto &currV : GlobVarGroups) {
    // Avoid double free by caching deleted sets.
    if (Visited.find(currV.second) != Visited.end())
      continue;
    Visited.insert(currV.second);
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

std::string TypeRewritingVisitor::getExistingIType(ConstraintVariable *DeclC) {
  auto *PVC = dyn_cast<PVConstraint>(DeclC);
  if (PVC != nullptr && PVC->hasItype())
    return " : " + PVC->getItype();
  return "";
}

// This function checks how to re-write a function declaration.
bool TypeRewritingVisitor::VisitFunctionDecl(FunctionDecl *FD) {

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
  auto &CS = Info.getConstraints();

  // Do we have a definition for this function?
  FunctionDecl *Definition = getDefinition(FD);
  if (Definition == nullptr)
    Definition = FD;

  // Make sure we haven't visited this function name before, and that we
  // only visit it once.
  if (isFunctionVisited(FuncName))
    return true;
  else
    VisitedSet.insert(FuncName);

  auto &DefFVars = *(Info.getFuncConstraints(Definition, Context));
  FVConstraint *Defnc = getOnly(DefFVars);
  assert(Defnc != nullptr);

  // If this is an external function. The no need to rewrite this declaration.
  // Because, we cannot and should not change the signature of
  // external functions.
  if (!Defnc->hasBody())
    return true;

  bool DidAny = Defnc->numParams() > 0;
  std::string NewSig = "";
  std::vector<std::string> ParmStrs;
  // Compare parameters.
  for (unsigned i = 0; i < Defnc->numParams(); ++i) {
    auto *Defn = dyn_cast<PVConstraint>(getOnly(Defnc->getParamVar(i)));
    assert(Defn);
    bool ParameterHandled = false;

    if (isAValidPVConstraint(Defn)) {
      // If this holds, then we want to insert a bounds safe interface.
      bool Constrained = Defn->anyChanges(CS.getVariables());
      if (Constrained) {
        // If the definition already has itype or there are no WILD arguments.
        if (Defn->hasItype() || !Defn->anyArgumentIsWild(CS.getVariables())) {
          // Here we should emit a checked type, with an itype (if exists)
          std::string PtypeS =
              Defn->mkString(Info.getConstraints().getVariables());

          // If there is no declaration?
          // check the itype in definition.
          PtypeS = PtypeS + getExistingIType(Defn) +
              ABRewriter.getBoundsString(Defn, Definition->getParamDecl(i));

          ParmStrs.push_back(PtypeS);
        } else {
          // Here, definition is checked type but at least one of the arguments
          // is WILD.
          std::string PtypeS =
              Defn->mkString(Info.getConstraints().getVariables(), false, true);
          std::string Bi =
              Defn->getRewritableOriginalTy() + Defn->getName() + " : itype(" +
                  PtypeS + ")" +
                  ABRewriter.getBoundsString(Defn,
                                         Definition->getParamDecl(i), true);
          ParmStrs.push_back(Bi);
        }
        ParameterHandled = true;
      }
    }
    // If the parameter has no changes? Just dump the original declaration.
    if (!ParameterHandled) {
      std::string Scratch = "";
      raw_string_ostream DeclText(Scratch);
      Definition->getParamDecl(i)->print(DeclText);
      ParmStrs.push_back(DeclText.str());
    }
  }

  // Compare returns.
  auto *Defn = dyn_cast<PVConstraint>(getOnly(Defnc->getReturnVars()));

  std::string ReturnVar = "";
  std::string EndStuff = "";
  bool ReturnHandled = false;

  if (isAValidPVConstraint(Defn)) {
    // Insert a bounds safe interface for the return.
    bool anyConstrained = Defn->anyChanges(CS.getVariables());
    if (anyConstrained) {
      // This means we were able to infer that return type
      // is a checked type.
      ReturnHandled = true;
      DidAny = true;
      std::string Ctype = "";
      // If the definition has itype or there is no argument which is WILD?
      if (Defn->hasItype() ||
          !Defn->anyArgumentIsWild(CS.getVariables())) {
        // Just get the checked itype
        ReturnVar = Defn->mkString(Info.getConstraints().getVariables());
        EndStuff = getExistingIType(Defn);
      } else {
        // One of the argument is WILD, emit an itype.
        Ctype =
            Defn->mkString(Info.getConstraints().getVariables(), true, true);
        ReturnVar = Defn->getRewritableOriginalTy();
        EndStuff = " : itype(" + Ctype + ")";
      }
    }
  }

  // This means inside the function, the return value is WILD
  // so the return type is what was originally declared.
  if (!ReturnHandled) {
    // If we used to implement a bounds-safe interface, continue to do that.
    ReturnVar = Defn->getOriginalTy() + " ";
    EndStuff = getExistingIType(Defn);
    if (!EndStuff.empty())
      DidAny = true;
  }

  NewSig = getStorageQualifierString(Definition) + ReturnVar + Defnc->getName()
      + "(";
  if (!ParmStrs.empty()) {
    // Gather individual parameter strings into a single buffer
    std::ostringstream ConcatParamStr;
    std::copy(ParmStrs.begin(), ParmStrs.end() - 1,
              std::ostream_iterator<std::string>(ConcatParamStr, ", "));
    ConcatParamStr << ParmStrs.back();

    NewSig = NewSig + ConcatParamStr.str();
    // Add varargs.
    if (functionHasVarArgs(Definition))
      NewSig = NewSig + ", ...";
    NewSig = NewSig + ")";
  } else {
    NewSig = NewSig + "void)";
    QualType ReturnTy = FD->getReturnType();
    QualType Ty = FD->getType();
    if (!Ty->isFunctionProtoType() && ReturnTy->isPointerType())
      DidAny = true;
  }

  if (!EndStuff.empty())
    NewSig = NewSig + EndStuff;

  if (DidAny) {
    // Do all of the declarations.
    for (auto *const RD : Definition->redecls())
      RewriteThese.insert(DAndReplace(RD, NewSig, true));
    // Save the modified function signature.
    if(FD->isStatic()) {
      auto FileName = PersistentSourceLoc::mkPSL(FD, *Context).getFileName();
	  FuncName = FileName + "::" + FuncName;
    }
    ModifiedFuncSignatures[FuncName] = NewSig;
  }

  return true;
}

// Check if the function is handled by this visitor.
bool TypeRewritingVisitor::isFunctionVisited(std::string FuncName) {
  return VisitedSet.find(FuncName) != VisitedSet.end();
}

static void emit(Rewriter &R, ASTContext &C, std::set<FileID> &Files,
                 std::string &OutputPostfix) {

  // Check if we are outputing to stdout or not, if we are, just output the
  // main file ID to stdout.
  if (Verbose)
    errs() << "Writing files out\n";

  SmallString<254> BaseAbs(BaseDir);
  std::string BaseDirFp;
  if (getAbsoluteFilePath(BaseDir, BaseDirFp)) {
    BaseAbs = BaseDirFp;
  }
  sys::path::remove_filename(BaseAbs);
  std::string base = BaseAbs.str();

  SourceManager &SM = C.getSourceManager();
  if (OutputPostfix == "-") {
    if (const RewriteBuffer *B = R.getRewriteBufferFor(SM.getMainFileID()))
      B->write(outs());
  } else
    for (const auto &F : Files)
      if (const RewriteBuffer *B = R.getRewriteBufferFor(F))
        if (const FileEntry *FE = SM.getFileEntryForID(F)) {
          assert(FE->isValid());

          // Produce a path/file name for the rewritten source file.
          // That path should be the same as the old one, with a
          // suffix added between the file name and the extension.
          // For example \foo\bar\a.c should become \foo\bar\a.checked.c
          // if the OutputPostfix parameter is "checked" .

          std::string pfName = sys::path::filename(FE->getName()).str();
          std::string dirName = sys::path::parent_path(FE->getName()).str();
          std::string fileName = sys::path::remove_leading_dotslash(pfName).str();
          std::string ext = sys::path::extension(fileName).str();
          std::string stem = sys::path::stem(fileName).str();
          std::string nFileName = stem + "." + OutputPostfix + ext;
          std::string nFile = nFileName;
          if (dirName.size() > 0)
            nFile = dirName + sys::path::get_separator().str() + nFileName;

          // Write this file out if it was specified as a file on the command
          // line.
          std::string feAbsS = "";
          if (getAbsoluteFilePath(FE->getName(), feAbsS)) {
            feAbsS = sys::path::remove_leading_dotslash(feAbsS);
          }

          if (canWrite(feAbsS)) {
            std::error_code EC;
            raw_fd_ostream out(nFile, EC, sys::fs::F_None);

            if (!EC) {
              if (Verbose)
                outs() << "writing out " << nFile << "\n";
              B->write(out);
            }
            else
              errs() << "could not open file " << nFile << "\n";
            // This is awkward. What to do? Since we're iterating,
            // we could have created other files successfully. Do we go back
            // and erase them? Is that surprising? For now, let's just keep
            // going.
          }
        }
}




// Rewrites types that inside other expressions. This includes cast expression
// and compound literal expressions.
class TypeExprRewriter
    : public clang::RecursiveASTVisitor<TypeExprRewriter> {
public:
  explicit TypeExprRewriter(ASTContext *C, ProgramInfo &I, Rewriter &R)
      : Context(C), Info(I) , Writer(R) {}

  bool VisitCompoundLiteralExpr(CompoundLiteralExpr *CLE) {
    // When an compound literal was visited in constraint generation, a
    // constraint variable for it was stored in program info.  There should be
    // either zero or one of these.
    CVarSet CVSingleton = Info.getPersistentConstraintVars(CLE, Context);
    SourceRange TypeSrcRange(CLE->getBeginLoc().getLocWithOffset(1),
         CLE->getTypeSourceInfo()->getTypeLoc().getEndLoc());
    rewriteType(CLE, TypeSrcRange);
    return true;
  }

  bool VisitCStyleCastExpr(CStyleCastExpr *ECE) {
    SourceRange TypeSrcRange
        (ECE->getBeginLoc().getLocWithOffset(1),
          ECE->getTypeInfoAsWritten()->getTypeLoc().getEndLoc());
    rewriteType(ECE, TypeSrcRange);
    return true;
  }

private:
  ASTContext *Context;
  ProgramInfo &Info;
  Rewriter &Writer;

  void rewriteType(Expr *E, SourceRange &Range) {
    CVarSet CVSingleton = Info.getPersistentConstraintVars(E, Context);
    if (CVSingleton.empty())
      return;
    ConstraintVariable *CV = getOnly(CVSingleton);

    // Only rewrite if the type has changed.
    if (CV->anyChanges(Info.getConstraints().getVariables())){
      // The constraint variable is able to tell us what the new type string
      // should be.
      std::string
          NewType = CV->mkString(Info.getConstraints().getVariables(), false);

      // Replace the original type with this new one
      if (canRewrite(Writer, Range))
        Writer.ReplaceText(Range, NewType);
    }
  }
};

// Adds type parameters to calls to alloc functions.
// The basic assumption this makes is that an alloc function will be surrounded
// by a cast expression giving its type when used as a type other than void*.
class TypeArgumentAdder
  : public clang::RecursiveASTVisitor<TypeArgumentAdder> {
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
            std::string TyStr =
                Entry.second->mkString(Info.getConstraints().getVariables(),
                                     false, false, true);
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
      // ArgInfo is null if there are no type arguments in the program
      if (auto *ArgInfo = DRE->GetTypeArgumentInfo())
        for (auto TypeArg : ArgInfo->typeArgumentss())
          if (!TypeArg.typeName->isVoidType())
            return true;
      return false;
    }
    // We only handle direct calls, so there must be a DeclRefExpr.
    llvm_unreachable("Callee of function call is not DeclRefExpr.");
  }
};


std::string ArrayBoundsRewriter::getBoundsString(PVConstraint *PV,
                                                 Decl *D, bool Isitype) {
  std::string BString = "";
  std::string BVarString = "";
  auto &ABInfo = Info.getABoundsInfo();
  BoundsKey DK;
  bool ValidBKey = true;
  // For itype we do not need ":".
  std::string Pfix = Isitype ? " " : " : ";
  if (PV->hasBoundsKey()) {
    DK = PV->getBoundsKey();
  } else if(!ABInfo.tryGetVariable(D, DK)){
    ValidBKey = false;
  }
  if (ValidBKey) {
    ABounds *ArrB = ABInfo.getBounds(DK);
    if (ArrB != nullptr) {
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

void RewriteConsumer::HandleTranslationUnit(ASTContext &Context) {
  Info.enterCompilationUnit(Context);

  // Rewrite Variable declarations
  Rewriter R(Context.getSourceManager(), Context.getLangOpts());
  std::set<FileID> TouchedFiles;
  DeclRewriter::rewriteDecls(Context, Info, R, TouchedFiles);

  // Take care of some other rewriting tasks
  std::set<llvm::FoldingSetNodeID> Seen;
  std::map<llvm::FoldingSetNodeID, AnnotationNeeded> NodeMap;
  CheckedRegionFinder CRF(&Context, R, Info, Seen, NodeMap);
  CheckedRegionAdder CRA(&Context, R, NodeMap);
  CastPlacementVisitor ECPV(&Context, Info, R);
  TypeExprRewriter TER(&Context, Info, R);
  TypeArgumentAdder TPA(&Context, Info, R);
  TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
  for (const auto &D : TUD->decls()) {
    ECPV.TraverseDecl(D);
    if (AddCheckedRegions) {
      // Adding checked regions enabled!?
      // TODO: Should checked region finding happen somewhere else? This is
      //       supposed to be rewriting.
      CRF.TraverseDecl(D);
      CRA.TraverseDecl(D);
    }
    TER.TraverseDecl(D);
    TPA.TraverseDecl(D);
  }

  // Output files.
  emit(R, Context, TouchedFiles, OutputPostfix);

  Info.exitCompilationUnit();
  return;
}