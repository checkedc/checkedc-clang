//=--DeclRewriter.cpp---------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
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
#include "clang/AST/RecursiveASTVisitor.h"

using namespace llvm;
using namespace clang;


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
      for (const auto &V : Vars)
        if (PVConstraint *T = dyn_cast<PVConstraint>(V))
          PV = T;
        else if (FVConstraint *T = dyn_cast<FVConstraint>(V))
          FV = T;

      if (PV && PV->anyChanges(Info.getConstraints().getVariables()) &&
          !PV->isPartOfFunctionPrototype()) {
        // Rewrite a declaration, only if it is not part of function prototype.
        DeclStmt *DS = nullptr;
        if (VDLToStmtMap.find(D) != VDLToStmtMap.end())
          DS = VDLToStmtMap[D];

        std::string newTy = getStorageQualifierString(D) +
            PV->mkString(Info.getConstraints().getVariables()) +
            ABRewriter.getBoundsString(PV, D);
        if (auto *VD = dyn_cast<VarDecl>(D))
          RewriteThese.insert(new VarDeclReplacement(VD, DS, newTy));
        else if (auto *FD = dyn_cast<FieldDecl>(D))
          RewriteThese.insert(new FieldDeclReplacement(FD, DS, newTy));
        else if (auto *PD = dyn_cast<ParmVarDecl>(D))
          RewriteThese.insert(new ParmVarDeclReplacement(PD, DS, newTy));
        else
          llvm_unreachable("Unrecognized declaration type.");
      } else if (FV && NewFuncSig.find(FV->getName()) != NewFuncSig.end()
          && !TRV.isFunctionVisited(FV->getName())) {
        auto *FD = cast<FunctionDecl>(D);
        // TODO: I don't think this branch is ever reached. Either remove it or
        //       add a test case that reaches it.
        // If this function already has a modified signature? and it is not
        // visited by our cast placement visitor then rewrite it.
        std::string NewSig = NewFuncSig[FV->getName()];
        RewriteThese.insert(new FunctionDeclReplacement(FD, NewSig, true));
      }
    }
  }

  // Build sets of variables that are declared in the same statement so we can
  // rewrite things like int x, *y, **z;
  GlobalVariableGroups GVG(R.getSourceMgr());
  for (const auto &D : TUD->decls()) {
    GVG.addGlobalDecl(dyn_cast<VarDecl>(D));
    //Search through the AST for fields that occur on the same line
    FieldFinder::gatherSameLineFields(GVG, D);
  }

  // Do the declaration rewriting
  DeclRewriter DeclR(R, Context, GVG);
  DeclR.rewrite(RewriteThese, TouchedFiles);

  for (const auto *R : RewriteThese)
    delete R;
}

void DeclRewriter::rewrite(RSet &ToRewrite, std::set<FileID> &TouchedFiles) {
  for (auto *const N : ToRewrite) {
    assert(N->getDecl() != nullptr);

    if (Verbose) {
      errs() << "Replacing type of decl:\n";
      N->getDecl()->dump();
      errs() << "with " << N->getReplacement() << "\n";
    }

    // Get a FullSourceLoc for the start location and add it to the
    // list of file ID's we've touched.
    SourceRange tTR = N->getDecl()->getSourceRange();
    FullSourceLoc tFSL(tTR.getBegin(), A.getSourceManager());
    TouchedFiles.insert(tFSL.getFileID());

    // Exact rewriting procedure depends on declaration type
    if (auto *PVR = dyn_cast<ParmVarDeclReplacement>(N)) {
      assert(N->getStatement() == nullptr);
      rewriteParmVarDecl(PVR);
    } else if (auto *VR = dyn_cast<VarDeclReplacement>(N)) {
      rewriteMultiDecl(VR, ToRewrite);
    } else if (auto *FR = dyn_cast<FunctionDeclReplacement>(N)) {
      rewriteFunctionDecl(FR);
    } else if (auto *FdR = dyn_cast<FieldDeclReplacement>(N)) {
      rewriteMultiDecl(FdR, ToRewrite);
    }
  }
}

