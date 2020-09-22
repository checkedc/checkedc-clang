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
                                Rewriter &R) {
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
    // PLoc specifies the location of the variable whose type it is to
    // re-write, but not where the actual type storage is. To get that, we
    // need to turn PLoc into a Decl and then get the SourceRange for the
    // type of the Decl. Note that what we need to get is the ExpansionLoc
    // of the type specifier, since we want where the text is printed before
    // the variable name, not the typedef or #define that creates the
    // name of the type.
    PersistentSourceLoc PLoc = V.first;
    if (Decl *D = std::get<1>(PSLMap[PLoc])) {
      ConstraintVariable *CV = V.second;
      PVConstraint *PV = dyn_cast<PVConstraint>(CV);
      FVConstraint *FV = dyn_cast<FVConstraint>(CV);

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
        RewriteThese.insert(new FunctionDeclReplacement(FD, NewSig, true,
                                                        true));
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
  DeclR.rewrite(RewriteThese);

  for (const auto *R : RewriteThese)
    delete R;
}

void DeclRewriter::rewrite(RSet &ToRewrite) {
  for (auto *const N : ToRewrite) {
    assert(N->getDecl() != nullptr);

    if (Verbose) {
      errs() << "Replacing type of decl:\n";
      N->getDecl()->dump();
      errs() << "with " << N->getReplacement() << "\n";
    }

    // Exact rewriting procedure depends on declaration type
    if (auto *PVR = dyn_cast<ParmVarDeclReplacement>(N)) {
      assert(N->getStatement() == nullptr);
      rewriteParmVarDecl(PVR);
    } else if (auto *VR = dyn_cast<VarDeclReplacement>(N)) {
      rewriteFieldOrVarDecl(VR, ToRewrite);
    } else if (auto *FR = dyn_cast<FunctionDeclReplacement>(N)) {
      rewriteFunctionDecl(FR);
    } else if (auto *FdR = dyn_cast<FieldDeclReplacement>(N)) {
      rewriteFieldOrVarDecl(FdR, ToRewrite);
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


template <typename DRType>
void DeclRewriter::rewriteFieldOrVarDecl(DRType *N, RSet &ToRewrite) {
  static_assert(std::is_same<DRType, FieldDeclReplacement>::value
                    || std::is_same<DRType, VarDeclReplacement>::value,
                "Method expects variable or field declaration replacement.");

  if (isSingleDeclaration(N)) {
    rewriteSingleDecl(N, ToRewrite);
  } else if (Skip.find(N) == Skip.end()) {
    rewriteMultiDecl(N, ToRewrite);
  } else {
    // Anything that reaches this case should be a multi-declaration that has
    // already been rewritten.
    assert("Declaration should have been rewritten." && !isSingleDeclaration(N)
               && Skip.find(N) != Skip.end());
  }
}

void DeclRewriter::rewriteSingleDecl(DeclReplacement *N, RSet &ToRewrite) {
  assert("Declaration is not a single declaration." && isSingleDeclaration(N));
  // This is the easy case, we can rewrite it locally, at the declaration.
  SourceRange TR = N->getDecl()->getSourceRange();
  doDeclRewrite(TR, N);
}

void DeclRewriter::rewriteMultiDecl(DeclReplacement *N, RSet &ToRewrite) {
  assert("Declaration is not a multi declaration." && !isSingleDeclaration(N));
  // Rewriting is more difficult when there are multiple variables declared in a
  // single statement. When this happens, we need to find all the declaration
  // replacement for this statement and apply them at the same time. We also
  // need to avoid rewriting any of these declarations twice by updating the
  // Skip set to include the processed declarations.

  // Step 1: get declaration replacement in the same statement
  RSet RewritesForThisDecl(DComp(R.getSourceMgr()));
  auto I = ToRewrite.find(N);
  while (I != ToRewrite.end()) {
    if (areDeclarationsOnSameLine(N, *I)) {
      assert("Unexpected DeclReplacement kind." &&
             (*I)->getKind() == N->getKind());
      RewritesForThisDecl.insert(*I);
    }
    ++I;
  }

  // Step 2: For each decl in the original, build up a new string. If the
  //         original decl was re-written, write that out instead. Existing
  //         initializers are preserved, any declarations that an initializer to
  //         be valid checked-c are given one.
  std::set<Decl *> SameLineDecls;
  getDeclsOnSameLine(N, SameLineDecls);

  bool IsFirst = true;
  SourceLocation PrevEnd;
  for (const auto &DL : SameLineDecls) {
    // Find the declaration replacement object for the current declaration
    DeclReplacement *SameLineReplacement;
    bool Found = false;
    for (const auto &NLT : RewritesForThisDecl)
      if (NLT->getDecl() == DL) {
        SameLineReplacement = NLT;
        Found = true;
        break;
      }

    if (IsFirst) {
      // Rewriting the first declaration is easy. Nothing should change if its
      // type does not to be rewritten. When rewriting is required, it is
      // essentially the same as the single declaration case.
      IsFirst = false;
      if (Found) {
        SourceRange SR(DL->getBeginLoc(), DL->getEndLoc());
        doDeclRewrite(SR, SameLineReplacement);
      }
    } else  {
      // The subsequent decls are more complicated because we need to insert a
      // type string even if the variables type hasn't changed.
      if (Found) {
        // If the type has changed, the DeclReplacement object has a replacement
        // string stored in it that should be used.
        SourceRange SR(PrevEnd, DL->getEndLoc());
        doDeclRewrite(SR, SameLineReplacement);
      } else {
        // When the type hasn't changed, we still need to insert the original
        // type for the variable.

        // This is a bit of trickery needed to get a string representation of
        // the declaration without the initializer. We don't want to rewrite to
        // initializer because this causes problems when rewriting casts and
        // generic function calls later on. (issue 267)
        auto *VD = dyn_cast<VarDecl>(DL);
        Expr *Init = nullptr;
        if (VD && VD->hasInit()) {
          Init = VD->getInit();
          VD->setInit(nullptr);
        }

        // Dump the declaration (without the initializer) to a string. Printing
        // the AST node gives the full declaration including the base type which
        // is not present in the multi-decl source code.
        std::string DeclStr = "";
        raw_string_ostream DeclStream(DeclStr);
        DL->print(DeclStream);
        assert("Original decl string empty." && !DeclStream.str().empty());

        // Do the replacement. PrevEnd is setup to be the source location of the
        // comma after the previous declaration in the multi-decl. getEndLoc is
        // either the end of the declaration or just before the initializer if
        // one is present.
        SourceRange SR(PrevEnd, DL->getEndLoc());
        R.ReplaceText(SR, DeclStream.str());

        // Undo prior trickery. This need to happen so that the PSL for the decl
        // is not changed since the PSL is used as a map key in a few places.
        if (VD && Init)
          VD->setInit(Init);
      }
    }

    // Variables in a mutli-decl are delimited by commas. The rewritten decls
    // are separate statements separated by a semicolon and a newline.
    SourceRange End = getNextCommaOrSemicolon(DL->getEndLoc());
    R.ReplaceText(End, ";\n ");
    PrevEnd = End.getEnd();
  }

  // Step 3: Be sure and skip all of the declarations that we just dealt with by
  //         adding them to the skip set.
  for (const auto &TN : RewritesForThisDecl)
    Skip.insert(TN);
}

// Common rewriting logic used to replace a single decl either on its own or as
// part of a multi decl. The primary responsibility of this method (aside from
// invoking the rewriter) is to add any required initializer expression.
void DeclRewriter::doDeclRewrite(SourceRange &SR, DeclReplacement *N) {
  std::string Replacement = N->getReplacement();
  if (auto *VD = dyn_cast<VarDecl>(N->getDecl())) {
    if (VD->hasInit()) {
      // Make sure we preserve any existing initializer
      SR.setEnd(VD->getInitializerStartLoc());
      Replacement += " =";
    } else {
      // There is no initializer. Add it if we need one.
      // MWH -- Solves issue 43. Should make it so we insert NULL if stdlib.h or
      // stdlib_checked.h is included
      if (VD->getStorageClass() != StorageClass::SC_Extern) {
        const std::string NullPtrStr = "((void *)0)";
        if (isPointerType(VD)) {
          Replacement += " = " + NullPtrStr;
        } else if  (VD->getType()->isArrayType()) {
          const auto *ElemType = VD->getType()->getPointeeOrArrayElementType();
          if (ElemType->isPointerType())
            Replacement += " = {" + NullPtrStr + "}";
        }
      }
    }
  }

  if (canRewrite(R, SR)) {
    R.ReplaceText(SR, Replacement);
  } else {
    // This can happen if SR is within a macro. If that is the case, maybe there
    // is still something we can do because Decl refers to a non-macro line.
    SourceRange Possible(R.getSourceMgr().getExpansionLoc(SR.getBegin()),
                         SR.getEnd());

    if (canRewrite(R, Possible))
      R.ReplaceText(Possible, Replacement);
    else
      llvm_unreachable(
          "Still can't rewrite declaration."
          "This should have been made WILD during constraint generation.");
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
  if (canRewrite(R, SR)) {
    R.ReplaceText(SR, N->getReplacement());
  } else {
    SourceRange Possible(R.getSourceMgr().getExpansionLoc(SR.getBegin()),
                         SR.getEnd());
    if (canRewrite(R, Possible)) {
      R.ReplaceText(Possible, N->getReplacement());
    } else if (Verbose) {
      errs() << "Don't know how to re-write FunctionDecl\n";
      N->getDecl()->dump();
      errs() << "at\n";
      if (N->getStatement())
        N->getStatement()->dump();
      errs() << "with " << N->getReplacement() << "\n";
    }
  }
}

// Uses clangs lexer to find the location of the next comma or semicolon after
// the given source location. This is used to find the end of each declaration
// within a multi-declaration.
SourceRange DeclRewriter::getNextCommaOrSemicolon(SourceLocation L) {
  SourceManager &SM = A.getSourceManager();
  auto Tok = Lexer::findNextToken(L, SM, A.getLangOpts());
  while (Tok.hasValue() && !Tok->is(clang::tok::eof)) {
    if (Tok->is(clang::tok::comma) || Tok->is(clang::tok::semi))
      return SourceRange(Tok->getLocation(), Tok->getEndLoc());
    Tok = Lexer::findNextToken(Tok->getEndLoc(), A.getSourceManager(),
                               A.getLangOpts());
  }
  llvm_unreachable("Unable to find comma or semicolon at source location.");
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
  if (!Defnc)
    return true;

  // If this is an external function, there is no need to rewrite the
  // declaration. We cannot change the signature of external functions.
  if (!Defnc->hasBody())
    return true;

  // DidAnyParams tracks if we have made any changes to the parameters for this
  // declarations. If no changes are made, then there is no need to rewrite the
  // parameter declarations.
  bool DidAnyParams = false;

  // Get rewritten parameter variable declarations
  std::vector<std::string> ParmStrs;
  for (unsigned I = 0; I < Defnc->numParams(); ++I) {
    auto *Defn = dyn_cast<PVConstraint>(Defnc->getParamVar(I));
    assert(Defn);

    ParmVarDecl *PVDecl = Definition->getParamDecl(I);
    if (isAValidPVConstraint(Defn) && Defn->anyChanges(CS.getVariables())) {
      // This means Defn has a checked type, so we should rewrite to use this
      // type with an itype if applicable.
      DidAnyParams = true;

      if (Defn->hasItype() || !Defn->anyArgumentIsWild(CS.getVariables())) {
        // If the definition already has itype or there are no WILD arguments.
        // New parameter declaration is the checked type plus any itype or array
        // bounds.
        std::string PtypeS =
            Defn->mkString(Info.getConstraints().getVariables());
        PtypeS = PtypeS + getExistingIType(Defn) +
            ABRewriter.getBoundsString(Defn, PVDecl);
        ParmStrs.push_back(PtypeS);
      } else {
        // Here, definition is checked type but at least one of the arguments
        // is WILD. We use the original type for the parameter, but also add an
        // itype.
        std::string PtypeS =
            Defn->mkString(Info.getConstraints().getVariables(), false, true);
        std::string Bi =
            Defn->getRewritableOriginalTy() + Defn->getName() + " : itype(" +
                PtypeS + ")" + ABRewriter.getBoundsString(Defn, PVDecl, true);
        ParmStrs.push_back(Bi);
      }
    } else {
      // When parameter isn't checked, we just dump the original declaration.
      ParmStrs.push_back(getSourceText(PVDecl->getSourceRange(), *Context));
    }
  }


  if (Defnc->numParams() == 0) {
    ParmStrs.push_back("void");
    QualType ReturnTy = FD->getReturnType();
    QualType Ty = FD->getType();
    if (!Ty->isFunctionProtoType() && ReturnTy->isPointerType())
      DidAnyParams = true;
  }

  // Get rewritten return variable
  auto *Defn = dyn_cast<PVConstraint>(Defnc->getReturnVar());

  std::string ReturnVar = "";
  std::string ItypeStr = "";
  bool GeneratedRetIType = false;

  // Does the same job as DidAnyParams, but with respect to the return value. If
  // the return does not change, there is no need to rewrite it.
  bool DidAnyReturn = false;

  // Insert a bounds safe interface for the return.
  if (isAValidPVConstraint(Defn) && Defn->anyChanges(CS.getVariables())) {
    // This means we can infer that the return type is a checked type.
    DidAnyReturn = true;
    // If the definition has itype or there is no argument which is WILD?
    if (Defn->hasItype() || !Defn->anyArgumentIsWild(CS.getVariables())) {
      // Just get the checked itype
      ReturnVar = Defn->mkString(Info.getConstraints().getVariables());
      ItypeStr = getExistingIType(Defn);
    } else {
      // One of the argument is WILD, emit an itype.
      std::string Itype =
          Defn->mkString(Info.getConstraints().getVariables(), false, true);
      ReturnVar = Defn->getRewritableOriginalTy();
      ItypeStr = " : itype(" + Itype + ")";
      GeneratedRetIType = true;

      // A small hack here. The inserted itype comes after param declarations,
      // so we have to rewrite the parameters if we want to insert an itype.
      DidAnyParams = true;
    }
  } else {
    // This means inside the function, the return value is WILD so the return
    // type is what was originally declared.
    ReturnVar = Defn->getOriginalTy() + " ";
    // If this there is already a bounds safe interface, keep using it.
    ItypeStr = getExistingIType(Defn);
  }

  // If the return is a function pointer, we need to rewrite the whole
  // declaration even if no actual changes were made to the parameters. It could
  // probably be done better, but getting the correct source locations is
  // painful.
  if (FD->getReturnType()->isFunctionPointerType() && DidAnyReturn)
    DidAnyParams = true;

  // Combine parameter and return variables rewritings into a single rewriting
  // for the entire function declaration.
  std::string NewSig = "";
  if (DidAnyReturn)
    NewSig = getStorageQualifierString(Definition) + ReturnVar;

  if (DidAnyReturn && DidAnyParams)
    NewSig += Defnc->getName();

  if (DidAnyParams && !ParmStrs.empty()) {
    // Gather individual parameter strings into a single buffer
    std::ostringstream ConcatParamStr;
    copy(ParmStrs.begin(), ParmStrs.end() - 1,
              std::ostream_iterator<std::string>(ConcatParamStr, ", "));
    ConcatParamStr << ParmStrs.back();

    NewSig += "(" + ConcatParamStr.str();
    // Add varargs.
    if (functionHasVarArgs(Definition))
      NewSig += ", ...";
    NewSig += ")";
  }
  if (!ItypeStr.empty())
    NewSig = NewSig + ItypeStr;

  if (ItypeStr.empty() || GeneratedRetIType) {
    NewSig = NewSig + ABRewriter.getBoundsString(Defn, FD, GeneratedRetIType);
  }

  // Add new declarations to RewriteThese if it has changed
  if (DidAnyReturn || DidAnyParams) {
    for (auto *const RD : Definition->redecls())
      RewriteThese.insert(new FunctionDeclReplacement(RD, NewSig, DidAnyReturn,
                                                      DidAnyParams));
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
