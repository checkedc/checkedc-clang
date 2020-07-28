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
    if (!Dr.fullDecl)
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
  DeclStmt *St = dyn_cast_or_null<DeclStmt>(Lhs.Statement);

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

  DeclStmt *RhStmt = dyn_cast_or_null<DeclStmt>(Rhs.Statement);
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
  if (VD && globVarGroups.find(VD) == globVarGroups.end()) {
    if (VDSet == nullptr)
      VDSet = new std::set<VarDecl *>();
    VDSet->insert(VD);
    globVarGroups[VD] = VDSet;
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
  assert (globVarGroups.find(VD) != globVarGroups.end() &&
         "Expected to find the group.");
  return *(globVarGroups[VD]);
}

GlobalVariableGroups::~GlobalVariableGroups() {
  std::set<std::set<VarDecl *> *> Visited;
  // Free each of the group.
  for (auto &currV : globVarGroups) {
    // Avoid double free by caching deleted sets.
    if (Visited.find(currV.second) != Visited.end())
      continue;
    Visited.insert(currV.second);
    delete (currV.second);
  }
  globVarGroups.clear();
}

// Test to see if we can rewrite a given SourceRange.
// Note that R.getRangeSize will return -1 if SR is within
// a macro as well. This means that we can't re-write any
// text that occurs within a macro.
bool canRewrite(Rewriter &R, SourceRange &SR) {
  return SR.isValid() && (R.getRangeSize(SR) != -1);
}

