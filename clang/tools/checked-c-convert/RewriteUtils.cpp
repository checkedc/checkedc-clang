//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
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

SourceRange DComp::getWholeSR(SourceRange orig, DAndReplace dr) const {
  SourceRange newSourceRange(orig);

  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(dr.Declaration)) {
    newSourceRange.setEnd(getFunctionDeclarationEnd(FD, SM));
    if (dr.fullDecl == false)
      newSourceRange = FD->getReturnTypeSourceRange();
  }

  return newSourceRange;
}

bool DComp::operator()(const DAndReplace lhs, const DAndReplace rhs) const {
  // Does the source location of the Decl in lhs overlap at all with
  // the source location of rhs?
  SourceRange srLHS = lhs.Declaration->getSourceRange();
  SourceRange srRHS = rhs.Declaration->getSourceRange();

  // Take into account whether or not a FunctionDeclaration specifies
  // the "whole" declaration or not. If it does not, it just specifies
  // the return position.
  srLHS = getWholeSR(srLHS, lhs);
  srRHS = getWholeSR(srRHS, rhs);

  // Also take into account whether or not there is a multi-statement
  // decl, because the generated ranges will overlap.
  DeclStmt *lhStmt = dyn_cast_or_null<DeclStmt>(lhs.Statement);

  if (lhStmt && !lhStmt->isSingleDecl()) {
    SourceLocation  newBegin = (*lhStmt->decls().begin())->getSourceRange().getBegin();
    bool            found;
    for (const auto &DT : lhStmt->decls()) {
      if (DT == lhs.Declaration) {
        found = true;
        break;
      }
      newBegin = DT->getSourceRange().getEnd();
    }
    assert (found);
    srLHS.setBegin(newBegin);
    // This is needed to make the subsequent test inclusive.
    srLHS.setEnd(srLHS.getEnd().getLocWithOffset(-1));
  }

  DeclStmt *rhStmt = dyn_cast_or_null<DeclStmt>(rhs.Statement);
  if (rhStmt && !rhStmt->isSingleDecl()) {
    SourceLocation  newBegin = (*rhStmt->decls().begin())->getSourceRange().getBegin();
    bool            found;
    for (const auto &DT : rhStmt->decls()) {
      if (DT == rhs.Declaration) {
        found = true;
        break;
      }
      newBegin = DT->getSourceRange().getEnd();
    }
    assert (found);
    srRHS.setBegin(newBegin);
    // This is needed to make the subsequent test inclusive.
    srRHS.setEnd(srRHS.getEnd().getLocWithOffset(-1));
  }

  SourceLocation x1 = srLHS.getBegin();
  SourceLocation x2 = srLHS.getEnd();
  SourceLocation y1 = srRHS.getBegin();
  SourceLocation y2 = srRHS.getEnd();

  bool contained =  SM.isBeforeInTranslationUnit(x1, y2) &&
                    SM.isBeforeInTranslationUnit(y1, x2);

  if (contained)
    return false;
  else
    return SM.isBeforeInTranslationUnit(x2, y1);
}

// Test to see if we can rewrite a given SourceRange.
// Note that R.getRangeSize will return -1 if SR is within
// a macro as well. This means that we can't re-write any
// text that occurs within a macro.
static bool canRewrite(Rewriter &R, SourceRange &SR) {
  return SR.isValid() && (R.getRangeSize(SR) != -1);
}

void rewrite(ParmVarDecl *PV, Rewriter &R, std::string sRewrite) {
  // First, find all the declarations of the containing function.
  DeclContext *DF = PV->getParentFunctionOrMethod();
  assert(DF != nullptr && "no parent function or method for decl");
  FunctionDecl *FD = cast<FunctionDecl>(DF);

  // For each function, determine which parameter in the declaration
  // matches PV, then, get the type location of that parameter
  // declaration and re-write.

  // This is kind of hacky, maybe we should record the index of the
  // parameter when we find it, instead of re-discovering it here.
  int parmIndex = -1;
  int c = 0;
  for (const auto &I : FD->parameters()) {
    if (I == PV) {
      parmIndex = c;
      break;
    }
    c++;
  }
  assert(parmIndex >= 0);

  for (FunctionDecl *toRewrite = FD; toRewrite != NULL;
       toRewrite = toRewrite->getPreviousDecl()) {
    int U = toRewrite->getNumParams();
    if (parmIndex < U) {
      // TODO these declarations could get us into deeper
      // header files.
      ParmVarDecl *Rewrite = toRewrite->getParamDecl(parmIndex);
      assert(Rewrite != NULL);
      SourceRange TR = Rewrite->getSourceRange();

      if (canRewrite(R, TR))
        R.ReplaceText(TR, sRewrite);
    }
  }
}

