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
#include <string.h>
#include "clang/AST/Type.h"
#include "RewriteUtils.h"
#include "MappingVisitor.h"
#include "Utils.h"
#include "ArrayBoundsInferenceConsumer.h"

using namespace llvm;
using namespace clang;

SourceRange DComp::getWholeSR(SourceRange Orig, DAndReplace Dr) const {
  SourceRange NewSrcRange(Orig);

  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(Dr.Declaration)) {
    NewSrcRange.setEnd(getFunctionDeclarationEnd(FD, SM));
    if (Dr.fullDecl == false)
      NewSrcRange = FD->getReturnTypeSourceRange();
  }

  return NewSrcRange;
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
    SourceLocation NewBegin = (*St->decls().begin())->getSourceRange().getBegin();
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

  bool Contained =  SM.isBeforeInTranslationUnit(X1, Y2) &&
                    SM.isBeforeInTranslationUnit(Y1, X2);

  if (Contained)
    return false;
  else
    return SM.isBeforeInTranslationUnit(X2, Y1);
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

  for (FunctionDecl *ToR = FD; ToR != nullptr; ToR = ToR->getPreviousDecl()) {
    int U = ToR->getNumParams();
    if (PIdx < U) {
      // TODO these declarations could get us into deeper
      // header files.
      ParmVarDecl *Rewrite = ToR->getParamDecl(PIdx);
      assert(Rewrite != nullptr && "Invalid param decl.");
      SourceRange TR = Rewrite->getSourceRange();

      if (canRewrite(R, TR))
        R.ReplaceText(TR, SRewrite);
    }
  }
}

void rewrite( VarDecl               *VD,
              Rewriter              &R,
              std::string SRewrite,
              Stmt                  *WhereStmt,
              RSet                  &skip,
              const DAndReplace     &N,
              RSet                  &ToRewrite,
              ASTContext            &A)
{
  DeclStmt *Where = dyn_cast_or_null<DeclStmt>(WhereStmt);

  if (Where != nullptr) {
    if (Verbose) {
      errs() << "VarDecl at:\n";
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
      // There is no initializer, lets add one.
      if (isPointerType(VD)) {
        SRewrite = SRewrite + " = NULL";
      }
    }

    // Is it a variable type? This is the easy case, we can re-write it
    // locally, at the site of the declaration.
    if (Where->isSingleDecl()) {
      if (canRewrite(R, TR)) {
        R.ReplaceText(TR, SRewrite);
      } else {
        // This can happen if SR is within a macro. If that is the case,
        // maybe there is still something we can do because Decl refers
        // to a non-macro line.

        SourceRange possible(R.getSourceMgr().getExpansionLoc(TR.getBegin()),
                             VD->getLocation());

        if (canRewrite(R, possible)) {
          R.ReplaceText(possible, SRewrite);
          std::string newStr = " " + VD->getName().str();
          R.InsertTextAfter(VD->getLocation(), newStr);
        } else {
          if (Verbose) {
            errs() << "Still don't know how to re-write VarDecl\n";
            VD->dump();
            errs() << "at\n";
            Where->dump();
            errs() << "with " << SRewrite << "\n";
          }
        }
      }
    } else if (!(Where->isSingleDecl()) && skip.find(N) == skip.end()) {
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
        if (tmp.Statement == WhereStmt)
          RewritesForThisDecl.insert(tmp);
        ++I;
      }

      // Step 2: remove the original line from the program.
      SourceRange DR = Where->getSourceRange();
      R.RemoveText(DR);

      // Step 3: for each decl in the original, build up a new string
      //         and if the original decl was re-written, write that
      //         out instead (WITH the initializer).
      std::string NewMultiLineDeclS = "";
      raw_string_ostream NewMlDecl(NewMultiLineDeclS);
      for (const auto &DL : Where->decls()) {
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
        assert(VDL != nullptr && "Invalid decl.");

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
            if (isPointerType(VDL)) {
              NewMlDecl << " = NULL";
            }
          }
          NewMlDecl << ";\n";
        }
        else {
          DL->print(NewMlDecl);
          NewMlDecl << ";\n";
        }
      }

      // Step 4: Write out the string built up in step 3.
      R.InsertTextAfter(DR.getEnd(), NewMlDecl.str());

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
        Where->dump();
        errs() << "with " << N.Replacement << "\n";
      }
    }
  } else {
    if (Verbose) {
      errs() << "Don't know where to rewrite a VarDecl! ";
      VD->dump();
      errs() << "\n";
    }
  }
}

