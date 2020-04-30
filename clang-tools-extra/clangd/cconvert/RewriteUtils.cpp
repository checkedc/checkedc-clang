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

#include "llvm/Support/raw_ostream.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include <algorithm>
#include <map>
#include <sstream>
#include "clang/AST/Type.h"
#include "RewriteUtils.h"
#include "MappingVisitor.h"
#include "Utils.h"
#include "CCGlobalOptions.h"
#include "ArrayBoundsInferenceConsumer.h"

using namespace llvm;
using namespace clang;

SourceRange DComp::getWholeSR(SourceRange Orig, DAndReplace Dr) const {
  SourceRange newSourceRange(Orig);

  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(Dr.Declaration)) {
    newSourceRange.setEnd(getFunctionDeclarationEnd(FD, SM));
    if (Dr.fullDecl == false)
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
    SourceLocation NewBegin = (*RhStmt->decls().begin())->getSourceRange().getBegin();
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
    if (VDSet == nullptr) {
      VDSet = new std::set<VarDecl *>();
    }
    VDSet->insert(VD);
    globVarGroups[VD] = VDSet;
    // Process the next decl.
    Decl *NDecl = VD->getNextDeclInContext();
    if (NDecl && dyn_cast<VarDecl>(NDecl)) {
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
  for (auto &currV: globVarGroups) {
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
static bool canRewrite(Rewriter &R, SourceRange &SR) {
  return SR.isValid() && (R.getRangeSize(SR) != -1);
}

void rewrite(ParmVarDecl *PV, Rewriter &R, std::string SRewrite) {
  // First, find all the declarations of the containing function.
  DeclContext *DF = PV->getParentFunctionOrMethod();
  assert(DF != nullptr && "no parent function or method for decl");
  FunctionDecl *FD = cast<FunctionDecl>(DF);

  // For each function, determine which parameter in the declaration
  // matches PV, then, get the type location of that parameter
  // declaration and re-write.

  // This is kind of hacky, maybe we should record the index of the
  // parameter when we find it, instead of re-discovering it here.
  int PIdx = -1;
  int c = 0;
  for (const auto &I : FD->parameters()) {
    if (I == PV) {
      PIdx = c;
      break;
    }
    c++;
  }
  assert(PIdx >= 0);

  for (FunctionDecl *toRewrite = FD; toRewrite != nullptr;
       toRewrite = toRewrite->getPreviousDecl()) {
    int U = toRewrite->getNumParams();
    if (PIdx < U) {
      // TODO these declarations could get us into deeper
      // Header files.
      ParmVarDecl *Rewrite = toRewrite->getParamDecl(PIdx);
      assert(Rewrite != nullptr);
      SourceRange TR = Rewrite->getSourceRange();

      if (canRewrite(R, TR))
        R.ReplaceText(TR, SRewrite);
    }
  }
}

bool areDeclarationsOnSameLine(VarDecl *VD1, DeclStmt *Stmt1, VarDecl *VD2,
                               DeclStmt *Stmt2, GlobalVariableGroups &GP) {
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

bool isSingleDeclaration(VarDecl *VD, DeclStmt *Stmt, GlobalVariableGroups &GP) {
  if (Stmt == nullptr) {
    auto &VDGroup = GP.getVarsOnSameLine(VD);
    return VDGroup.size() == 1;
  } else {
    return Stmt->isSingleDecl();
  }
}

void getDeclsOnSameLine(VarDecl *VD, DeclStmt *Stmt, GlobalVariableGroups &GP,
                        std::set<Decl*> &Decls) {
  if (Stmt != nullptr) {
    Decls.insert(Stmt->decls().begin(), Stmt->decls().end());
  } else {
    Decls.insert(GP.getVarsOnSameLine(VD).begin(),
                 GP.getVarsOnSameLine(VD).end());
  }
}

SourceLocation deleteAllDeclarationsOnLine(VarDecl *VD, DeclStmt *Stmt,
                                           Rewriter &R,
                                           GlobalVariableGroups &GP) {
  if (Stmt != nullptr) {
    // If there is a statement, delete the entire statement.
    R.RemoveText(Stmt->getSourceRange());
    return Stmt->getSourceRange().getEnd();
  } else {
    SourceLocation BLoc;
    SourceManager &SM = R.getSourceMgr();
    // Remove all vars on the line.
    for (auto D : GP.getVarsOnSameLine(VD)) {
      SourceRange ToDel = D->getSourceRange();
      if (BLoc.isInvalid() ||
          SM.isBeforeInTranslationUnit(ToDel.getBegin(), BLoc))
        BLoc = ToDel.getBegin();
      R.RemoveText(D->getSourceRange());
    }
    return BLoc;
  }
}

void rewrite( VarDecl               *VD,
              Rewriter              &R,
              std::string SRewrite,
              Stmt                  *WhereStmt,
              RSet                  &skip,
              const DAndReplace     &N,
              RSet                  &ToRewrite,
              ASTContext            &A,
              GlobalVariableGroups  &GP)
{
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
  if (isSingleDeclaration(VD, Where, GP)) {
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
  } else if (!isSingleDeclaration(VD, Where, GP) &&
             skip.find(N) == skip.end()) {
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
      if (areDeclarationsOnSameLine(VD, Where, dyn_cast<VarDecl>(tmp.Declaration),
                                    dyn_cast_or_null<DeclStmt>(tmp.Statement), GP))
        RewritesForThisDecl.insert(tmp);
      ++I;
    }

    // Step 2: Remove the original line from the program.
    SourceLocation EndOfLine = deleteAllDeclarationsOnLine(VD, Where, R, GP);

    // Step 3: For each decl in the original, build up a new string
    //         and if the original decl was re-written, write that
    //         out instead (WITH the initializer).
    std::string NewMultiLineDeclS = "";
    raw_string_ostream NewMlDecl(NewMultiLineDeclS);
    std::set<Decl *> SameLineDecls;
    getDeclsOnSameLine(VD, Where, GP, SameLineDecls);

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
        errs() << "Expected a variable declaration but got an invalid AST node\n";
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
            NewMlDecl << " = nullptr";
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
      skip.insert(TN);
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

void rewrite( Rewriter              &R,
              RSet                  &ToRewrite,
              RSet                  &Skip,
              SourceManager         &S,
              ASTContext            &A,
              std::set<FileID>      &Files,
              GlobalVariableGroups  &GP)
{
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
    FullSourceLoc tFSL(tTR.getBegin(), S);
    Files.insert(tFSL.getFileID());

    // Is it a parameter type?
    if (ParmVarDecl *PV = dyn_cast<ParmVarDecl>(D)) {
      assert(Where == nullptr);
      rewrite(PV, R, N.Replacement);
    } else if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      rewrite(VD, R, N.Replacement, Where, Skip, N, ToRewrite, A, GP);
    } else if (FunctionDecl *UD = dyn_cast<FunctionDecl>(D)) {
      // TODO: If the return type is a fully-specified function pointer,
      //       then clang will give back an invalid source range for the
      //       return type source range. For now, check that the source
      //       range is valid.
      //       Additionally, a source range can be (mis) identified as
      //       spanning multiple files. We don't know how to re-write that,
      //       so don't.

      if (N.fullDecl) {
        SourceRange SR = UD->getSourceRange();
        SR.setEnd(getFunctionDeclarationEnd(UD, S));

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


// For a given function name, what are the argument positions for that function
// that we would want to treat specially and insert a cast into?
std::set<unsigned int> TypeRewritingVisitor::getParamsForExtern(std::string E) {
  return StringSwitch<std::set<unsigned int>>(E)
          .Case("free", {0})
          .Default(std::set<unsigned int>());
}

// Checks the bindings in the environment for all of the constraints
// associated with C and returns true if any of those constraints
// are WILD.
bool TypeRewritingVisitor::anyTop(std::set<ConstraintVariable*> C) {
  bool TopFound = false;
  Constraints &CS = Info.getConstraints();
  Constraints::EnvironmentMap &env = CS.getVariables();
  for (ConstraintVariable *c : C) {
    if (PointerVariableConstraint *pvc = dyn_cast<PointerVariableConstraint>(c)) {
      for (uint32_t v : pvc->getCvars()) {
        ConstAtom *CK = env[CS.getVar(v)];
        if (CK->getKind() == Atom::A_Wild) {
          TopFound = true;
        }
      }
    }
  }
  return TopFound;
}

std::string TypeRewritingVisitor::getExistingIType(ConstraintVariable *DeclC,
                                                   ConstraintVariable *Defc,
                                                   FunctionDecl *FuncDecl) {
  std::string Ret = "";
  ConstraintVariable *T = DeclC;
  if (FuncDecl == nullptr) {
    T = Defc;
  }
  if (PVConstraint *PVC = dyn_cast<PVConstraint>(T)) {
    if (PVC->getItypePresent()) {
      Ret = " : " + PVC->getItype();
    }
  }
  return Ret;
}

// This function checks how to re-write a function declaration.
bool TypeRewritingVisitor::VisitFunctionDecl(FunctionDecl *FD) {

  // Get all of the constraint variables for the function.
  // Check and see if we have a definition in scope. If we do, then:
  // For the return value and each of the parameters, do the following:
  //   1. Get a constraint variable representing the definition (def) and the
  //      declaration (dec).
  //   2. Check if def < dec, dec < def, or dec = def.
  //   3. Only if def < dec, we insert a bounds-safe interface.
  // If we don't have a definition in scope, we can assert that all of
  // the constraint variables are equal.
  // Finally, we need to note that we've visited this particular function, and
  // that we shouldn't make one of these visits again.

  auto FuncName = FD->getNameAsString();

  auto &CS = Info.getConstraints();

  // Do we have a definition for this declaration?
  FunctionDecl *Definition = getDefinition(FD);
  FunctionDecl *Declaration = getDeclaration(FD);

  if (Definition == nullptr)
    return true;

  // Make sure we haven't visited this function name before, and that we
  // only visit it once.
  if (VisitedSet.find(FuncName) != VisitedSet.end())
    return true;
  else
    VisitedSet.insert(FuncName);

  FVConstraint *Defnc =
      getHighestT<FVConstraint>(
          Info.getFuncDefnConstraints(Definition, Context),
          Info);

  FVConstraint *Declc = nullptr;
  std::set<ConstraintVariable*> *FuncDeclKeys =
      Info.getFuncDeclConstraintSet(
          Info.getUniqueDeclKey(Definition, Context));
  // Get constraint variables for the declaration and the definition.
  // Those constraints should be function constraints.
  if (FuncDeclKeys != nullptr) {
    // If there is no declaration?
    // Get the on demand function variable constraint.
    Declc = getHighestT<FVConstraint>(*FuncDeclKeys, Info);
  } else {
    // No declaration constraints found. So, create on demand
    // declaration constraints.
    Declc =
        getHighestT<FVConstraint>(
            Info.getVariableOnDemand(Definition, Context, false), Info);
  }

  assert(Declc != nullptr);
  assert(Defnc != nullptr);

  if (Declc->numParams() == Defnc->numParams()) {
    // Track whether we did any work and need to make a substitution or not.
    bool DidAny = Declc->numParams() > 0;
    std::string s = "";
    std::vector<std::string> ParmStrs;
    // Compare parameters.
    for (unsigned i = 0; i < Declc->numParams(); ++i) {
      auto Decl = getHighestT<PVConstraint>(Declc->getParamVar(i), Info);
      auto Defn = getHighestT<PVConstraint>(Defnc->getParamVar(i), Info);
      assert(Decl);
      assert(Defn);
      bool ParameterHandled = false;

      if (ProgramInfo::isAValidPVConstraint(Decl) &&
          ProgramInfo::isAValidPVConstraint(Defn)) {
        auto HeadDefnCVar = *(Defn->getCvars().begin());
        auto HeadDeclCVar = *(Decl->getCvars().begin());
        // If this holds, then we want to insert a bounds safe interface.
        bool Constrained = !CS.isWild(HeadDefnCVar);
        // Definition is more precise than declaration.
        // Section 5.3:
        // https://www.microsoft.com/en-us/research/uploads/prod/2019/05/checkedc-post2019.pdf
        if (Constrained && CS.isWild(HeadDeclCVar)) {
          // If definition is more precise
          // than declaration emit an itype.
          std::string PtypeS =
              Defn->mkString(Info.getConstraints().getVariables(), false, true);
          std::string bi = Defn->getRewritableOriginalTy() +
                           Defn->getName() + " : itype(" +
              PtypeS + ")" +
              ABRewriter.getBoundsString(Definition->getParamDecl(i), true);
          ParmStrs.push_back(bi);
          ParameterHandled = true;
        } else if (Constrained) {
          // Both the declaration and definition are same
          // and they are safer than what was originally declared.
          // Here we should emit a checked type!
          std::string PtypeS =
              Defn->mkString(Info.getConstraints().getVariables());

          // If there is no declaration?
          // check the itype in definition.
          PtypeS = PtypeS + getExistingIType(Decl, Defn, Declaration) +
              ABRewriter.getBoundsString(Definition->getParamDecl(i));

          ParmStrs.push_back(PtypeS);
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
    auto Decl = getHighestT<PVConstraint>(Declc->getReturnVars(), Info);
    auto Defn = getHighestT<PVConstraint>(Defnc->getReturnVars(), Info);
    std::string ReturnVar = "";
    std::string EndStuff = "";
    bool ReturnHandled = false;

    if (ProgramInfo::isAValidPVConstraint(Decl) &&
        ProgramInfo::isAValidPVConstraint(Defn)) {
      auto HeadDefnCVar = *(Defn->getCvars().begin());
      auto HeadDeclCVar = *(Decl->getCvars().begin());
      // Insert a bounds safe interface for the return.
      bool anyConstrained = !CS.isWild(HeadDefnCVar);
      if (anyConstrained) {
        ReturnHandled = true;
        DidAny = true;
        std::string Ctype = "";
        DidAny = true;
        // Definition is more precise than declaration.
        // Section 5.3:
        // https://www.microsoft.com/en-us/research/uploads/prod/2019/05/checkedc-post2019.pdf
        if (CS.isWild(HeadDeclCVar)) {
          Ctype =
              Defn->mkString(Info.getConstraints().getVariables(), true, true);
          ReturnVar = Defn->getRewritableOriginalTy();
          EndStuff = " : itype(" + Ctype + ")";
        } else {
          // This means we were able to infer that return type
          // is a checked type.
          // However, the function returns a less precise type, whereas
          // all the uses of the function converts the return value
          // into a more precise type.
          // Do not change the type
          ReturnVar = Decl->mkString(Info.getConstraints().getVariables());
          EndStuff = getExistingIType(Decl, Defn, Declaration);
        }
      }
    }

    // This means inside the function, the return value is WILD
    // so the return type is what was originally declared.
    if (!ReturnHandled) {
      // If we used to implement a bounds-safe interface, continue to do that.
      ReturnVar = Decl->getOriginalTy() + " ";

      EndStuff = getExistingIType(Decl, Defn, Declaration);
      if (!EndStuff.empty()) {
        DidAny = true;
      }
    }

    s = getStorageQualifierString(Definition) + ReturnVar + Declc->getName() + "(";
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
    }

    if (EndStuff.size() > 0)
      s = s + EndStuff;

    if (DidAny) {
      // Do all of the declarations.
      for (const auto &RD : Definition->redecls())
        rewriteThese.insert(DAndReplace(RD, s, true));
      // Save the modified function signature.
      ModifiedFuncSignatures[FuncName] = s;
    }
  }

  return true;
}

bool TypeRewritingVisitor::VisitCallExpr(CallExpr *E) {
  return true;
}

// Check if the function is handled by this visitor.
bool TypeRewritingVisitor::isFunctionVisited(std::string FuncName) {
  return VisitedSet.find(FuncName) != VisitedSet.end();
}

static bool
canWrite(std::string filePath, std::set<std::string> &iof, std::string b) {
  // Was this file explicitly provided on the command line?
  if (iof.count(filePath) > 0)
    return true;
  // Is this file contained within the base directory?

  sys::path::const_iterator baseIt = sys::path::begin(b);
  sys::path::const_iterator pathIt = sys::path::begin(filePath);
  sys::path::const_iterator baseEnd = sys::path::end(b);
  sys::path::const_iterator pathEnd = sys::path::end(filePath);
  std::string baseSoFar = (*baseIt).str() + sys::path::get_separator().str();
  std::string pathSoFar = (*pathIt).str() + sys::path::get_separator().str();
  ++baseIt;
  ++pathIt;

  while ((baseIt != baseEnd) && (pathIt != pathEnd)) {
    sys::fs::file_status baseStatus;
    sys::fs::file_status pathStatus;
    std::string s1 = (*baseIt).str();
    std::string s2 = (*pathIt).str();

    if (std::error_code ec = sys::fs::status(baseSoFar, baseStatus))
      return false;

    if (std::error_code ec = sys::fs::status(pathSoFar, pathStatus))
      return false;

    if (!sys::fs::equivalent(baseStatus, pathStatus))
      break;

    if (s1 != sys::path::get_separator().str())
      baseSoFar += (s1 + sys::path::get_separator().str());
    if (s2 != sys::path::get_separator().str())
      pathSoFar += (s2 + sys::path::get_separator().str());

    ++baseIt;
    ++pathIt;
  }

  if (baseIt == baseEnd && baseSoFar == pathSoFar)
    return true;
  else
    return false;
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


// This Visitor adds _Checked and _UnChecked annotations to blocks
class CheckedRegionAdder : public clang::RecursiveASTVisitor<CheckedRegionAdder>
{
  public:
    explicit CheckedRegionAdder(ASTContext *_C, Rewriter& _R, ProgramInfo &_I,
                                std::set<llvm::FoldingSetNodeID> &S)
      : Context(_C), Writer(_R), Info(_I), Seen(S) {

    }
    int Nwild = 0;
    int Nchecked = 0;
    int Ndecls = 0;

    bool VisitForStmt(ForStmt *S) {
      int Localwild = 0;

      for (const auto &SubStmt : S->children()) {
        CheckedRegionAdder Sub(Context,Writer,Info,Seen);
        Sub.TraverseStmt(SubStmt);
        Localwild += Sub.Nwild;
      }

      Nwild += Localwild;
      return false;
    }

    bool VisitSwitchStmt(SwitchStmt *S) {
      int Localwild = 0;

      for (const auto &SubStmt : S->children()) {
        CheckedRegionAdder Sub(Context,Writer,Info,Seen);
        Sub.TraverseStmt(SubStmt);
        Localwild += Sub.Nwild;
      }

      Nwild += Localwild;
      return false;
    }

    bool VisitIfStmt(IfStmt *S) {
      int Localwild = 0;

      for (const auto &SubStmt : S->children()) {
        CheckedRegionAdder Sub(Context,Writer,Info,Seen);
        Sub.TraverseStmt(SubStmt);
        Localwild += Sub.Nwild;
      }

      Nwild += Localwild;
      return false;
    }

    bool VisitWhileStmt(WhileStmt *S) {
      int Localwild = 0;

      for (const auto &SubStmt : S->children()) {
        CheckedRegionAdder Sub(Context,Writer,Info,Seen);
        Sub.TraverseStmt(SubStmt);
        Localwild += Sub.Nwild;
      }

      Nwild += Localwild;
      return false;
    }

    bool VisitDoStmt(DoStmt *S) {
      int Localwild = 0;

      for (const auto &subStmt : S->children()) {
        CheckedRegionAdder Sub(Context,Writer,Info,Seen);
        Sub.TraverseStmt(subStmt);
        Localwild += Sub.Nwild;
      }

      Nwild += Localwild;
      return false;
    }


    bool VisitCompoundStmt(CompoundStmt *S) {
      // Visit all subblocks, find all unchecked types
      int Localwild = 0;
      for (const auto &SubStmt : S->children()) {
        CheckedRegionAdder Sub(Context,Writer,Info,Seen);
        Sub.TraverseStmt(SubStmt);
        Localwild += Sub.Nwild;
        Nchecked += Sub.Nchecked;
        Ndecls += Sub.Ndecls;
      }

      addCheckedAnnotation(S, Localwild);

      // Compound Statements are always considered to have 0 wild types
      // This is because a compound statement marked w/ _Unchecked can live
      // inside a _Checked region.
      Nwild = 0;

      llvm::FoldingSetNodeID Id;
      S->Profile(Id, *Context, true);
      Seen.insert(Id);

      // Compound Statements should be the bottom of the visitor,
      // as it creates it's own sub-visitor.
      return false;
    }

    bool VisitCStyleCastExpr(CStyleCastExpr *E) {
      // TODO This is over cautious
      Nwild++;
      return true;
    }

    // Check if this compound statement is the body
    // to a function with unsafe parameters.
    bool hasUncheckedParameters(CompoundStmt *S) {
      const auto &Parents = Context->getParents(*S);
      if (Parents.empty()) {
        return false;
      }

      auto Parent = Parents[0].get<FunctionDecl>();
      if (!Parent) {
        return false;
      }

      int Localwild = 0;
      for (auto Child : Parent->parameters()) {
        CheckedRegionAdder Sub(Context,Writer,Info,Seen);
        Sub.TraverseParmVarDecl(Child);
        Localwild += Sub.Nwild;
      }

      return Localwild != 0 || Parent->isVariadic();
    }


    bool VisitUnaryOperator(UnaryOperator *U) {
      //TODO handle computing pointers
      if (U->getOpcode() == UO_AddrOf) {
        // wild++;
      }
      return true;
    }

    bool isInStatementPosition(CallExpr *C) {
      // First check if our parent is a compound statement
      const auto &Parents = Context->getParents(*C);
      if (Parents.empty()) {
        return false; // This case shouldn't happen,
                      // but if it does play it safe and mark WILD.
      }
      auto Parent = Parents[0].get<CompoundStmt>();
      if (Parent) {
        //Check if we are the only child
        int NumChilds = 0;
        for (const auto& child: Parent->children()) {
          NumChilds++;
        }
        return NumChilds > 1;
      } else {
        //TODO there are other statement positions
        //     besides child of compound stmt
        return false;
      }
    }

    bool VisitCallExpr(CallExpr *C) {
      auto FD = C->getDirectCallee();
      if (FD && FD->isVariadic()) {
        // If this variadic call is in statement positon, we can wrap in it
        // an unsafe block and avoid polluting the entire block as unsafe.
        // If it's not (as in it is used in an expression) then we fall back to
        // reporting an WILD value.
        if (isInStatementPosition(C)) {
          // Insert an _Unchecked block around the call
          auto Begin = C->getBeginLoc();
          Writer.InsertTextBefore(Begin, "_Unchecked { ");
          auto End = C->getEndLoc();
          Writer.InsertTextAfterToken(End, "; }");
        } else {
          // Call is inside an epxression, mark WILD.
          Nwild++;
        }
      }
      if (FD) {
        auto type = FD->getReturnType();
        if (type->isPointerType())
          Nwild++;
      }
      return true;
    }


    bool VisitVarDecl(VarDecl *VD) {
      // Check if the variable is WILD.
      bool FoundWild = false;
      std::set<ConstraintVariable*> CVSet = Info.getVariable(VD, Context);
      for (auto Cv : CVSet) {
        if (Cv->hasWild(Info.getConstraints().getVariables())) {
          FoundWild = true;
        }
      }

      if (FoundWild)
        Nwild++;


      // Check if the variable contains an unchecked type.
      if (isUncheckedPtr(VD->getType()))
        Nwild++;
      Ndecls++;
      return true;
    }

    bool VisitParmVarDecl(ParmVarDecl *PVD) {
      // Check if the variable is WILD.
      bool FoundWild = false;
      std::set<ConstraintVariable*> CVSet = Info.getVariable(PVD, Context);
      for (auto Cv : CVSet) {
	llvm::errs() << "\nCheckedRegion:\n";
        Cv->dump();
	llvm::errs() << "\n";
        if (Cv->hasWild(Info.getConstraints().getVariables())) {
          FoundWild = true;
        }
      }

      if (FoundWild)
        Nwild++;

      // Check if the variable is a void*.
      if (isUncheckedPtr(PVD->getType()))
        Nwild++;
      Ndecls++;
      return true;
    }

    bool isUncheckedPtr(QualType Qt) {
      // TODO does a more efficient representation exist?
      std::set<std::string> Seen;
      return isUncheckedPtrAcc(Qt, Seen);
    }

    // Recursively determine if a type is unchecked.
    bool isUncheckedPtrAcc(QualType Qt, std::set<std::string> &Seen) {
      auto Ct = Qt.getCanonicalType();
      auto TyStr = Ct.getAsString();
      auto Search = Seen.find(TyStr);
      if (Search == Seen.end()) {
        Seen.insert(TyStr);
      } else {
        return false;
      }

      if (Ct->isVoidPointerType()) {
        return true;
      } else if (Ct->isVoidType()) {
        return true;
      } if (Ct->isPointerType()) {
        return isUncheckedPtrAcc(Ct->getPointeeType(), Seen);
      } else if (Ct->isRecordType()) {
        return isUncheckedStruct(Ct, Seen);
      } else {
        return false;
      }
    }

    // Iterate through all fields of the struct and find unchecked types.
    // TODO doesn't handle recursive structs correctly
    bool isUncheckedStruct(QualType Qt, std::set<std::string> &Seen) {
      auto RcdTy = dyn_cast<RecordType>(Qt);
      if (RcdTy) {
        auto D = RcdTy->getDecl();
        if (D) {
          bool Unsafe = false;
          for (auto const &Fld : D->fields()) {
            auto Ftype = Fld->getType();
            Unsafe |= isUncheckedPtrAcc(Ftype, Seen);
            std::set<ConstraintVariable*> CVSet =
                Info.getVariable(Fld, Context);
            for (auto Cv : CVSet) {
              Unsafe |= Cv->hasWild(Info.getConstraints().getVariables());
            }
          }
          return Unsafe;
        } else {
          return true;
        }
      } else {
        return false;
      }
    }

    void addCheckedAnnotation(CompoundStmt *S, int Localwild) {
      auto Cur = S->getWrittenCheckedSpecifier();

      llvm::FoldingSetNodeID Id;
      S->Profile(Id, *Context, true);
      auto Search = Seen.find(Id);

      if (Search == Seen.end()) {
        auto Loc = S->getBeginLoc();
        bool IsChecked = !hasUncheckedParameters(S) &&
                       Cur == CheckedScopeSpecifier::CSS_None && Localwild == 0;

        // Don't add _Unchecked to top level functions.
        if (!(!IsChecked && isFunctionBody(S))) {
          Writer.InsertTextBefore(Loc, IsChecked ? "_Checked" : "_Unchecked");
        }
      }
    }

    bool isFunctionBody(CompoundStmt *S) {
      const auto &Parents = Context->getParents(*S);
      if (Parents.empty()) {
        return false;
      }
      return Parents[0].get<FunctionDecl>();
    }


    bool VisitMemberExpr(MemberExpr *E){
      ValueDecl *VD = E->getMemberDecl();
      if (VD) {
        // Check if the variable is WILD.
        bool FoundWild = false;
        std::set<ConstraintVariable*> CVSet = Info.getVariable(VD, Context);
        for (auto Cv : CVSet) {
          if (Cv->hasWild(Info.getConstraints().getVariables())) {
            FoundWild = true;
          }
        }

        if (FoundWild)
          Nwild++;

        // Check if the variable is a void*.
        if (isUncheckedPtr(VD->getType()))
          Nwild++;
        Ndecls++;
      }
      return true;
    }

  private:
    ASTContext *Context;
    Rewriter &Writer;
    ProgramInfo &Info;
    std::set<llvm::FoldingSetNodeID> &Seen;
};

// Class for visiting variable usages and function calls to add
// explicit casting if needed.
class CastPlacementVisitor :
    public RecursiveASTVisitor<CastPlacementVisitor> {
public:
  explicit CastPlacementVisitor(ASTContext *C, ProgramInfo &I,
                                Rewriter& R)
      : Context(C), Info(I), Writer(R) {}

  bool VisitCallExpr(CallExpr *CE) {
    Decl *D = CE->getCalleeDecl();
    if (D) {
      PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(CE, *Context);
      if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
        // Get the constraint variable for the function.
        std::set<ConstraintVariable *> &V = Info.getFuncDefnConstraints(FD, Context);
        // TODO Deubgging lines
        // llvm::errs() << "Decl for: " << FD->getNameAsString() << "\nVars:";
        // for (auto &CV : V) {
        //   CV->dump();
        //   llvm::errs() << "\n";
        // }

        // Did we see this function in another file?
        auto Fname = FD->getNameAsString();
        auto PInfo = Info.get_MF()[Fname];

        if (V.size() > 0) {
          // Get the FV constraint for the Callee.
          FVConstraint *FV = nullptr;
          for (const auto &C : V) {
            if (PVConstraint * PVC = dyn_cast<PVConstraint>(C)) {
              if (FVConstraint * F = PVC->getFV()) {
                FV = F;
                break;
              }
            } else if (FVConstraint * FVC = dyn_cast<FVConstraint>(C)) {
              FV = FVC;
              break;
            }
          }
          // Now we need to check the type of the arguments and corresponding
          // parameters to see, if any explicit casting is needed.
          if (FV) {
            unsigned i = 0;
            for (const auto &A : CE->arguments()) {
              if (i < FD->getNumParams()) {
                std::set<ConstraintVariable *> ArgumentConstraints =
                    Info.getVariable(A, Context, true);
                std::set<ConstraintVariable *> &ParameterConstraints =
                    FV->getParamVar(i);
                bool CastInserted = false;
                for (auto *ArgumentC : ArgumentConstraints) {
                  CastInserted = false;
                  for (auto *ParameterC : ParameterConstraints) {
                    auto Dinfo = i < PInfo.size() ? PInfo[i] : CHECKED;
                    if (needCasting(ArgumentC, ParameterC, Dinfo)) {
                      // We expect the cast string to end with "(".
                      std::string CastString =
                          getCastString(ArgumentC, ParameterC, Dinfo);
                      Writer.InsertTextBefore(A->getBeginLoc(), CastString);
                      Writer.InsertTextAfterToken(A->getEndLoc(), ")");
                      CastInserted = true;
                      break;
                    }
                  }
                  // If we have already inserted a cast, then break.
                  if (CastInserted) break;
                }

              }
              i++;
            }
          }
        }
      }
    }
    return true;
  }

  bool VisitBinAssign(BinaryOperator *O) {
    // TODO: Aron try to add cast to RHS expression..if applicable.
    return true;
  }

  
private:
  // Check whether an explicit casting is needed when the pointer represented
  // by src variable is assigned to dst.
  bool needCasting(ConstraintVariable *Src, ConstraintVariable *Dst,
                   IsChecked Dinfo) {
    auto &E = Info.getConstraints().getVariables();
    auto SrcChecked = Src->anyChanges(E);
    // Check if the src is a checked type and destination is not.
    return (SrcChecked && !Dst->anyChanges(E)) ||
           (SrcChecked && Dinfo == WILD);
  }

  // Get the type name to insert for casting.
  std::string getCastString(ConstraintVariable *Src, ConstraintVariable *Dst,
                            IsChecked Dinfo) {
    assert(needCasting(Src, Dst, Dinfo) && "No casting needed.");
    auto &E = Info.getConstraints().getVariables();
    // The destination type should be a non-checked type.
    assert(!Dst->anyChanges(E) || Dinfo == WILD);
    return "((" + Dst->getRewritableOriginalTy() + ")";
  }
  ASTContext            *Context;
  ProgramInfo           &Info;
  Rewriter&    Writer;

};

// This class initializes all the structure variables that
// contains at least one checked pointer?
class StructVariableInitializer :
    public clang::RecursiveASTVisitor<StructVariableInitializer>
{
public:
  explicit StructVariableInitializer(ASTContext *_C, ProgramInfo &_I, RSet &R)
    : Context(_C), I(_I), RewriteThese(R)
  {
    RecordsWithCPointers.clear();
  }

  bool VariableNeedsInitializer(VarDecl *VD, DeclStmt *S) {
    RecordDecl *RD = VD->getType().getTypePtr()->getAsRecordDecl();
    if (RecordDecl *Definition = RD->getDefinition()) {
      // See if we already know that this structure has a checked pointer.
      if (RecordsWithCPointers.find(Definition) !=
          RecordsWithCPointers.end()) {
        return true;
      }
      for (const auto &D : Definition->fields()) {
        if (D->getType()->isPointerType() || D->getType()->isArrayType()) {
          std::set<ConstraintVariable *> FieldConsVars =
              I.getVariable(D, Context, false);
          for (auto CV: FieldConsVars) {
            PVConstraint *PV = dyn_cast<PVConstraint>(CV);
            if (PV && PV->anyChanges(I.getConstraints().getVariables())) {
              // Ok this contains a pointer that is checked.
              // Store it.
              RecordsWithCPointers.insert(Definition);
              return true;
            }
          }
        }
      }
    }
    return false;
  }

  // Check to see if this variable require an initialization.
  bool VisitDeclStmt(DeclStmt *S) {

    std::set<VarDecl*> AllDecls;

    if (S->isSingleDecl()) {
      if (VarDecl *VD = dyn_cast<VarDecl>(S->getSingleDecl())) {
        AllDecls.insert(VD);
      }
    } else {
      for (const auto &D : S->decls()) {
        if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
          AllDecls.insert(VD);
        }
      }
    }

    for (auto VD: AllDecls) {
      // Check if this variable is a structure or union and
      // doesn't have an initializer.
      if (!VD->hasInit() && isStructOrUnionType(VD)) {
        // Check if the variable needs a initializer.
        if (VariableNeedsInitializer(VD, S)) {
          const clang::Type *Ty = VD->getType().getTypePtr();
          std::string OriginalType = tyToStr(Ty);
          // Create replacement text with an initializer.
          std::string ToReplace = OriginalType + " " +
                                  VD->getName().str() + " = {}";
          RewriteThese.insert(DAndReplace(VD, S, ToReplace));
        }
      }
    }

    return true;
  }
private:
  ASTContext*  Context;
  ProgramInfo &I;
  RSet &RewriteThese;
  std::set<RecordDecl*> RecordsWithCPointers;

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

void ArrayBoundsRewriter::computeArrayBounds() {
  HandleArrayVariablesBoundsDetection(Context, Info);
}

std::string ArrayBoundsRewriter::getBoundsString(Decl *D, bool Isitype) {
  std::string BString = "";
  std::string BVarString = "";
  auto &ArrBInfo = Info.getArrayBoundsInformation();

  if (ArrBInfo.hasBoundsInformation(D))
    BVarString = ArrBInfo.getBoundsInformation(D).second;

  if (BVarString.length() > 0) {
    // For itype we do not need ":".
    if (!Isitype)
      BString = ":";
    BString += " count(" + BVarString + ")";
  }
  return BString;
}

void RewriteConsumer::HandleTranslationUnit(ASTContext &Context) {
  Info.enterCompilationUnit(Context);

  // Compute the bounds information for all the array variables.
  ArrayBoundsRewriter ABRewriter(&Context, Info);
  ABRewriter.computeArrayBounds();

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

  RSet skip(DComp(Context.getSourceManager()));
  MappingVisitor V(keys, Context);
  TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
  StructVariableInitializer FV =
      StructVariableInitializer(&Context, Info, RewriteThese);
  GlobalVariableGroups GVG(R.getSourceMgr());
  std::set<llvm::FoldingSetNodeID> seen;
  CheckedRegionAdder CRA(&Context, R, Info, seen);
  CastPlacementVisitor ECPV(&Context, Info, R);
  for (auto &D : TUD->decls()) {
    V.TraverseDecl(D);
    FV.TraverseDecl(D);
    ECPV.TraverseDecl(D);
    if (AddCheckedRegions)
      // Adding checked regions enabled!?
      CRA.TraverseDecl(D);
    GVG.addGlobalDecl(dyn_cast<VarDecl>(D));
  }

  std::tie(PSLMap, VDLToStmtMap) = V.getResults();

  for (const auto &V : Info.getVarMap()) {
    PersistentSourceLoc PLoc = V.first;
    std::set<ConstraintVariable*> Vars = V.second;
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
    clang::Type *T = nullptr;

    std::tie(S, D, T) = PSLMap[PLoc];

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
                            ABRewriter.getBoundsString(D);
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

  rewrite(R, RewriteThese, skip, Context.getSourceManager(),
          Context, Files, GVG);

  // Output files.
  emit(R, Context, Files, OutputPostfix);

  Info.exitCompilationUnit();
  return;
}
