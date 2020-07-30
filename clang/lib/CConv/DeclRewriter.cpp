//=--DeclRewriter.cpp---------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#include "clang/CConv/CCGlobalOptions.h"
#include "clang/CConv/RewriteUtils.h"
#include "clang/CConv/Utils.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/CConv/DeclRewriter.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/ASTContext.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include <sstream>
#include "clang/CConv/StructInit.h"
#include "clang/CConv/MappingVisitor.h"

using namespace llvm;
using namespace clang;

unsigned int DeclRewriter::getParameterIndex(ParmVarDecl *PV,
                                             FunctionDecl *FD) {
  // This is kind of hacky, maybe we should record the index of the
  // parameter when we find it, instead of re-discovering it here.
  unsigned int PIdx = 0;
  for (const auto &I : FD->parameters()) {
    if (I == PV)
      return PIdx;
    PIdx++;
  }
  llvm_unreachable("Parameter declaration not found in function declaration.");
}

void DeclRewriter::rewrite(ParmVarDecl *PV, std::string SRewrite) {
  // First, find all the declarations of the containing function.
  DeclContext *DF = PV->getParentFunctionOrMethod();
  assert(DF != nullptr && "no parent function or method for decl");
  FunctionDecl *FD = cast<FunctionDecl>(DF);

  // For each function, determine which parameter in the declaration
  // matches PV, then, get the type location of that parameter
  // declaration and re-write.
  unsigned int PIdx = getParameterIndex(PV, FD);

  for (auto *CurFD = FD; CurFD != nullptr; CurFD = CurFD->getPreviousDecl())
    if (PIdx < CurFD->getNumParams()) {
      // TODO these declarations could get us into deeper header files.
      ParmVarDecl *Rewrite = CurFD->getParamDecl(PIdx);
      assert(Rewrite != nullptr);
      SourceRange TR = Rewrite->getSourceRange();

      if (canRewrite(R, TR))
        R.ReplaceText(TR, SRewrite);
    }
}

bool DeclRewriter::areDeclarationsOnSameLine(VarDecl *VD1, DeclStmt *Stmt1,
                                             VarDecl *VD2, DeclStmt *Stmt2) {
  if (VD1 && VD2) {
    if (Stmt1 == nullptr && Stmt2 == nullptr) {
      auto &VDGroup = GP.getVarsOnSameLine(VD1);
      return VDGroup.find(VD2) != VDGroup.end();
    } else if (Stmt1 == nullptr || Stmt2 == nullptr) {
      return false;
    } else {
      return Stmt1 == Stmt2;
    }
  }
  return false;
}

bool DeclRewriter::isSingleDeclaration(VarDecl *VD, DeclStmt *Stmt) {
  if (Stmt == nullptr) {
    auto &VDGroup = GP.getVarsOnSameLine(VD);
    return VDGroup.size() == 1;
  } else {
    return Stmt->isSingleDecl();
  }
}

void DeclRewriter::getDeclsOnSameLine(VarDecl *VD, DeclStmt *Stmt,
                                      std::set<Decl *> &Decls) {
  if (Stmt != nullptr)
    Decls.insert(Stmt->decls().begin(), Stmt->decls().end());
  else
    Decls.insert(GP.getVarsOnSameLine(VD).begin(),
                 GP.getVarsOnSameLine(VD).end());
}

SourceLocation DeclRewriter::deleteAllDeclarationsOnLine(VarDecl *VD,
                                                         DeclStmt *Stmt) {
  if (Stmt != nullptr) {
    // If there is a statement, delete the entire statement.
    R.RemoveText(Stmt->getSourceRange());
    return Stmt->getSourceRange().getEnd();
  } else {
    SourceLocation BLoc;
    SourceManager &SM = R.getSourceMgr();
    // Remove all vars on the line.
    for (auto *D : GP.getVarsOnSameLine(VD)) {
      SourceRange ToDel = D->getSourceRange();
      if (BLoc.isInvalid() ||
          SM.isBeforeInTranslationUnit(ToDel.getBegin(), BLoc))
        BLoc = ToDel.getBegin();
      R.RemoveText(D->getSourceRange());
    }
    return BLoc;
  }
}