std::string TypeRewritingVisitor::getExistingIType(ConstraintVariable *DeclC) {
  std::string Ret = "";
  ConstraintVariable *T = DeclC;
  if (PVConstraint *PVC = dyn_cast<PVConstraint>(T)) {
    if (PVC->hasItype()) {
      Ret = " : " + PVC->getItype();
    }
  }
  return Ret;
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
  auto isStatic = FD->isStatic();

  

  auto &CS = Info.getConstraints();

  // Do we have a definition for this function?
  FunctionDecl *Definition = getDefinition(FD);
  if (Definition == nullptr)
    Definition = FD;

  // Make sure we haven't visited this function name before, and that we
  // only visit it once.
  if (VisitedSet.find(FuncName) != VisitedSet.end())
    return true;
  else
    VisitedSet.insert(FuncName);

  auto &DefFVars = *(Info.getFuncConstraints(Definition, Context));
  FVConstraint *Defnc = getOnly(DefFVars);
  assert(Defnc != nullptr);

  // If this is an external function. The no need to rewrite this declaration.
  // Because, we cannot and should not change the signature of
  // external functions.
  if (!Defnc->hasBody()) {
    return true;
  }

  bool DidAny = Defnc->numParams() > 0;
  std::string s = "";
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
        // If the definition already has itype or there is no
        // argument which is WILD.
        if (Defn->hasItype() ||
            !Defn->anyArgumentIsWild(CS.getVariables())) {
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
          std::string bi =
              Defn->getRewritableOriginalTy() + Defn->getName() + " : itype(" +
                  PtypeS + ")" +
                  ABRewriter.getBoundsString(Defn,
                                         Definition->getParamDecl(i), true);
          ParmStrs.push_back(bi);
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
  auto Defn = dyn_cast<PVConstraint>(getOnly(Defnc->getReturnVars()));

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
    if (!EndStuff.empty()) {
      DidAny = true;
    }
  }

  s = getStorageQualifierString(Definition) + ReturnVar + Defnc->getName() +
      "(";
  if (ParmStrs.size() > 0) {
    std::ostringstream ss;

    std::copy(ParmStrs.begin(), ParmStrs.end() - 1,
              std::ostream_iterator<std::string>(ss, ", "));
    ss << ParmStrs.back();

    s = s + ss.str();
    // Add varargs.
    if (functionHasVarArgs(Definition)) {
      s = s + ", ...";
    }
    s = s + ")";
  } else {
    s = s + "void)";
    QualType ReturnTy = FD->getReturnType();
    QualType Ty = FD->getType();
    if (!Ty->isFunctionProtoType() && ReturnTy->isPointerType())
      DidAny = true;
  }

  if (EndStuff.size() > 0)
    s = s + EndStuff;

  if (DidAny) {
    // Do all of the declarations.
    for (auto *const RD : Definition->redecls())
      rewriteThese.insert(DAndReplace(RD, s, true));
    // Save the modified function signature.
    if(isStatic) { 
  	auto psl = PersistentSourceLoc::mkPSL(FD, *Context);
	auto fileName = psl.getFileName();
  	auto qualifiedName = fileName + "::" + FuncName;
    	ModifiedFuncSignatures[qualifiedName] = s;
    } else {
    	ModifiedFuncSignatures[FuncName] = s;
    }
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




// Visit every array compound literal expression (i.e., (int*[2]){&a, &b}) and
// replace the type name with a new type based on constraint solution.
class CompoundLiteralRewriter
    : public clang::RecursiveASTVisitor<CompoundLiteralRewriter> {
public:
  explicit CompoundLiteralRewriter(ASTContext *C, ProgramInfo &I, Rewriter &R)
      : Context(C), Info(I) , Writer(R) {}

  bool VisitCompoundLiteralExpr(CompoundLiteralExpr *CLE) {
    // When an compound literal was visited in constraint generation, a
    // constraint variable for it was stored in program info.  There should be
    // either zero or one of these.
    CVarSet CVSingleton = Info.getPersistentConstraintVars(CLE, Context);
    if (CVSingleton.empty())
      return true;
    ConstraintVariable *CV = getOnly(CVSingleton);

    // Only rewrite if the type has changed.
    if(CV->anyChanges(Info.getConstraints().getVariables())){
      // The constraint variable is able to tell us what the new type string
      // should be.
      std::string NewType = CV->mkString(Info.getConstraints().getVariables(),false);

      // Replace the original type with this new one
      SourceRange *TypeSrcRange =
          new SourceRange(CLE->getBeginLoc().getLocWithOffset(1),
                          CLE->getTypeSourceInfo()->getTypeLoc().getEndLoc());

      if(canRewrite(Writer, *TypeSrcRange))
        Writer.ReplaceText(*TypeSrcRange, NewType);
    }
    return true;
  }

private:
  ASTContext *Context;
  ProgramInfo &Info;
  Rewriter &Writer;
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

std::map<std::string, std::string> RewriteConsumer::ModifiedFuncSignatures;

std::string RewriteConsumer::getModifiedFuncSignature(std::string FuncName) {
  if (RewriteConsumer::ModifiedFuncSignatures.find(FuncName) !=
      RewriteConsumer::ModifiedFuncSignatures.end()) {
    return RewriteConsumer::ModifiedFuncSignatures[FuncName];
  }
  return "";
}

bool RewriteConsumer::hasModifiedSignature(std::string FuncName) {
  return RewriteConsumer::ModifiedFuncSignatures.find(FuncName) !=
         RewriteConsumer::ModifiedFuncSignatures.end();
}

std::string ArrayBoundsRewriter::getBoundsString(PVConstraint *PV,
                                                 Decl *D, bool Isitype) {
  std::string BString = "";
  std::string BVarString = "";
  auto &ABInfo = Info.getABoundsInfo();
  BoundsKey DK;
  bool ValidBKey = true;
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
      if (!BString.empty()) {
        // For itype we do not need ":".
        BString = Pfix + BString;
      }
    }
  }
  if (BString.empty() && PV->hasBoundsStr()) {
    BString = Pfix + PV->getBoundsStr();
  }
  return BString;
}

void RewriteConsumer::HandleTranslationUnit(ASTContext &Context) {
  Info.enterCompilationUnit(Context);

  // Compute the bounds information for all the array variables.
  ArrayBoundsRewriter ABRewriter(&Context, Info);

  Rewriter R(Context.getSourceManager(), Context.getLangOpts());
  std::set<FileID> Files;

  std::set<std::string> v;
  RSet RewriteThese(DComp(Context.getSourceManager()));
  // Unification is done, so visit and see if we need to place any casts
  // in the program.
  TypeRewritingVisitor TRV = TypeRewritingVisitor(&Context, Info, RewriteThese, v,
                                                  RewriteConsumer::
                                                      ModifiedFuncSignatures,
                                                  ABRewriter);
  for (const auto &D : Context.getTranslationUnitDecl()->decls())
    TRV.TraverseDecl(D);

  // Build a map of all of the PersistentSourceLoc's back to some kind of
  // Stmt, Decl, or Type.
  VariableMap &VarMap = Info.getVarMap();
  std::set<PersistentSourceLoc> keys;

  for (const auto &I : VarMap)
    keys.insert(I.first);
  SourceToDeclMapType PSLMap;
  VariableDecltoStmtMap VDLToStmtMap;

  MappingVisitor MV(keys, Context);
  TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
  StructVariableInitializer SVI =
      StructVariableInitializer(&Context, Info, RewriteThese);
  GlobalVariableGroups GVG(R.getSourceMgr());
  std::set<llvm::FoldingSetNodeID> seen;
  std::map<llvm::FoldingSetNodeID, AnnotationNeeded> nodeMap;
  CheckedRegionFinder CRF(&Context, R, Info, seen, nodeMap);
  CheckedRegionAdder CRA(&Context, R, nodeMap);
  CastPlacementVisitor ECPV(&Context, Info, R);
  CompoundLiteralRewriter CLR(&Context, Info, R);
  TypeArgumentAdder TPA(&Context, Info, R);
  for (const auto &D : TUD->decls()) {
    MV.TraverseDecl(D);
    SVI.TraverseDecl(D);
    ECPV.TraverseDecl(D);
    if (AddCheckedRegions) {
      // Adding checked regions enabled!?
      CRF.TraverseDecl(D);
      CRA.TraverseDecl(D);
    }

    GVG.addGlobalDecl(dyn_cast<VarDecl>(D));
    CLR.TraverseDecl(D);
    TPA.TraverseDecl(D);
  }

  std::tie(PSLMap, VDLToStmtMap) = MV.getResults();

  for (const auto &V : Info.getVarMap()) {
    PersistentSourceLoc PLoc = V.first;
    CVarSet Vars = V.second;
    // I don't think it's important that Vars have any especial size, but
    // at one point I did so I'm keeping this comment here. It's possible
    // that what we really need to do is to ensure that when we work with
    // either PV or FV below, that they are the LUB of what is in Vars.
    // assert(Vars.size() > 0 && Vars.size() <= 2);

    // PLoc specifies the location of the variable whose type it is to
    // re-write, but not where the actual type storage is. To get that, we
    // need to turn PLoc into a Decl and then get the SourceRange for the
    // type of the Decl. Note that what we need to get is the ExpansionLoc
    // of the type specifier, since we want where the text is printed before
    // the variable name, not the typedef or #define that creates the
    // name of the type.

    Stmt *S = nullptr;
    Decl *D = nullptr;
    DeclStmt *DS = nullptr;

    std::tie(S, D) = PSLMap[PLoc];

    if (D) {
      // We might have one Decl for multiple Vars, however, one will be a
      // PointerVar so we'll use that.
      VariableDecltoStmtMap::iterator K = VDLToStmtMap.find(D);
      if (K != VDLToStmtMap.end())
        DS = K->second;

      PVConstraint *PV = nullptr;
      FVConstraint *FV = nullptr;
      for (const auto &V : Vars) {
        if (PVConstraint *T = dyn_cast<PVConstraint>(V))
          PV = T;
        else if (FVConstraint *T = dyn_cast<FVConstraint>(V))
          FV = T;
      }


      if (PV && PV->anyChanges(Info.getConstraints().getVariables()) &&
          !PV->isPartOfFunctionPrototype()) {
        // Rewrite a declaration, only if it is not part of function prototype.
        std::string newTy = getStorageQualifierString(D) +
                            PV->mkString(Info.getConstraints().getVariables()) +
                            ABRewriter.getBoundsString(PV, D);
        RewriteThese.insert(DAndReplace(D, DS, newTy));
      } else if (FV && RewriteConsumer::hasModifiedSignature(FV->getName()) &&
                 !TRV.isFunctionVisited(FV->getName())) {
        // If this function already has a modified signature? and it is not
        // visited by our cast placement visitor then rewrite it.
        std::string newSig =
            RewriteConsumer::getModifiedFuncSignature(FV->getName());
        RewriteThese.insert(DAndReplace(D, newSig, true));
      }
    }
  }

  DeclRewriter DeclR(R, Context, GVG);
  RSet Skip(DComp(Context.getSourceManager()));
  DeclR.rewrite(RewriteThese, Skip, Files);

  // Output files.
  emit(R, Context, Files, OutputPostfix);

  Info.exitCompilationUnit();
  return;
}