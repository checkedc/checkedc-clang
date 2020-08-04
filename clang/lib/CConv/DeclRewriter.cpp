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

void DeclRewriter::rewriteParmVarDecl(const DAndReplace &N) {
  ParmVarDecl *PV = N.getDecl<ParmVarDecl>();

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
        R.ReplaceText(TR, N.Replacement);
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

void DeclRewriter::rewriteVarDecl(const DAndReplace &N, RSet &ToRewrite) {
  VarDecl *VD = N.getDecl<VarDecl>();
  std::string SRewrite = N.Replacement;
  if (Verbose) {
    errs() << "VarDecl at:\n";
    if (N.Statement)
      N.Statement->dump();
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
  if (isSingleDeclaration(VD, N.Statement)) {
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
          if (N.Statement)
            N.Statement->dump();
          errs() << "with " << SRewrite << "\n";
        }
      }
    }
  } else if (!isSingleDeclaration(VD, N.Statement) &&
             Skip.find(N) == Skip.end()) {
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
      DAndReplace Tmp = *I;
      if (areDeclarationsOnSameLine(VD, N.Statement,
                                    dyn_cast<VarDecl>(Tmp.Declaration),
                                    Tmp.Statement))
        RewritesForThisDecl.insert(Tmp);
      ++I;
    }

    // Step 2: Remove the original line from the program.
    SourceLocation EndOfLine = deleteAllDeclarationsOnLine(VD, N.Statement);

    // Step 3: For each decl in the original, build up a new string
    //         and if the original decl was re-written, write that
    //         out instead (WITH the initializer).
    std::string NewMultiLineDeclS = "";
    raw_string_ostream NewMlDecl(NewMultiLineDeclS);
    std::set<Decl *> SameLineDecls;
    getDeclsOnSameLine(VD, N.Statement, SameLineDecls);

    for (const auto &DL : SameLineDecls) {
      DAndReplace SameLineReplacement;
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
          SameLineReplacement = NLT;
          Found = true;
          break;
        }

      if (Found) {
        NewMlDecl << SameLineReplacement.Replacement;
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
      if (N.Statement)
        N.Statement->dump();
      errs() << "with " << N.Replacement << "\n";
    }
  }
}

void DeclRewriter::rewriteFunctionDecl(const DAndReplace &N) {
  // TODO: If the return type is a fully-specified function pointer,
  //       then clang will give back an invalid source range for the
  //       return type source range. For now, check that the source
  //       range is valid.
  //       Additionally, a source range can be (mis) identified as
  //       spanning multiple files. We don't know how to re-write that,
  //       so don't.

  FunctionDecl *UD = N.getDecl<FunctionDecl>();
  SourceRange SR;
  if (N.FullDecl) {
    SR = UD->getSourceRange();
    SR.setEnd(getFunctionDeclarationEnd(UD, A.getSourceManager()));
  } else {
    SR = UD->getReturnTypeSourceRange();
  }
  if (canRewrite(R, SR))
    R.ReplaceText(SR, N.Replacement);
}

void DeclRewriter::rewrite(RSet &ToRewrite, std::set<FileID> &TouchedFiles) {
  for (const auto &N : ToRewrite) {
    assert(N.Declaration != nullptr);

    if (Verbose) {
      errs() << "Replacing type of decl:\n";
      N.Declaration->dump();
      errs() << "with " << N.Replacement << "\n";
    }

    // Get a FullSourceLoc for the start location and add it to the
    // list of file ID's we've touched.
    SourceRange tTR = N.Declaration->getSourceRange();
    FullSourceLoc tFSL(tTR.getBegin(), A.getSourceManager());
    TouchedFiles.insert(tFSL.getFileID());

    // Exact rewriting procedure depends on declaration type
    if (N.hasDeclType<ParmVarDecl>()) {
      // TODO: why is this asserted?
      assert(N.Statement == nullptr);
      rewriteParmVarDecl(N);
    } else if (N.hasDeclType<VarDecl>()) {
      rewriteVarDecl(N, ToRewrite);
    } else if (N.hasDeclType<FunctionDecl>()) {
      rewriteFunctionDecl(N);
    } else if (N.hasDeclType<FieldDecl>()) {
      SourceRange SR = N.getDecl<FieldDecl>()->getSourceRange();
      if (canRewrite(R, SR))
        R.ReplaceText(SR, N.Replacement);
    }
  }
}

// Note: This is variable declared static in the header file in order to pass
// information between different invocations on different translation units.
std::map<std::string, std::string> DeclRewriter::NewFuncSig;

// This function is the public entry point for declaration rewriting.
void DeclRewriter::rewriteDecls(ASTContext &Context, ProgramInfo &Info,
                                Rewriter &R, std::set<FileID> &TouchedFiles) {
  // Compute the bounds information for all the array variables.
  ArrayBoundsRewriter ABRewriter(&Context, Info);

  // Collect function and record declarations that need to be rewritten in a set
  // as well as their rewriten types in a map.
  RSet RewriteThese(DComp(Context.getSourceManager()));
  FunctionDeclBuilder TRV = FunctionDeclBuilder(&Context, Info, RewriteThese,
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
  string NewSig = "";
  vector<string> ParmStrs;
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
          string PtypeS =
              Defn->mkString(Info.getConstraints().getVariables());

          // If there is no declaration?
          // check the itype in definition.
          PtypeS = PtypeS + getExistingIType(Defn) +
              ABRewriter.getBoundsString(Defn, Definition->getParamDecl(i));

          ParmStrs.push_back(PtypeS);
        } else {
          // Here, definition is checked type but at least one of the arguments
          // is WILD.
          string PtypeS =
              Defn->mkString(Info.getConstraints().getVariables(), false, true);
          string Bi =
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
      string Scratch = "";
      raw_string_ostream DeclText(Scratch);
      Definition->getParamDecl(i)->print(DeclText);
      ParmStrs.push_back(DeclText.str());
    }
  }

  // Compare returns.
  auto *Defn = dyn_cast<PVConstraint>(getOnly(Defnc->getReturnVars()));

  string ReturnVar = "";
  string EndStuff = "";
  bool ReturnHandled = false;

  if (isAValidPVConstraint(Defn)) {
    // Insert a bounds safe interface for the return.
    bool anyConstrained = Defn->anyChanges(CS.getVariables());
    if (anyConstrained) {
      // This means we were able to infer that return type
      // is a checked type.
      ReturnHandled = true;
      DidAny = true;
      string Ctype = "";
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
    ostringstream ConcatParamStr;
    copy(ParmStrs.begin(), ParmStrs.end() - 1,
              ostream_iterator<string>(ConcatParamStr, ", "));
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

std::string FunctionDeclBuilder::getExistingIType(ConstraintVariable *DeclC) {
  auto *PVC = dyn_cast<PVConstraint>(DeclC);
  if (PVC != nullptr && PVC->hasItype())
    return " : " + PVC->getItype();
  return "";
}

// Check if the function is handled by this visitor.
bool FunctionDeclBuilder::isFunctionVisited(string FuncName) {
  return VisitedSet.find(FuncName) != VisitedSet.end();
}