void DeclRewriter::rewrite(VarDecl *VD, std::string SRewrite, Stmt *WhereStmt,
                           const DAndReplace &N, RSet &ToRewrite) {
  DeclStmt *Where = dyn_cast_or_null<DeclStmt>(WhereStmt);

  if (Verbose) {
    errs() << "VarDecl at:\n";
    if (Where)
      Where->dump();
  }
  SourceRange TR = VD->getSourceRange();

  // Is there an initializer? If there is, change TR so that it points
  // to the START of the SourceRange of the initializer text, and drop
  // an '=' token into sRewrite.
  if (VD->hasInit()) {
    SourceLocation EqLoc = VD->getInitializerStartLoc();
    TR.setEnd(EqLoc);
    SRewrite = SRewrite + " = ";
  } else {
    // There is no initializer, lets add it.
    if (isPointerType(VD) &&
        (VD->getStorageClass() != StorageClass::SC_Extern))
      SRewrite = SRewrite + " = ((void *)0)";
    //MWH -- Solves issue 43. Should make it so we insert NULL if
    // stdlib.h or stdlib_checked.h is included
  }

  // Is it a variable type? This is the easy case, we can re-write it
  // locally, at the site of the declaration.
  if (isSingleDeclaration(VD, Where)) {
    if (canRewrite(R, TR)) {
      R.ReplaceText(TR, SRewrite);
    } else {
      // This can happen if SR is within a macro. If that is the case,
      // maybe there is still something we can do because Decl refers
      // to a non-macro line.

      SourceRange Possible(R.getSourceMgr().getExpansionLoc(TR.getBegin()),
                           VD->getLocation());

      if (canRewrite(R, Possible)) {
        R.ReplaceText(Possible, SRewrite);
        std::string NewStr = " " + VD->getName().str();
        R.InsertTextAfter(VD->getLocation(), NewStr);
      } else {
        if (Verbose) {
          errs() << "Still don't know how to re-write VarDecl\n";
          VD->dump();
          errs() << "at\n";
          if (Where)
            Where->dump();
          errs() << "with " << SRewrite << "\n";
        }
      }
    }
  } else if (!isSingleDeclaration(VD, Where) && Skip.find(N) == Skip.end()) {
    // Hack time!
    // Sometimes, like in the case of a decl on a single line, we'll need to
    // do multiple NewTyps at once. In that case, in the inner loop, we'll
    // re-scan and find all of the NewTyps related to that line and do
    // everything at once. That means sometimes we'll get NewTyps that
    // we don't want to process twice. We'll skip them here.

    // Step 1: get the re-written types.
    RSet RewritesForThisDecl(DComp(R.getSourceMgr()));
    auto I = ToRewrite.find(N);
    while (I != ToRewrite.end()) {
      DAndReplace tmp = *I;
      if (areDeclarationsOnSameLine(VD,
                                    Where,
                                    dyn_cast<VarDecl>(tmp.Declaration),
                                    dyn_cast_or_null<DeclStmt>(tmp.Statement)))
        RewritesForThisDecl.insert(tmp);
      ++I;
    }

    // Step 2: Remove the original line from the program.
    SourceLocation EndOfLine = deleteAllDeclarationsOnLine(VD, Where);

    // Step 3: For each decl in the original, build up a new string
    //         and if the original decl was re-written, write that
    //         out instead (WITH the initializer).
    std::string NewMultiLineDeclS = "";
    raw_string_ostream NewMlDecl(NewMultiLineDeclS);
    std::set<Decl *> SameLineDecls;
    getDeclsOnSameLine(VD, Where, SameLineDecls);

    for (const auto &DL : SameLineDecls) {
      DAndReplace N;
      bool Found = false;
      VarDecl *VDL = dyn_cast<VarDecl>(DL);
      if (VDL == nullptr) {
        // Example:
        //        struct {
        //           const wchar_t *start;
        //            const wchar_t *end;
        //        } field[6], name;
        // we cannot handle this.
        errs()
            << "Expected a variable declaration but got an invalid AST node\n";
        DL->dump();
        continue;
      }
      assert(VDL != nullptr);

      for (const auto &NLT : RewritesForThisDecl)
        if (NLT.Declaration == DL) {
          N = NLT;
          Found = true;
          break;
        }

      if (Found) {
        NewMlDecl << N.Replacement;
        if (Expr *E = VDL->getInit()) {
          NewMlDecl << " = ";
          E->printPretty(NewMlDecl, nullptr, A.getPrintingPolicy());
        } else {
          if (isPointerType(VDL))
            NewMlDecl << " = ((void *)0)";
        }
        NewMlDecl << ";\n";
      } else {
        DL->print(NewMlDecl);
        NewMlDecl << ";\n";
      }
    }

    // Step 4: Write out the string built up in step 3.
    R.InsertTextAfter(EndOfLine, NewMlDecl.str());

    // Step 5: Be sure and skip all of the NewTyps that we dealt with
    //         during this time of hacking, by adding them to the
    //         skip set.

    for (const auto &TN : RewritesForThisDecl)
      Skip.insert(TN);
  } else {
    if (Verbose) {
      errs() << "Don't know how to re-write VarDecl\n";
      VD->dump();
      errs() << "at\n";
      if (Where)
        Where->dump();
      errs() << "with " << N.Replacement << "\n";
    }
  }
}