void DeclRewriter::rewriteParmVarDecl(ParmVarDeclReplacement *N) {
  // First, find all the declarations of the containing function.
  DeclContext *DF = N->getDecl()->getParentFunctionOrMethod();
  assert(DF != nullptr && "no parent function or method for decl");
  FunctionDecl *FD = cast<FunctionDecl>(DF);

  // For each function, determine which parameter in the declaration
  // matches PV, then, get the type location of that parameter
  // declaration and re-write.
  unsigned int PIdx = getParameterIndex(N->getDecl(), FD);

  for (auto *CurFD = FD; CurFD != nullptr; CurFD = CurFD->getPreviousDecl())
    if (PIdx < CurFD->getNumParams()) {
      ParmVarDecl *Rewrite = CurFD->getParamDecl(PIdx);
      assert(Rewrite != nullptr);
      SourceRange TR = Rewrite->getSourceRange();

      if (canRewrite(R, TR))
        R.ReplaceText(TR, N->getReplacement());
    }
}


template <typename DT, DeclReplacement::DRKind DK>
void DeclRewriter::rewriteMultiDecl(DeclReplacementTempl<DT, DK> *N,
                                    RSet &ToRewrite) {
  DT *D = N->getDecl();
  std::string SRewrite = N->getReplacement();
  if (Verbose) {
    errs() << "Decl at:\n";
    if (N->getStatement())
      N->getStatement()->dump();
  }
  SourceRange TR = D->getSourceRange();

  // Is there an initializer? If there is, change TR so that it points
  // to the START of the SourceRange of the initializer text, and drop
  // an '=' token into sRewrite.
  // Only Vardecls can have initializers
  if(auto VD = dyn_cast<VarDecl>(D)) {
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
  }

  // Is it a variable type? This is the easy case, we can re-write it
  // locally, at the site of the declaration.
  if (isSingleDeclaration(N)) {
    if (canRewrite(R, TR)) {
      R.ReplaceText(TR, SRewrite);
    } else {
      // This can happen if SR is within a macro. If that is the case,
      // maybe there is still something we can do because Decl refers
      // to a non-macro line.

      SourceRange Possible(R.getSourceMgr().getExpansionLoc(TR.getBegin()),
                           D->getLocation());

      if (canRewrite(R, Possible)) {
        R.ReplaceText(Possible, SRewrite);
        std::string NewStr = " " + D->getName().str();
        R.InsertTextAfter(D->getLocation(), NewStr);
      } else {
        if (Verbose) {
          errs() << "Still don't know how to re-write VarDecl\n";
          D->dump();
          errs() << "at\n";
          if (N->getStatement())
            N->getStatement()->dump();
          errs() << "with " << SRewrite << "\n";
        }
      }
    }
  } else if (!isSingleDeclaration(N) &&
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
      auto *Tmp = dyn_cast<DeclReplacementTempl<DT, DK>>(*I);
      if (Tmp != nullptr && areDeclarationsOnSameLine(N, Tmp))
        RewritesForThisDecl.insert(Tmp);
      ++I;
    }

    // Step 2: Remove the original line from the program.
    SourceLocation EndOfLine = deleteAllDeclarationsOnLine(N);

    // Step 3: For each decl in the original, build up a new string
    //         and if the original decl was re-written, write that
    //         out instead (WITH the initializer).
    std::string NewMultiLineDeclS = "";
    raw_string_ostream NewMlDecl(NewMultiLineDeclS);
    std::set<Decl *> SameLineDecls;
    getDeclsOnSameLine(N, SameLineDecls);

    for (const auto &DL : SameLineDecls) {
      DT *SDL = dyn_cast<DT>(DL);
      if (SDL == nullptr) {
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
      assert(SDL != nullptr);

      DeclReplacement *SameLineReplacement;
      bool Found = false;
      for (const auto &NLT : RewritesForThisDecl)
        if (NLT->getDecl() == DL) {
          SameLineReplacement = NLT;
          Found = true;
          break;
        }

      if (Found) {
        NewMlDecl << SameLineReplacement->getReplacement();
        if (auto VDL = dyn_cast<VarDecl>(SDL)) {
          if (Expr *E = VDL->getInit()) {
            NewMlDecl << " = ";
            E->printPretty(NewMlDecl, nullptr, A.getPrintingPolicy());
          } else {
            if (isPointerType(VDL))
              NewMlDecl << " = ((void *)0)";
          }
        }
        NewMlDecl << (dyn_cast<VarDecl>(SDL) ? ";\n" : "; ");
      } else {
        DL->print(NewMlDecl);
        NewMlDecl << (dyn_cast<VarDecl>(SDL) ? ";\n" : "; ");
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
      D->dump();
      errs() << "at\n";
      if (N->getStatement())
        N->getStatement()->dump();
      errs() << "with " << N->getReplacement() << "\n";
    }
  }
}

void DeclRewriter::rewriteFunctionDecl(FunctionDeclReplacement *N) {
  // TODO: If the return type is a fully-specified function pointer,
  //       then clang will give back an invalid source range for the
  //       return type source range. For now, check that the source
  //       range is valid.
  //       Additionally, a source range can be (mis) identified as
  //       spanning multiple files. We don't know how to re-write that,
  //       so don't.
  SourceRange SR = N->getSourceRange(A.getSourceManager());
  if (canRewrite(R, SR))
    R.ReplaceText(SR, N->getReplacement());
}

bool DeclRewriter::areDeclarationsOnSameLine(DeclReplacement *N1,
                                             DeclReplacement *N2) {
  Decl *D1 = N1->getDecl();
  Decl *D2 = N2->getDecl();
  if (D1 && D2) {
    // In the event that this is a FieldDecl,
    // these statements will always be null
    DeclStmt *Stmt1 = N1->getStatement();
    DeclStmt *Stmt2 = N2->getStatement();
    if (Stmt1 == nullptr && Stmt2 == nullptr) {
      auto &DGroup = GP.getVarsOnSameLine(D1);
      return DGroup.find(D2) != DGroup.end();
    } else if (Stmt1 == nullptr || Stmt2 == nullptr) {
      return false;
    } else {
      return Stmt1 == Stmt2;
    }
  }
  return false;
}


bool DeclRewriter::isSingleDeclaration(DeclReplacement *N) {
  DeclStmt *Stmt = N->getStatement();
  if (Stmt == nullptr) {
    auto &VDGroup = GP.getVarsOnSameLine(N->getDecl());
    return VDGroup.size() == 1;
  } else {
    return Stmt->isSingleDecl();
  }
}




void DeclRewriter::getDeclsOnSameLine(DeclReplacement *N,
                                      std::set<Decl *> &Decls) {
  if (N->getStatement() != nullptr)
    Decls.insert(N->getStatement()->decls().begin(),
                 N->getStatement()->decls().end());
  else
    Decls.insert(GP.getVarsOnSameLine(N->getDecl()).begin(),
                 GP.getVarsOnSameLine(N->getDecl()).end());
}


SourceLocation
DeclRewriter::deleteAllDeclarationsOnLine(DeclReplacement *DR)
{
  if (DeclStmt *Stmt = DR->getStatement()) {
    // If there is a statement, delete the entire statement.
    R.RemoveText(Stmt->getSourceRange());
    return Stmt->getSourceRange().getEnd();
  } else {
    SourceLocation BLoc;
    SourceManager &SM = R.getSourceMgr();
    // Remove all vars on the line.
    for (auto *SD : GP.getVarsOnSameLine(DR->getDecl())) {
      SourceRange ToDel = SD->getSourceRange();
      if (BLoc.isInvalid() ||
          SM.isBeforeInTranslationUnit(ToDel.getBegin(), BLoc))
        BLoc = ToDel.getBegin();
      if(dyn_cast<VarDecl>(DR->getDecl())) {
        R.RemoveText(SD->getSourceRange());
      } else if (dyn_cast<FieldDecl>(DR->getDecl())) {
        // If it's a FielDecl make sure to grab the end semicolon
        auto end = Lexer::getLocForEndOfToken(SD->getEndLoc(), 0,
                                              SM, A.getLangOpts());
        R.RemoveText(SourceRange(SD->getBeginLoc(), end));
      } else {
        llvm_unreachable("Only VarDecls or FieldDecls should be passed here");
      }
    }
    return BLoc;
  }
}

// Note: This is variable declared static in the header file in order to pass
// information between different invocations on different translation units.
std::map<std::string, std::string> DeclRewriter::NewFuncSig;

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
  VisitedSet.insert(FuncName);

  FVConstraint *Defnc = Info.getFuncConstraint(Definition, Context);
  assert(Defnc != nullptr);

  // If this is an external function, there is no need to rewrite the
  // declaration. We cannot change the signature of external functions.
  if (!Defnc->hasBody())
    return true;

  // DidAny tracks if we have made any changes to this function declaration.
  // If no changes are made, then there is no need to rewrite anything, and the
  // declaration is not added to RewriteThese.
  bool DidAny = false;

  // Get rewritten parameter variable declarations
  std::vector<std::string> ParmStrs;
  for (unsigned i = 0; i < Defnc->numParams(); ++i) {
    auto *Defn = dyn_cast<PVConstraint>(Defnc->getParamVar(i));
    assert(Defn);

    if (isAValidPVConstraint(Defn) && Defn->anyChanges(CS.getVariables())) {
      // This means Defn has a checked type, so we should rewrite to use this
      // type with an itype if applicable.
      DidAny = true;

      if (Defn->hasItype() || !Defn->anyArgumentIsWild(CS.getVariables())) {
        // If the definition already has itype or there are no WILD arguments.
        // New parameter declaration is the checked type plus any itype or array
        // bounds.
        std::string PtypeS =
            Defn->mkString(Info.getConstraints().getVariables());
        PtypeS = PtypeS + getExistingIType(Defn) +
            ABRewriter.getBoundsString(Defn, Definition->getParamDecl(i));
        ParmStrs.push_back(PtypeS);
      } else {
        // Here, definition is checked type but at least one of the arguments
        // is WILD. We use the original type for the parameter, but also add an
        // itype.
        std::string PtypeS =
            Defn->mkString(Info.getConstraints().getVariables(), false, true);
        std::string Bi =
            Defn->getRewritableOriginalTy() + Defn->getName() + " : itype(" +
                PtypeS + ")" +
                ABRewriter.getBoundsString(Defn,
                                       Definition->getParamDecl(i), true);
        ParmStrs.push_back(Bi);
      }
    } else {
      // If the parameter isn't checked, we can just dump the original
      // declaration.
      std::string Scratch = "";
      raw_string_ostream DeclText(Scratch);
      Definition->getParamDecl(i)->print(DeclText);
      ParmStrs.push_back(DeclText.str());
    }
  }

  // Get rewritten return variable
  auto *Defn = dyn_cast<PVConstraint>(Defnc->getReturnVar());

  std::string ReturnVar = "";
  std::string ItypeStr = "";

  // Insert a bounds safe interface for the return.
  if (isAValidPVConstraint(Defn) && Defn->anyChanges(CS.getVariables())) {
    // This means we can infer that the return type is a checked type.
    DidAny = true;
    // If the definition has itype or there is no argument which is WILD?
    if (Defn->hasItype() || !Defn->anyArgumentIsWild(CS.getVariables())) {
      // Just get the checked itype
      ReturnVar = Defn->mkString(Info.getConstraints().getVariables());
      ItypeStr = getExistingIType(Defn);
    } else {
      // One of the argument is WILD, emit an itype.
      std::string Itype =
          Defn->mkString(Info.getConstraints().getVariables(), true, true);
      ReturnVar = Defn->getRewritableOriginalTy();
      ItypeStr = " : itype(" + Itype + ")";
    }
  } else {
    // This means inside the function, the return value is WILD so the return
    // type is what was originally declared.
    ReturnVar = Defn->getOriginalTy() + " ";
    // If this there is already a bounds safe interface, keep using it.
    ItypeStr = getExistingIType(Defn);
  }

  // Combine parameter and return variables rewritings into a single rewriting
  // for the entire function declaration.
  std::string NewSig =
      getStorageQualifierString(Definition) + ReturnVar + Defnc->getName()
          + "(";
  if (!ParmStrs.empty()) {
    // Gather individual parameter strings into a single buffer
    std::ostringstream ConcatParamStr;
    copy(ParmStrs.begin(), ParmStrs.end() - 1,
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
  if (!ItypeStr.empty())
    NewSig = NewSig + ItypeStr;

  // Add new declarations to RewriteThese if it has changed
  if (DidAny) {
    for (auto *const RD : Definition->redecls())
      RewriteThese.insert(new FunctionDeclReplacement(RD, NewSig, true));
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
bool FunctionDeclBuilder::isFunctionVisited(std::string FuncName) {
  return VisitedSet.find(FuncName) != VisitedSet.end();
}

bool FieldFinder::VisitFieldDecl(FieldDecl *FD) {
  GVG.addGlobalDecl(FD);
  return true;
}

void FieldFinder::gatherSameLineFields(GlobalVariableGroups &GVG, Decl *D) {
  FieldFinder FF(GVG);
  FF.TraverseDecl(D);
}