void rewrite( Rewriter              &R,
              RSet                  &ToRewrite,
              RSet                  &Skip,
              SourceManager         &S,
              ASTContext            &A,
              std::set<FileID>      &Files)
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
      assert(Where == nullptr && "Invalid statement.");
      rewrite(PV, R, N.Replacement);
    } else if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      rewrite(VD, R, N.Replacement, Where, Skip, N, ToRewrite, A);
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
std::set<unsigned int> CastPlacementVisitor::getParamsForExtern(std::string E) {
  return StringSwitch<std::set<unsigned int>>(E)
          .Case("free", {0})
          .Default(std::set<unsigned int>());
}

// Checks the bindings in the environment for all of the constraints
// associated with C and returns true if any of those constraints
// are WILD.
bool CastPlacementVisitor::anyTop(std::set<ConstraintVariable *> C) {
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

std::string CastPlacementVisitor::getExistingIType(ConstraintVariable *Declc,
                                                   ConstraintVariable *Defc,
                                                   FunctionDecl *FuncDecl) {
  std::string Ret = "";
  ConstraintVariable *T = Declc;
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
bool CastPlacementVisitor::VisitFunctionDecl(FunctionDecl *FD) {

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

  auto funcName = FD->getNameAsString();

  // Do we have a definition for this declaration?
  FunctionDecl *Definition = getDefinition(FD);
  FunctionDecl *Declaration = getDeclaration(FD);

  if (Definition == nullptr)
    return true;

  // Make sure we haven't visited this function name before, and that we
  // only visit it once.
  if (VisitedSet.find(funcName) != VisitedSet.end())
    return true;
  else
    VisitedSet.insert(funcName);

  FVConstraint *CDefn = dyn_cast<FVConstraint>(
    getHighest(Info.getVariableOnDemand(Definition, Context, true), Info));

  FVConstraint *CDecl = nullptr;
  // Get constraint variables for the declaration and the definition.
  // Those constraints should be function constraints.
  if (Declaration == nullptr) {
    // if there is no declaration?
    // get the on demand function variable constraint.
    auto FuncDefKey = Info.getUniqueFuncKey(Definition, Context);
    auto &DemandMap = Info.getOnDemandFuncDeclConstraintMap();
    if (DemandMap.find(FuncDefKey) != DemandMap.end()) {
      CDecl = dyn_cast<FVConstraint>(getHighest(DemandMap[FuncDefKey], Info));
    } else {
      CDecl = CDefn;
    }
  } else {
    CDecl = dyn_cast<FVConstraint>(
      getHighest(Info.getVariableOnDemand(Declaration, Context, false), Info));
  }

  assert(CDecl != nullptr);
  assert(CDefn != nullptr);

  if (CDecl->numParams() == CDefn->numParams()) {
    // Track whether we did any work and need to make a substitution or not.
    bool DidAny = CDecl->numParams() > 0;
    std::string S = "";
    std::vector<std::string> ParmStrs;
    // Compare parameters.
    for (unsigned i = 0; i < CDecl->numParams(); ++i) {
      auto Decl = getHighest(CDecl->getParamVar(i), Info);
      auto Defn = getHighest(CDefn->getParamVar(i), Info);
      assert(Decl);
      assert(Defn);

      // If this holds, then we want to insert a bounds safe interface.
      bool Constrained = Defn->anyChanges(Info.getConstraints().getVariables());
      // definition is more precise than declaration.
      // Section 5.3:
      // https://www.microsoft.com/en-us/research/uploads/prod/2019/05/checkedc-post2019.pdf
      if (Constrained && Decl->hasWild(Info.getConstraints().getVariables())) {
        // if definition is more precise
        // than declaration emit an itype
        std::string Ctype =
            Defn->mkString(Info.getConstraints().getVariables(), false, true);
        std::string Bi =  Defn->getRewritableOriginalTy() + Defn->getName() + " : itype("+
            Ctype + ABRewriter.getBoundsString(Definition->getParamDecl(i), true)+") ";
        ParmStrs.push_back(Bi);
      } else if (Constrained) {
        // both the declaration and definition are same
        // and they are safer than what was originally declared.
        // here we should emit a checked type!
        std::string Bi = Defn->mkString(Info.getConstraints().getVariables());

        // if there is no declaration?
        // check the itype in definition
        Bi = Bi + getExistingIType(Decl, Defn, Declaration) +
            ABRewriter.getBoundsString(Definition->getParamDecl(i));

        ParmStrs.push_back(Bi);
      } else {
        std::string Scratch = "";
        raw_string_ostream DeclText(Scratch);
        Definition->getParamDecl(i)->print(DeclText);
        ParmStrs.push_back(DeclText.str());
      }
    }

    // Compare returns.
    auto Decl = getHighest(CDecl->getReturnVars(), Info);
    auto Defn = getHighest(CDefn->getReturnVars(), Info);

    // Insert a bounds safe interface for the return.
    std::string ReturnVar = "";
    std::string EndStuff = "";
    bool ReturnHandled = false;
    bool anyConstrained = Defn->anyChanges(Info.getConstraints().getVariables());
    if (anyConstrained) {
      ReturnHandled = true;
      DidAny = true;
      std::string Ctype = "";
      DidAny = true;
      // definition is more precise than declaration.
      // Section 5.3:
      // https://www.microsoft.com/en-us/research/uploads/prod/2019/05/checkedc-post2019.pdf
      if (Decl->hasWild(Info.getConstraints().getVariables())) {
        Ctype = Defn->mkString(Info.getConstraints().getVariables(), true, true);
        ReturnVar = Defn->getRewritableOriginalTy();
        EndStuff = " : itype("+ Ctype +") ";
      } else {
        // this means we were able to infer that return type
        // is a checked type.
        // however, the function returns a less precise type, whereas
        // all the uses of the function converts the return value
        // into a more precise type.
        // do not change the type
        ReturnVar = Decl->mkString(Info.getConstraints().getVariables());
        EndStuff = getExistingIType(Decl, Defn, Declaration);
      }
    }

    // this means inside the function, the return value is WILD
    // so the return type is what was originally declared.
    if (!ReturnHandled) {
      // If we used to implement a bounds-safe interface, continue to do that.
      ReturnVar = Decl->getOriginalTy() + " ";

      EndStuff = getExistingIType(Decl, Defn, Declaration);
      if (!EndStuff.empty()) {
        DidAny = true;
      }
    }

    S = getStorageQualifierString(Definition) + ReturnVar + CDecl->getName() + "(";
    if (ParmStrs.size() > 0) {
      std::ostringstream Ss;

      std::copy(ParmStrs.begin(), ParmStrs.end() - 1,
                std::ostream_iterator<std::string>(Ss, ", "));
      Ss << ParmStrs.back();

      S = S + Ss.str();
      // add varargs
      if (functionHasVarArgs(Definition)) {
        S = S + ", ...";
      }
      S = S + ")";
    } else {
      S = S + "void)";
    }

    if (EndStuff.size() > 0)
      S = S + EndStuff;

    if (DidAny) {
      // Do all of the declarations.
      for (const auto &RD : Definition->redecls())
        rewriteThese.insert(DAndReplace(RD, S, true));
      // save the modified function signature.
      ModifiedFuncSignatures[funcName] = S;
    }
  }

  return true;
}

bool CastPlacementVisitor::VisitCallExpr(CallExpr *E) {
  return true;
}

// check if the function is handled by this visitor
bool CastPlacementVisitor::isFunctionVisited(std::string FName) {
  return VisitedSet.find(FName) != VisitedSet.end();
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
                 std::set<std::string> &InOutFiles, std::string &BaseDir,
                 std::string &OutputPostfix) {

  // Check if we are outputing to stdout or not, if we are, just output the
  // main file ID to stdout.
  if (true)
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

          if (canWrite(feAbsS, InOutFiles, base)) {
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
    explicit CheckedRegionAdder(ASTContext *_C, Rewriter &_R, ProgramInfo &_I,
                                std::set<llvm::FoldingSetNodeID> &seen)
      : Context(_C), Writer(_R), Info(_I), Seen(seen) {

    }
    int Nwild = 0;
    int Nchecked = 0;
    int Ndecls = 0;

    bool VisitCompoundStmt(CompoundStmt *S) {
      // Visit all subblocks, find all unchecked types
      int Localwild = 0;
      for (const auto &SubSt : S->children()) {
        CheckedRegionAdder Sub(Context,Writer,Info,Seen);
        Sub.TraverseStmt(SubSt);
        Localwild += Sub.Nwild;
        Nchecked += Sub.Nchecked;
        Ndecls += Sub.Ndecls;
      }

      addCheckedAnnotation(S, Localwild);

      // Compound Statements are always considered to have 0 wild types
      // This is because a compound statement marked w/ _Unchecked can live
      // inside a _Checked region
      Nwild = 0;

      llvm::FoldingSetNodeID Id;
      S->Profile(Id, *Context, true);
      Seen.insert(Id);

      // Compound Statements should be the bottom of the visitor,
      // as it creates it's own sub-visitor
      return false;
    }

    bool VisitCStyleCastExpr(CStyleCastExpr *E) {
      // TODO This is over cautious
      Nwild++;
      return true;
    }

    // Check if this compound statement is the body
    // to a function with unsafe parameters
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
      for (auto child : Parent->parameters()) {
        CheckedRegionAdder Sub(Context,Writer,Info,Seen);
        Sub.TraverseParmVarDecl(child);
        Localwild += Sub.Nwild;
      }

      return Localwild != 0 || Parent->isVariadic();
    }

    bool VisitUnaryOperator(UnaryOperator*U) {
      //TODO handle computing pointers
      if (U->getOpcode() == UO_AddrOf) {
        // wild++;
      }
      return true;
    }

    bool VisitCallExpr(CallExpr *C) {
      auto FD = C->getDirectCallee();
      if (FD && FD->isVariadic()) {
        Nwild++;
      }
      if (FD) {
        auto Rtype = FD->getReturnType();
        if (Rtype->isPointerType())
          Nwild++;
      }
      return true;
    }


    bool VisitVarDecl(VarDecl *VD) {
      // Check if the variable is WILD
      bool FoundWild = false;
      std::set<ConstraintVariable *> ConsVars = Info.getVariable(VD, Context);
      for (auto ConsVar : ConsVars) {
        if (ConsVar->hasWild(Info.getConstraints().getVariables())) {
          FoundWild = true;
        }
      }

      if (FoundWild)
        Nwild++;


      // Check if the variable contains an unchecked type
      if (isUncheckedPtr(VD->getType()))
        Nwild++;
      Ndecls++;
      return true;
    }

    bool VisitParmVarDecl(ParmVarDecl *PVD) {
      // Check if the variable is WILD
      bool FoundWild = false;
      std::set<ConstraintVariable *> CVSet = Info.getVariable(PVD, Context);
      for (auto CV : CVSet) {
        if (CV->hasWild(Info.getConstraints().getVariables())) {
          FoundWild = true;
        }
      }

      if (FoundWild)
        Nwild++;

      // Check if the variable is a void*
      if (isUncheckedPtr(PVD->getType()))
        Nwild++;
      Ndecls++;
      return true;
    }

    bool isUncheckedPtr(QualType Qt) {
      // TODO does a  more efficient representation exist?
      std::set<std::string> Seen;
      return isUncheckedPtrAcc(Qt, Seen);
    }

    // Recursively determine if a type is unchecked
    bool isUncheckedPtrAcc(QualType Qt, std::set<std::string> &Seen) {
      auto Ct = Qt.getCanonicalType();
      auto Id = Ct.getAsString();
      auto Search = Seen.find(Id);
      if (Search == Seen.end()) {
        Seen.insert(Id);
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

    // Iterate through all fields of the struct and find unchecked types
    // TODO doesn't handle recursive structs correctly
    bool isUncheckedStruct(QualType Qt, std::set<std::string> &Seen) {
      auto Rt = dyn_cast<RecordType>(Qt);
      if (Rt) {
        auto D = Rt->getDecl();
        if (D) {
          bool Unsafe = false;
          for (auto const &Fld : D->fields()) {
            auto Ftype = Fld->getType();
            Unsafe |= isUncheckedPtrAcc(Ftype, Seen);
            std::set<ConstraintVariable *> CvSet =
                Info.getVariable(Fld, Context);
            for (auto Cv : CvSet) {
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

        Writer.InsertTextBefore(Loc, IsChecked ? "_Checked" : "_Unchecked");
      }
    }


    bool VisitMemberExpr(MemberExpr *Me){
      ValueDecl *St = Me->getMemberDecl();
      if (St) {
        // Check if the variable is WILD
        bool FoundWild = false;
        std::set<ConstraintVariable *> CvSet = Info.getVariable(St, Context);
        for (auto Cv : CvSet) {
          if (Cv->hasWild(Info.getConstraints().getVariables())) {
            FoundWild = true;
          }
        }

        if (FoundWild)
          Nwild++;

        // Check if the variable is a void*
        if (isUncheckedPtr(St->getType()))
          Nwild++;
        Ndecls++;
      }
      return true;
    }

  private:
    ASTContext  *Context;
    Rewriter     &Writer;
    ProgramInfo    &Info;
    std::set<llvm::FoldingSetNodeID> &Seen;
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
      // see if we already know that this structure has a checked pointer.
      if (RecordsWithCPointers.find(Definition) != RecordsWithCPointers.end()) {
        return true;
      }
      for (const auto &D : Definition->fields()) {
        if (D->getType()->isPointerType() || D->getType()->isArrayType()) {
          std::set<ConstraintVariable *> FieldConsVars =
              I.getVariable(D, Context, false);
          for (auto CV : FieldConsVars) {
            PVConstraint *PV = dyn_cast<PVConstraint>(CV);
            if (PV && PV->anyChanges(I.getConstraints().getVariables())) {
              // ok this contains a pointer that is checked.
              // store it.
              RecordsWithCPointers.insert(Definition);
              return true;
            }
          }
        }
      }
    }
    return false;
  }

  // check to see if this variable require an initialization.
  bool VisitDeclStmt(DeclStmt *S) {

    std::set<VarDecl *> AllDecls;

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

    for (auto VD : AllDecls) {
      // check if this variable is a structure or union and doesn't have an initializer.
      if (!VD->hasInit() && isStructOrUnionType(VD)) {
        // check if the variable needs a initializer.
        if (VariableNeedsInitializer(VD, S)) {
          const clang::Type *Ty = VD->getType().getTypePtr();
          std::string OriginalType = tyToStr(Ty);
          // create replacement text with an initializer.
          std::string ToReplace = OriginalType + " " +
                                  VD->getName().str() + " = {}";
          RewriteThese.insert(DAndReplace(VD, S, ToReplace));
        }
      }
    }

    return true;
  }
private:
  ASTContext *Context;
  ProgramInfo &I;
  RSet &RewriteThese;
  std::set<RecordDecl *> RecordsWithCPointers;

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
    // for itype we do not need ":"
    if (!Isitype)
      BString = ":";
    BString += " count(" + BVarString + ")";
  }
  return BString;
}

void RewriteConsumer::HandleTranslationUnit(ASTContext &Context) {
  Info.enterCompilationUnit(Context);

  // compute the bounds information for all the array variables.
  ArrayBoundsRewriter ABRewriter(&Context, Info);
  ABRewriter.computeArrayBounds();

  Rewriter R(Context.getSourceManager(), Context.getLangOpts());
  std::set<FileID> Files;

  std::set<std::string> v;
  RSet RewriteThese(DComp(Context.getSourceManager()));
  // Unification is done, so visit and see if we need to place any casts
  // in the program.
  CastPlacementVisitor CPV =
      CastPlacementVisitor(&Context, Info, RewriteThese, v,
                           RewriteConsumer::ModifiedFuncSignatures,
                           ABRewriter);
  for (const auto &D : Context.getTranslationUnitDecl()->decls())
    CPV.TraverseDecl(D);

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
      StructVariableInitializer(&Context,
                                Info, RewriteThese);
  std::set<llvm::FoldingSetNodeID> seen;
  CheckedRegionAdder CRA(&Context, R, Info, seen);
  for (auto &D : TUD->decls()) {
    V.TraverseDecl(D);
    FV.TraverseDecl(D);
    CRA.TraverseDecl(D);
  }

  std::tie(PSLMap, VDLToStmtMap) = V.getResults();

  for (const auto &V : Info.getVarMap()) {
    PersistentSourceLoc PLoc = V.first;
    std::set<ConstraintVariable *> Vars = V.second;
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


      if (PV && PV->anyChanges(Info.getConstraints().getVariables()) && !PV->isPartOfFunctionPrototype()) {
        // Rewrite a declaration, only if it is not part of function prototype
        std::string newTy = getStorageQualifierString(D) + PV->mkString(Info.getConstraints().getVariables()) +
                            ABRewriter.getBoundsString(D);
        RewriteThese.insert(DAndReplace(D, DS, newTy));
      } else if (FV && RewriteConsumer::hasModifiedSignature(FV->getName()) &&
                 !CPV.isFunctionVisited(FV->getName())) {
        // if this function already has a modified signature? and it is not
        // visited by our cast placement visitor then rewrite it.
        std::string newSig = RewriteConsumer::getModifiedFuncSignature(FV->getName());
        RewriteThese.insert(DAndReplace(D, newSig, true));
      }
    }
  }

  rewrite(R, RewriteThese, skip, Context.getSourceManager(), Context, Files);

  // Output files.
  emit(R, Context, Files, InOutFiles, BaseDir, OutputPostfix);

  Info.exitCompilationUnit();
  return;
}