void rewrite( VarDecl               *VD,
              Rewriter              &R,
              std::string           sRewrite,
              Stmt                  *WhereStmt,
              RSet                  &skip,
              const DAndReplace     &N,
              RSet                  &toRewrite,
              ASTContext            &A)
{
  DeclStmt *Where = dyn_cast_or_null<DeclStmt>(WhereStmt);

  if (Where != NULL) {
    if (Verbose) {
      errs() << "VarDecl at:\n";
      Where->dump();
    }
    SourceRange TR = VD->getSourceRange();

    // Is there an initializer? If there is, change TR so that it points
    // to the START of the SourceRange of the initializer text, and drop
    // an '=' token into sRewrite.
    if (VD->hasInit()) {
      SourceLocation eqLoc = VD->getInitializerStartLoc();
      TR.setEnd(eqLoc);
      sRewrite = sRewrite + " = ";
    } else {
      // DAMN, there is no initializer, lets add it.
      if(isPointerType(VD)) {
        sRewrite = sRewrite + " = NULL";
      }
    }

    // Is it a variable type? This is the easy case, we can re-write it
    // locally, at the site of the declaration.
    if (Where->isSingleDecl()) {
      if (canRewrite(R, TR)) {
        R.ReplaceText(TR, sRewrite);
      } else {
        // This can happen if SR is within a macro. If that is the case,
        // maybe there is still something we can do because Decl refers
        // to a non-macro line.

        SourceRange possible(R.getSourceMgr().getExpansionLoc(TR.getBegin()),
                             VD->getLocation());

        if (canRewrite(R, possible)) {
          R.ReplaceText(possible, sRewrite);
          std::string newStr = " " + VD->getName().str();
          R.InsertTextAfter(VD->getLocation(), newStr);
        } else {
          if (Verbose) {
            errs() << "Still don't know how to re-write VarDecl\n";
            VD->dump();
            errs() << "at\n";
            Where->dump();
            errs() << "with " << sRewrite << "\n";
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
      RSet rewritesForThisDecl(DComp(R.getSourceMgr()));
      auto I = toRewrite.find(N);
      while (I != toRewrite.end()) {
        DAndReplace tmp = *I;
        if (tmp.Statement == WhereStmt)
          rewritesForThisDecl.insert(tmp);
        ++I;
      }

      // Step 2: remove the original line from the program.
      SourceRange DR = Where->getSourceRange();
      R.RemoveText(DR);

      // Step 3: for each decl in the original, build up a new string
      //         and if the original decl was re-written, write that
      //         out instead (WITH the initializer).
      std::string newMultiLineDeclS = "";
      raw_string_ostream newMLDecl(newMultiLineDeclS);
      for (const auto &DL : Where->decls()) {
        DAndReplace N;
        bool found = false;
        VarDecl *VDL = dyn_cast<VarDecl>(DL);
        if(VDL == nullptr) {
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
        assert(VDL != NULL);

        for (const auto &NLT : rewritesForThisDecl)
          if (NLT.Declaration == DL) {
            N = NLT;
            found = true;
            break;
          }

        if (found) {
          newMLDecl << N.Replacement;
          if (Expr *E = VDL->getInit()) {
            newMLDecl << " = ";
            E->printPretty(newMLDecl, nullptr, A.getPrintingPolicy());
          } else {
            if(isPointerType(VDL)) {
              newMLDecl << " = NULL";
            }
          }
          newMLDecl << ";\n";
        }
        else {
          DL->print(newMLDecl);
          newMLDecl << ";\n";
        }
      }

      // Step 4: Write out the string built up in step 3.
      R.InsertTextAfter(DR.getEnd(), newMLDecl.str());

      // Step 5: Be sure and skip all of the NewTyps that we dealt with
      //         during this time of hacking, by adding them to the
      //         skip set.

      for (const auto &TN : rewritesForThisDecl)
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
              RSet                  &toRewrite,
              RSet                  &skip,
              SourceManager         &S,
              ASTContext            &A,
              std::set<FileID>      &Files)
{
  for (const auto &N : toRewrite) {
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
      assert(Where == NULL);
      rewrite(PV, R, N.Replacement);
    } else if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      rewrite(VD, R, N.Replacement, Where, skip, N, toRewrite, A);
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
      std::string sRewrite = N.Replacement;

      if (canRewrite(R, SR))
        R.ReplaceText(SR, sRewrite);
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
bool CastPlacementVisitor::anyTop(std::set<ConstraintVariable*> C) {
  bool anyTopFound = false;
  Constraints &CS = Info.getConstraints();
  Constraints::EnvironmentMap &env = CS.getVariables();
  for (ConstraintVariable *c : C) {
    if (PointerVariableConstraint *pvc = dyn_cast<PointerVariableConstraint>(c)) {
      for (uint32_t v : pvc->getCvars()) {
        ConstAtom *CK = env[CS.getVar(v)];
        if (CK->getKind() == Atom::A_Wild) {
          anyTopFound = true;
        }
      }
    }
  }
  return anyTopFound;
}

std::string CastPlacementVisitor::getExistingIType(ConstraintVariable *decl,
                                                   ConstraintVariable *defn,
                                                   FunctionDecl *funcDecl) {
  std::string ret = "";
  ConstraintVariable *target = decl;
  if(funcDecl == nullptr) {
    target = defn;
  }
  if (PVConstraint *PVC = dyn_cast<PVConstraint>(target)) {
    if (PVC->getItypePresent()) {
      ret = " : " + PVC->getItype();
    }
  }
  return ret;
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

  if(Definition == nullptr)
    return true;

  // Make sure we haven't visited this function name before, and that we
  // only visit it once.
  if (VisitedSet.find(funcName) != VisitedSet.end())
    return true;
  else
    VisitedSet.insert(funcName);

  FVConstraint *cDefn = dyn_cast<FVConstraint>(
    getHighest(Info.getVariableOnDemand(Definition, Context, true), Info));

  FVConstraint *cDecl = nullptr;
  // Get constraint variables for the declaration and the definition.
  // Those constraints should be function constraints.
  if(Declaration == nullptr) {
    // if there is no declaration?
    // get the on demand function variable constraint.
    auto funcDefKey = Info.getUniqueFuncKey(Definition, Context);
    auto &onDemandMap = Info.getOnDemandFuncDeclConstraintMap();
    if(onDemandMap.find(funcDefKey) != onDemandMap.end()) {
      cDecl = dyn_cast<FVConstraint>(getHighest(onDemandMap[funcDefKey], Info));
    } else {
      cDecl = cDefn;
    }
  } else {
    cDecl = dyn_cast<FVConstraint>(
      getHighest(Info.getVariableOnDemand(Declaration, Context, false), Info));
  }

  assert(cDecl != nullptr);
  assert(cDefn != nullptr);

  if (cDecl->numParams() == cDefn->numParams()) {
    // Track whether we did any work and need to make a substitution or not.
    bool didAny = cDecl->numParams() > 0;
    std::string s = "";
    std::vector<std::string> parmStrs;
    // Compare parameters.
    for (unsigned i = 0; i < cDecl->numParams(); ++i) {
      auto Decl = getHighest(cDecl->getParamVar(i), Info);
      auto Defn = getHighest(cDefn->getParamVar(i), Info);
      assert(Decl);
      assert(Defn);

      // If this holds, then we want to insert a bounds safe interface.
      bool anyConstrained = Defn->anyChanges(Info.getConstraints().getVariables());
      // definition is more precise than declaration.
      // Section 5.3:
      // https://www.microsoft.com/en-us/research/uploads/prod/2019/05/checkedc-post2019.pdf
      if(anyConstrained && Decl->hasWild(Info.getConstraints().getVariables())) {
        // if definition is more precise
        // than declaration emit an itype
        std::string ctype = Defn->mkString(Info.getConstraints().getVariables(), false, true);
        std::string bi =  Defn->getRewritableOriginalTy() + Defn->getName() + " : itype("+ctype +
                          ABRewriter.getBoundsString(Definition->getParamDecl(i), true)+") ";
        parmStrs.push_back(bi);
      } else if (anyConstrained) {
        // both the declaration and definition are same
        // and they are safer than what was originally declared.
        // here we should emit a checked type!
        std::string v = Defn->mkString(Info.getConstraints().getVariables());

        // if there is no declaration?
        // check the itype in definition
        v = v + getExistingIType(Decl, Defn, Declaration) + ABRewriter.getBoundsString(Definition->getParamDecl(i));

        parmStrs.push_back(v);
      } else {
        std::string scratch = "";
        raw_string_ostream declText(scratch);
        Definition->getParamDecl(i)->print(declText);
        parmStrs.push_back(declText.str());
      }
    }

    // Compare returns.
    auto Decl = getHighest(cDecl->getReturnVars(), Info);
    auto Defn = getHighest(cDefn->getReturnVars(), Info);

    // Insert a bounds safe interface for the return.
    std::string returnVar = "";
    std::string endStuff = "";
    bool returnHandled = false;
    bool anyConstrained = Defn->anyChanges(Info.getConstraints().getVariables());
    if(anyConstrained) {
      returnHandled = true;
      didAny = true;
      std::string ctype = "";
      didAny = true;
      // definition is more precise than declaration.
      // Section 5.3:
      // https://www.microsoft.com/en-us/research/uploads/prod/2019/05/checkedc-post2019.pdf
      if(Decl->hasWild(Info.getConstraints().getVariables())) {
        ctype = Defn->mkString(Info.getConstraints().getVariables(), true, true);
        returnVar = Defn->getRewritableOriginalTy();
        endStuff = " : itype("+ctype+") ";
      } else {
        // this means we were able to infer that return type
        // is a checked type.
        // however, the function returns a less precise type, whereas
        // all the uses of the function converts the return value
        // into a more precise type.
        // do not change the type
        returnVar = Decl->mkString(Info.getConstraints().getVariables());
        endStuff = getExistingIType(Decl, Defn, Declaration);
      }
    }

    // this means inside the function, the return value is WILD
    // so the return type is what was originally declared.
    if(!returnHandled) {
      // If we used to implement a bounds-safe interface, continue to do that.
      returnVar = Decl->getOriginalTy() + " ";

      endStuff = getExistingIType(Decl, Defn, Declaration);
      if(!endStuff.empty()) {
        didAny = true;
      }
    }

    s = getStorageQualifierString(Definition) + returnVar + cDecl->getName() + "(";
    if (parmStrs.size() > 0) {
      std::ostringstream ss;

      std::copy(parmStrs.begin(), parmStrs.end() - 1,
                std::ostream_iterator<std::string>(ss, ", "));
      ss << parmStrs.back();

      s = s + ss.str();
      // add varargs
      if(functionHasVarArgs(Definition)) {
        s = s + ", ...";
      }
      s = s + ")";
    } else {
      s = s + "void)";
    }

    if (endStuff.size() > 0)
      s = s + endStuff;

    if (didAny) {
      // Do all of the declarations.
      for (const auto &RD : Definition->redecls())
        rewriteThese.insert(DAndReplace(RD, s, true));
      // save the modified function signature.
      ModifiedFuncSignatures[funcName] = s;
    }
  }

  return true;
}

bool CastPlacementVisitor::VisitCallExpr(CallExpr *E) {
  return true;
}

// check if the function is handled by this visitor
bool CastPlacementVisitor::isFunctionVisited(std::string funcName) {
  return VisitedSet.find(funcName) != VisitedSet.end();
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

  SmallString<254> baseAbs(BaseDir);
  std::string baseDirFP;
  if(getAbsoluteFilePath(BaseDir, baseDirFP)) {
    baseAbs = baseDirFP;
  }
  sys::path::remove_filename(baseAbs);
  std::string base = baseAbs.str();

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
          if(getAbsoluteFilePath(FE->getName(), feAbsS)) {
            feAbsS = sys::path::remove_leading_dotslash(feAbsS);
          }

          if(canWrite(feAbsS, InOutFiles, base)) {
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
    explicit CheckedRegionAdder(ASTContext *_C, Rewriter& _R, ProgramInfo &_I, std::set<llvm::FoldingSetNodeID>& seen)
      : Context(_C), Writer(_R), Info(_I), Seen(seen) {

    }
    int wild = 0;
    int checked = 0;
    int decls = 0;

    bool VisitCompoundStmt(CompoundStmt *s) {
      // Visit all subblocks, find all unchecked types
      int localwild = 0;
      for (const auto& subStmt : s->children()) {
        CheckedRegionAdder sub(Context,Writer,Info,Seen);
        sub.TraverseStmt(subStmt);
        localwild += sub.wild;
        checked += sub.checked;
        decls += sub.decls;
      }

      addCheckedAnnotation(s, localwild);

      // Compound Statements are always considered to have 0 wild types
      // This is because a compound statement marked w/ _Unchecked can live
      // inside a _Checked region
      wild = 0;

      llvm::FoldingSetNodeID id;
      s->Profile(id, *Context, true);
      Seen.insert(id);

      // Compound Statements should be the bottom of the visitor,
      // as it creates it's own sub-visitor
      return false;
    }

    bool VisitCStyleCastExpr(CStyleCastExpr *e) {
      // TODO This is over cautious
      auto type = e->getTypeAsWritten();
      wild++;
      return true;
    }

    // Check if this compound statement is the body
    // to a function with unsafe parameters
    bool hasUncheckedParameters(CompoundStmt *s) {
      const auto& parents = Context->getParents(*s);
      if (parents.empty()) {
        return false;
      }

      auto parent = parents[0].get<FunctionDecl>();
      if(!parent) {
        return false;
      }

      int localwild = 0;
      for (auto child : parent->parameters()) {
        CheckedRegionAdder sub(Context,Writer,Info,Seen);
        sub.TraverseParmVarDecl(child);
        localwild += sub.wild;
      }

      return localwild != 0 || parent->isVariadic();
    }

    bool VisitUnaryOperator(UnaryOperator* u) {
      //TODO handle computing pointers
      if (u->getOpcode() == UO_AddrOf) {
        // wild++;
      }
      return true;
    }

    bool VisitCallExpr(CallExpr *c) {
      auto FD = c->getDirectCallee();
      if (FD && FD->isVariadic()) {
        wild++;
      }
      if (FD) {
        auto type = FD->getReturnType();
        if (type->isPointerType()) wild++;
      }
      return true;
    }


    bool VisitVarDecl(VarDecl *st) {
      // Check if the variable is WILD
      bool foundWild = false;
      std::set<ConstraintVariable*> cvs = Info.getVariable(st, Context);
      for (auto cv : cvs) {
        if (cv->hasWild(Info.getConstraints().getVariables())) {
          foundWild = true;
        }
      }

      if (foundWild) wild++;


      // Check if the variable is a void*
      if (isUncheckedPtr(st->getType())) wild++;
      decls++;
      return true;
    }

    bool VisitParmVarDecl(ParmVarDecl *st) {
      // Check if the variable is WILD
      bool foundWild = false;
      std::set<ConstraintVariable*> cvs = Info.getVariable(st, Context);
      for (auto cv : cvs) {
        if (cv->hasWild(Info.getConstraints().getVariables())) {
          foundWild = true;
        }
      }

      if (foundWild) wild++;

      // Check if the variable is a void*
      if (isUncheckedPtr(st->getType())) wild++;
      decls++;
      return true;
    }

    // Recursively determine if a type is unchecked
    bool isUncheckedPtr(QualType t) {
      auto ct = t.getCanonicalType();
      if (ct->isVoidType()) {
        return true;
      } if (ct->isPointerType()) {
        return isUncheckedPtr(ct->getPointeeType());
      } else if (ct->isRecordType()) {
        return false;
        //return isUncheckedStruct(ct);
      } else {
        return false;
      }
    }

    // Iterate through all fields of the struct and find unchecked types
    // TODO doesn't handle recursive structs correctly
    bool isUncheckedStruct(QualType t) {
      auto rt = dyn_cast<RecordType>(t);
      if (rt) {
        auto decl = rt->getDecl();
        if (decl) {
          bool unsafe = false;
          for (auto const&field : decl->fields()) {
            auto field_type = field->getType();
            unsafe |= isUncheckedPtr(field_type);
            std::set<ConstraintVariable*> cvs = Info.getVariable(field, Context);
            for (auto cv : cvs) {
              unsafe |= cv->hasWild(Info.getConstraints().getVariables());
            }
          }
          return unsafe;
        } else {
          return true;
        }
      } else {
        return false;
      }
    }

    void addCheckedAnnotation(CompoundStmt *s, int localwild) {
      auto cur = s->getWrittenCheckedSpecifier();

      llvm::FoldingSetNodeID id;
      s->Profile(id, *Context, true);
      auto search = Seen.find(id);

      if (search == Seen.end()) {
        auto loc = s->getBeginLoc();
        bool checked = !hasUncheckedParameters(s) &&
          cur == CheckedScopeSpecifier::CSS_None &&
          localwild == 0;

        Writer.InsertTextBefore(loc, checked ? "_Checked" : "_Unchecked");
      }
    }


    bool VisitMemberExpr(MemberExpr *me){
      ValueDecl* st = me->getMemberDecl();
      if(st) {
        // Check if the variable is WILD
        bool foundWild = false;
        std::set<ConstraintVariable*> cvs = Info.getVariable(st, Context);
        for (auto cv : cvs) {
          if (cv->hasWild(Info.getConstraints().getVariables())) {
            foundWild = true;
          }
        }

        if (foundWild) wild++;

        // Check if the variable is a void*
        if (isUncheckedPtr(st->getType())) wild++;
        decls++;
      }
      return true;
    }

  private:
    ASTContext*  Context;
    Rewriter&    Writer;
    ProgramInfo& Info;
    std::set<llvm::FoldingSetNodeID>& Seen;
};







// This is a visitor that tries to find all the variables
// inferred as arrayed by the checked-c-convert
class DeclArrayVisitor : public clang::RecursiveASTVisitor<DeclArrayVisitor>
{
public:
  explicit DeclArrayVisitor(ASTContext *_C, Rewriter& _R, ProgramInfo& _I)
          : Context(_C), Writer(_R), Info(_I)
  {
  }

  bool VisitDecl(Decl* D)
  {
    // check if this is a variable declaration.
    VarDecl* VD = dyn_cast_or_null<clang::VarDecl>(D);
    if (!VD)
      return true;

    // ProgramInfo.getVariable() can find variables in a function
    // context or not.  I'm not clear of the difference yet, so we
    // just run our analysis on both.

    std::set<ConstraintVariable*> a;
    // check if the function body exists before
    // fetching inbody variable.
    if(hasFunctionBody(D)) {
      a = Info.getVariable(D, Context, true);
    }

    std::set<ConstraintVariable*> b = Info.getVariable(D, Context, false);
    std::set<ConstraintVariable*> CV;
    std::set_union(a.begin(), a.end(),
                   b.begin(), b.end(),
                   std::inserter(CV, CV.begin()));

    bool foundArr = false;
    for (const auto& C: CV) {
      foundArr |= C->hasArr(Info.getConstraints().getVariables());
    }

    if (foundArr) {
      // Find the end of the line that contains this statement.
      FullSourceLoc sl(D->getEndLoc(), Context->getSourceManager());
      const char* buf = sl.getCharacterData();
      const char* ptr = strchr(buf, '\n');

      // Deal with Windows/DOS "\r\n" line endings.
      if (ptr && ptr > buf && ptr[-1] == '\r')
        --ptr;

      if (ptr) {
        SourceLocation eol = D->getEndLoc().getLocWithOffset(ptr-buf);
        sl = FullSourceLoc(eol, Context->getSourceManager());
        Writer.InsertTextBefore(eol, "*/");
        Writer.InsertTextBefore(eol, VD->getName());
        Writer.InsertTextBefore(eol, "/*ARR:");
      }
    }
    return true;
  }

private:
  ASTContext*  Context;
  Rewriter&    Writer;
  ProgramInfo& Info;
};

// This class initializes all the structure variables that
// contains at least one checked pointer?
class StructVariableInitializer : public clang::RecursiveASTVisitor<StructVariableInitializer>
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
      if(RecordsWithCPointers.find(Definition) != RecordsWithCPointers.end()) {
        return true;
      }
      for (const auto &D : Definition->fields()) {
        if (D->getType()->isPointerType() || D->getType()->isArrayType()) {
          std::set<ConstraintVariable *> fieldConsVars = I.getVariable(D, Context, false);
          for (auto CV: fieldConsVars) {
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

    std::set<VarDecl*> allDecls;

    if (S->isSingleDecl()) {
      if (VarDecl *VD = dyn_cast<VarDecl>(S->getSingleDecl())) {
        allDecls.insert(VD);
      }
    } else {
      for (const auto &D : S->decls()) {
        if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
          allDecls.insert(VD);
        }
      }
    }

    for(auto VD: allDecls) {
      // check if this variable is a structure or union and doesn't have an initializer.
      if(!VD->hasInit() && isStructOrUnionType(VD)) {
        // check if the variable needs a initializer.
        if(VariableNeedsInitializer(VD, S)) {
          const clang::Type *Ty = VD->getType().getTypePtr();
          std::string OriginalType = tyToStr(Ty);
          // create replacement text with an initializer.
          std::string toReplace = OriginalType + " " + VD->getName().str() + " = {}";
          RewriteThese.insert(DAndReplace(VD, S, toReplace));
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

std::string RewriteConsumer::getModifiedFuncSignature(std::string funcName) {
  if(RewriteConsumer::ModifiedFuncSignatures.find(funcName) != RewriteConsumer::ModifiedFuncSignatures.end()) {
    return RewriteConsumer::ModifiedFuncSignatures[funcName];
  }
  return "";
}

bool RewriteConsumer::hasModifiedSignature(std::string funcName) {
  return RewriteConsumer::ModifiedFuncSignatures.find(funcName) != RewriteConsumer::ModifiedFuncSignatures.end();
}

void ArrayBoundsRewriter::computeArrayBounds() {
  HandleArrayVariablesBoundsDetection(Context, Info);
}

std::string ArrayBoundsRewriter::getBoundsString(Decl *decl, bool isitype) {
  std::string boundsString = "";
  std::string boundVarString = "";
  auto &arrBoundsInfo = Info.getArrayBoundsInformation();

  if (arrBoundsInfo.hasBoundsInformation(decl))
    boundVarString = arrBoundsInfo.getBoundsInformation(decl).second;

  if (boundVarString.length() > 0) {
    // for itype we do not need ":"
    if (!isitype)
      boundsString = ":";
    boundsString += " count(" + boundVarString + ")";
  }
  return boundsString;
}

void RewriteConsumer::HandleTranslationUnit(ASTContext &Context) {
  Info.enterCompilationUnit(Context);

  // compute the bounds information for all the array variables.
  ArrayBoundsRewriter ABRewriter(&Context, Info);
  ABRewriter.computeArrayBounds();

  Rewriter R(Context.getSourceManager(), Context.getLangOpts());
  std::set<FileID> Files;

  std::set<std::string> v;
  RSet rewriteThese(DComp(Context.getSourceManager()));
  // Unification is done, so visit and see if we need to place any casts
  // in the program.
  CastPlacementVisitor CPV = CastPlacementVisitor(&Context, Info, rewriteThese, v,
                                                  RewriteConsumer::ModifiedFuncSignatures, ABRewriter);
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
  StructVariableInitializer FV = StructVariableInitializer(&Context, Info, rewriteThese);
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


      if (PV && PV->anyChanges(Info.getConstraints().getVariables()) && !PV->isPartOfFunctionPrototype()) {
        // Rewrite a declaration, only if it is not part of function prototype
        std::string newTy = getStorageQualifierString(D) + PV->mkString(Info.getConstraints().getVariables()) +
                            ABRewriter.getBoundsString(D);
        rewriteThese.insert(DAndReplace(D, DS, newTy));
      } else if (FV && RewriteConsumer::hasModifiedSignature(FV->getName()) &&
                 !CPV.isFunctionVisited(FV->getName())) {
        // if this function already has a modified signature? and it is not
        // visited by our cast placement visitor then rewrite it.
        std::string newSig = RewriteConsumer::getModifiedFuncSignature(FV->getName());
        rewriteThese.insert(DAndReplace(D, newSig, true));
      }
    }
  }

  rewrite(R, rewriteThese, skip, Context.getSourceManager(), Context, Files);

  // Output files.
  emit(R, Context, Files, InOutFiles, BaseDir, OutputPostfix);

  Info.exitCompilationUnit();
  return;
}