void DeclRewriter::rewrite(RSet &ToRewrite, std::set<FileID> &TouchedFiles) {
  for (const auto &N : ToRewrite) {
    Decl *D = N.Declaration;
    DeclStmt *Where = dyn_cast_or_null<DeclStmt>(N.Statement);
    assert(D != nullptr);

    if (Verbose) {
      errs() << "Replacing type of decl:\n";
      D->dump();
      errs() << "with " << N.Replacement << "\n";
    }

    // Get a FullSourceLoc for the start location and add it to the
    // list of file ID's we've touched.
    SourceRange tTR = D->getSourceRange();
    FullSourceLoc tFSL(tTR.getBegin(), A.getSourceManager());
    TouchedFiles.insert(tFSL.getFileID());

    // Is it a parameter type?
    if (ParmVarDecl *PV = dyn_cast<ParmVarDecl>(D)) {
      assert(Where == nullptr);
      rewrite(PV, N.Replacement);
    } else if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      rewrite(VD, N.Replacement, Where, N, ToRewrite);
    } else if (FunctionDecl *UD = dyn_cast<FunctionDecl>(D)) {
      // TODO: If the return type is a fully-specified function pointer,
      //       then clang will give back an invalid source range for the
      //       return type source range. For now, check that the source
      //       range is valid.
      //       Additionally, a source range can be (mis) identified as
      //       spanning multiple files. We don't know how to re-write that,
      //       so don't.

      if (N.FullDecl) {
        SourceRange SR = UD->getSourceRange();
        SR.setEnd(getFunctionDeclarationEnd(UD, A.getSourceManager()));

        if (canRewrite(R, SR))
          R.ReplaceText(SR, N.Replacement);
      } else {
        SourceRange SR = UD->getReturnTypeSourceRange();
        if (canRewrite(R, SR))
          R.ReplaceText(SR, N.Replacement);
      }
    } else if (FieldDecl *FD = dyn_cast<FieldDecl>(D)) {
      SourceRange SR = FD->getSourceRange();
      std::string SRewrite = N.Replacement;

      if (canRewrite(R, SR))
        R.ReplaceText(SR, SRewrite);
    }
  }
}

std::map<std::string, std::string> DeclRewriter::NewFuncSig;

void DeclRewriter::rewriteDecls(ASTContext &Context, ProgramInfo &Info,
                                Rewriter &R, std::set<FileID> &TouchedFiles) {
  // Compute the bounds information for all the array variables.
  ArrayBoundsRewriter ABRewriter(&Context, Info);

  // Collect function and record declarations that need to be rewritten in a set
  // as well as their rewriten types in a map.
  RSet RewriteThese(DComp(Context.getSourceManager()));
  TypeRewritingVisitor TRV = TypeRewritingVisitor(&Context, Info, RewriteThese,
                                                  NewFuncSig, ABRewriter);
  StructVariableInitializer SVI = StructVariableInitializer(&Context, Info,
                                                            RewriteThese);
  for (const auto &D : Context.getTranslationUnitDecl()->decls()) {
    TRV.TraverseDecl(D);
    SVI.TraverseDecl(D);
  }

  // Build a map of all of the PersistentSourceLoc's back to some kind of
  // Stmt, Decl, or Type.
  TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
  std::set<PersistentSourceLoc> Keys;
  for (const auto &I : Info.getVarMap())
    Keys.insert(I.first);
  MappingVisitor MV(Keys, Context);
  for (const auto &D : TUD->decls())
    MV.TraverseDecl(D);
  SourceToDeclMapType PSLMap;
  VariableDecltoStmtMap VDLToStmtMap;
  std::tie(PSLMap, VDLToStmtMap) = MV.getResults();

  // Add declarations from this map into the rewriting set
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
    if (Decl *D = std::get<1>(PSLMap[PLoc])) {
      // We might have one Decl for multiple Vars, however, one will be a
      // PointerVar so we'll use that.
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
        DeclStmt *DS = nullptr;
        if (VDLToStmtMap.find(D) != VDLToStmtMap.end())
          DS = VDLToStmtMap[D];

        std::string newTy = getStorageQualifierString(D) +
            PV->mkString(Info.getConstraints().getVariables()) +
            ABRewriter.getBoundsString(PV, D);
        RewriteThese.insert(DAndReplace(D, DS, newTy));
      } else if (FV && NewFuncSig.find(FV->getName()) != NewFuncSig.end()
                    && !TRV.isFunctionVisited(FV->getName())) {
        // If this function already has a modified signature? and it is not
        // visited by our cast placement visitor then rewrite it.
        std::string NewSig = NewFuncSig[FV->getName()];
        RewriteThese.insert(DAndReplace(D, NewSig, true));
      }
    }
  }

  // TODO: why do we need to do this?
  GlobalVariableGroups GVG(R.getSourceMgr());
  for (const auto &D : TUD->decls())
    GVG.addGlobalDecl(dyn_cast<VarDecl>(D));

  // Do the declaration rewriting
  DeclRewriter DeclR(R, Context, GVG);
  DeclR.rewrite(RewriteThese, TouchedFiles);
}