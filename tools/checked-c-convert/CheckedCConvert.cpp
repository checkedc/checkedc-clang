//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Option/OptTable.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetSelect.h"

#include <algorithm>
#include <map>
#include <sstream>

#include "Constraints.h"

#include "ConstraintBuilder.h"
#include "PersistentSourceLoc.h"
#include "ProgramInfo.h"
#include "MappingVisitor.h"

using namespace clang::driver;
using namespace clang::tooling;
using namespace clang;
using namespace llvm;

static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::extrahelp MoreHelp("");

static cl::OptionCategory ConvertCategory("checked-c-convert options");
cl::opt<bool> DumpIntermediate( "dump-intermediate",
                                cl::desc("Dump intermediate information"),
                                cl::init(false),
                                cl::cat(ConvertCategory));

cl::opt<bool> Verbose("verbose",
                      cl::desc("Print verbose information"),
                      cl::init(false),
                      cl::cat(ConvertCategory));

static cl::opt<std::string>
    OutputPostfix("output-postfix",
                  cl::desc("Postfix to add to the names of rewritten files, if "
                           "not supplied writes to STDOUT"),
                  cl::init("-"), cl::cat(ConvertCategory));

static cl::opt<bool> DumpStats( "dump-stats",
                                cl::desc("Dump statistics"),
                                cl::init(false),
                                cl::cat(ConvertCategory));

static cl::opt<std::string>
BaseDir("base-dir",
  cl::desc("Base directory for the code we're translating"),
  cl::init(""),
  cl::cat(ConvertCategory));

const clang::Type *getNextTy(const clang::Type *Ty) {
  if(Ty->isPointerType()) {
    // TODO: how to keep the qualifiers around, and what qualifiers do
    //       we want to keep?
    QualType qtmp = Ty->getLocallyUnqualifiedSingleStepDesugaredType();
    return qtmp.getTypePtr()->getPointeeType().getTypePtr();
  }
  else
    return Ty;
}

// Test to see if we can rewrite a given SourceRange. 
// Note that R.getRangeSize will return -1 if SR is within
// a macro as well. This means that we can't re-write any 
// text that occurs within a macro.
bool canRewrite(Rewriter &R, SourceRange &SR) {
  return SR.isValid() && (R.getRangeSize(SR) != -1);
}

ConstraintVariable *getHighest(std::set<ConstraintVariable*> Vs, ProgramInfo &Info) {
  if (Vs.size() == 0)
    return nullptr;

  ConstraintVariable *V = nullptr;

  for (auto &P : Vs) {
    if (V) {
      if (V->isLt(*P, Info))
        V = P;
    } else {
      V = P;
    }
  }

  return V;
}

// Walk the list of declarations and find a declaration accompanied by 
// a definition and a function body. 
FunctionDecl *getDefinition(FunctionDecl *FD) {
  for (const auto &D : FD->redecls())
    if (FunctionDecl *tFD = dyn_cast<FunctionDecl>(D))
      if (tFD->isThisDeclarationADefinition() && tFD->hasBody())
        return tFD;

  return nullptr;
}

// Walk the list of declarations and find a declaration that is NOT 
// a definition and does NOT have a body. 
FunctionDecl *getDeclaration(FunctionDecl *FD) {
  for (const auto &D : FD->redecls())
    if (FunctionDecl *tFD = dyn_cast<FunctionDecl>(D))
      if (!tFD->isThisDeclarationADefinition())
        return tFD;

  return FD;
}

// A Declaration, optional DeclStmt, and a replacement string
// for that Declaration. 
struct DAndReplace
{
  Decl        *Declaration; // The declaration to replace.
  Stmt        *Statement;   // The Stmt, if it exists.
  std::string Replacement;  // The string to replace the declaration with.
  bool        fullDecl;     // If the declaration is a function, true if 
                            // replace the entire declaration or just the 
                            // return declaration.
  DAndReplace() : Declaration(nullptr),
                  Statement(nullptr),
                  Replacement(""),
                  fullDecl(false) { }

  DAndReplace(Decl *D, std::string R) : Declaration(D), 
                                        Statement(nullptr),
                                        Replacement(R),
                                        fullDecl(false) {} 

  DAndReplace(Decl *D, std::string R, bool F) : Declaration(D), 
                                                Statement(nullptr),
                                                Replacement(R),
                                                fullDecl(F) {} 


  DAndReplace(Decl *D, Stmt *S, std::string R) :  Declaration(D),
                                                  Statement(S),
                                                  Replacement(R),
                                                  fullDecl(false) { }
};

SourceLocation 
getFunctionDeclarationEnd(FunctionDecl *FD, SourceManager &S)
{        
  const FunctionDecl *oFD = nullptr;

  if (FD->hasBody(oFD) && oFD == FD) { 
    // Replace everything up to the beginning of the body. 
    const Stmt *Body = FD->getBody(oFD); 
 
    int Offset = 0; 
    const char *Buf = S.getCharacterData(Body->getSourceRange().getBegin());

    while (*Buf != ')') {
      Buf--;
      Offset--;
    }

    return Body->getSourceRange().getBegin().getLocWithOffset(Offset);
  } else {
    return FD->getSourceRange().getEnd();
  }
}

// Compare two DAndReplace values. The algorithm for comparing them relates 
// their source positions. If two DAndReplace values refer to overlapping 
// source positions, then they are the same. Otherwise, they are ordered
// by their placement in the input file. 
//
// There are two special cases: Function declarations, and DeclStmts. In turn:
//
//  - Function declarations might either be a DAndReplace describing the entire 
//    declaration, i.e. replacing "int *foo(void)" 
//    with "int *foo(void) : itype(_Ptr<int>)". Or, it might describe just 
//    replacing only the return type, i.e. "_Ptr<int> foo(void)". This is 
//    discriminated against with the 'fullDecl' field of the DAndReplace type
//    and the comparison function first checks if the operands are 
//    FunctionDecls and if the 'fullDecl' field is set. 
//  - A DeclStmt of mupltiple Decls, i.e. 'int *a = 0, *b = 0'. In this case,
//    we want the DAndReplace to refer only to the specific sub-region that
//    would be replaced, i.e. '*a = 0' and '*b = 0'. To do that, we traverse
//    the Decls contained in a DeclStmt and figure out what the appropriate 
//    source locations are to describe the positions of the independent 
//    declarations. 
struct DComp
{
  SourceManager &SM;
  DComp(SourceManager &S) : SM(S) { }

  SourceRange getWholeSR(SourceRange orig, DAndReplace dr) const {
    SourceRange newSourceRange(orig);

    if (FunctionDecl *FD = dyn_cast<FunctionDecl>(dr.Declaration)) {
      newSourceRange.setEnd(getFunctionDeclarationEnd(FD, SM));
      if (dr.fullDecl == false)
        newSourceRange = FD->getReturnTypeSourceRange();
    } 

    return newSourceRange;
  }

  bool operator()(const DAndReplace lhs, const DAndReplace rhs) const {
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
};

typedef std::set<DAndReplace, DComp> RSet;

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

// Visit each Decl in toRewrite and apply the appropriate pointer type
// to that Decl. The state of the rewrite is contained within R, which
// is both input and output. R is initialized to point to the 'main'
// source file for this transformation. toRewrite contains the set of
// declarations to rewrite. S is passed for source-level information
// about the current compilation unit. skip indicates some rewrites that
// we should skip because we already applied them, for example, as part 
// of turning a single line declaration into a multi-line declaration.
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

static
bool 
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

void emit(Rewriter &R, ASTContext &C, std::set<FileID> &Files,
          std::set<std::string> &InOutFiles) {

  // Check if we are outputing to stdout or not, if we are, just output the
  // main file ID to stdout.
  if (Verbose)
    errs() << "Writing files out\n";

  SmallString<254> baseAbs(BaseDir);
  std::error_code ec = sys::fs::make_absolute(baseAbs);
  assert(!ec);
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
          SmallString<254>  feAbs(FE->getName());
          std::string feAbsS = "";
          if (std::error_code ec = sys::fs::make_absolute(feAbs)) {
            if (Verbose)
              errs() << "could not make path absolote\n";
          } else
            feAbsS = sys::path::remove_leading_dotslash(feAbs.str());

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

// Class for visiting declarations during re-writing to find locations to
// insert casts. Right now, it looks specifically for 'free'. 
class CastPlacementVisitor : public RecursiveASTVisitor<CastPlacementVisitor> {
public:
  explicit CastPlacementVisitor(ASTContext *C, ProgramInfo &I, Rewriter &R,
      RSet &DR, std::set<FileID> &Files, std::set<std::string> &V)
    : Context(C), R(R), Info(I), rewriteThese(DR), Files(Files), VisitedSet(V) {} 

  bool VisitCallExpr(CallExpr *);
  bool VisitFunctionDecl(FunctionDecl *);
private:
  std::set<unsigned int> getParamsForExtern(std::string);
  bool anyTop(std::set<ConstraintVariable*>);
  ASTContext            *Context;
  Rewriter              &R;
  ProgramInfo           &Info;
  RSet                  &rewriteThese;
  std::set<FileID>      &Files;
  std::set<std::string> &VisitedSet;
};

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

  // Make sure we haven't visited this function name before, and that we 
  // only visit it once. 
  if (VisitedSet.find(funcName) != VisitedSet.end()) 
    return true;
  else
    VisitedSet.insert(funcName);

  // Do we have a definition for this declaration?  
  FunctionDecl *Definition = getDefinition(FD);
  FunctionDecl *Declaration = getDeclaration(FD);

  if(Definition == nullptr)
    return true;

  assert (Declaration != nullptr);

  // Get constraint variables for the declaration and the definition.
  // Those constraints should be function constraints. 
  auto cDecl = dyn_cast<FVConstraint>(
      getHighest(Info.getVariable(Declaration, Context, false), Info));
  auto cDefn = dyn_cast<FVConstraint>(
      getHighest(Info.getVariable(Definition, Context, true), Info));
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
      if (Defn->isLt(*Decl, Info) && anyConstrained) {
        std::string scratch = "";
        raw_string_ostream declText(scratch);
        Definition->getParamDecl(i)->print(declText);
        std::string ctype = Defn->mkString(Info.getConstraints().getVariables(), false);
        std::string bi = declText.str() + " : itype("+ctype+") ";
        parmStrs.push_back(bi);
      } else {
        // Do what we used to do.
        if (anyConstrained) { 
          std::string v = Defn->mkString(Info.getConstraints().getVariables());
          if (PVConstraint *PVC = dyn_cast<PVConstraint>(Defn)) {
            if (PVC->getItypePresent()) {
              v = v + " : " + PVC->getItype();
            }
          }
          parmStrs.push_back(v);
        } else {
          std::string scratch = "";
          raw_string_ostream declText(scratch);
          Definition->getParamDecl(i)->print(declText);
          parmStrs.push_back(declText.str());
        }
      }
    }

    // Compare returns. 
    auto Decl = getHighest(cDecl->getReturnVars(), Info);
    auto Defn = getHighest(cDefn->getReturnVars(), Info);

    // Insert a bounds safe interface for the return. 
    std::string returnVar = "";
    std::string endStuff = "";
    bool anyConstrained = Defn->anyChanges(Info.getConstraints().getVariables());
    if (Defn->isLt(*Decl, Info) && anyConstrained) {
      std::string ctype = Defn->mkString(Info.getConstraints().getVariables());
      returnVar = Defn->getTy();
      endStuff = " : itype("+ctype+") ";
      didAny = true;
    } else {
      // If we used to implement a bounds-safe interface, continue to do that.  
      returnVar = Decl->mkString(Info.getConstraints().getVariables());

      if (PVConstraint *PVC = dyn_cast<PVConstraint>(Decl)) {
        if (PVC->getItypePresent()) {
          assert(PVC->getItype().size() > 0);
          endStuff = " : " + PVC->getItype();
          didAny = true;
        }
      }
    }

    s = returnVar + cDecl->getName() + "(";
    if (parmStrs.size() > 0) {
      std::ostringstream ss;

      std::copy(parmStrs.begin(), parmStrs.end() - 1,
           std::ostream_iterator<std::string>(ss, ", "));
      ss << parmStrs.back();

      s = s + ss.str() + ")";
    } else {
      s = s + "void)";
    }

    if (endStuff.size() > 0)
      s = s + endStuff;

    if (didAny) 
      // Do all of the declarations.
      for (const auto &RD : Definition->redecls())
        rewriteThese.insert(DAndReplace(RD, s, true));
  }

  return true;
}

bool CastPlacementVisitor::VisitCallExpr(CallExpr *E) {
  return true;
}

class RewriteConsumer : public ASTConsumer {
public:
  explicit RewriteConsumer(ProgramInfo &I, 
    std::set<std::string> &F, ASTContext *Context) : Info(I), InOutFiles(F) {}

  virtual void HandleTranslationUnit(ASTContext &Context) {
    Info.enterCompilationUnit(Context);
    
    Rewriter R(Context.getSourceManager(), Context.getLangOpts());
    std::set<FileID> Files;

    std::set<std::string> v;
    RSet                  rewriteThese(DComp(Context.getSourceManager()));
    // Unification is done, so visit and see if we need to place any casts
    // in the program. 
    CastPlacementVisitor CPV = CastPlacementVisitor(&Context, Info, R, rewriteThese, Files, v);
    for (const auto &D : Context.getTranslationUnitDecl()->decls())
      CPV.TraverseDecl(D);

    // Build a map of all of the PersistentSourceLoc's back to some kind of 
    // Stmt, Decl, or Type.
    VariableMap &VarMap = Info.getVarMap();
    std::set<PersistentSourceLoc> keys;

    for (const auto &I : VarMap)
      keys.insert(I.first);
    std::map<PersistentSourceLoc, MappingVisitor::StmtDeclOrType> PSLMap;
    VariableDecltoStmtMap VDLToStmtMap;

    RSet skip(DComp(Context.getSourceManager()));
    MappingVisitor V(keys, Context);
    TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
    for (const auto &D : TUD->decls())
      V.TraverseDecl(D);

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

        if (PV && PV->anyChanges(Info.getConstraints().getVariables())) {
          // Rewrite a declaration.
          std::string newTy = PV->mkString(Info.getConstraints().getVariables());
          rewriteThese.insert(DAndReplace(D, DS, newTy));;
        } else if (FV && FV->anyChanges(Info.getConstraints().getVariables())) {
          // Rewrite a function variables return value.
          std::set<ConstraintVariable*> V = FV->getReturnVars();
          if (V.size() > 0) {
            std::string newTy = 
              (*V.begin())->mkString(Info.getConstraints().getVariables());
            rewriteThese.insert(DAndReplace(D, DS, newTy));
          }
        }
      }
    }

    rewrite(R, rewriteThese, skip, Context.getSourceManager(), Context, Files);

    // Output files.
    emit(R, Context, Files, InOutFiles);

    Info.exitCompilationUnit();
    return;
  }

private:
  ProgramInfo &Info;
  std::set<std::string> &InOutFiles;
};

template <typename T, typename V>
class GenericAction : public ASTFrontendAction {
public:
  GenericAction(V &I) : Info(I) {}

  virtual std::unique_ptr<ASTConsumer>
  CreateASTConsumer(CompilerInstance &Compiler, StringRef InFile) {
    return std::unique_ptr<ASTConsumer>(new T(Info, &Compiler.getASTContext()));
  }

private:
  V &Info;
};

template <typename T, typename V, typename U>
class GenericAction2 : public ASTFrontendAction {
public:
  GenericAction2(V &I, U &P) : Info(I),Files(P) {}

  virtual std::unique_ptr<ASTConsumer>
    CreateASTConsumer(CompilerInstance &Compiler, StringRef InFile) {
    return std::unique_ptr<ASTConsumer>
      (new T(Info, Files, &Compiler.getASTContext()));
  }

private:
  V &Info;
  U &Files;
};

template <typename T>
std::unique_ptr<FrontendActionFactory>
newFrontendActionFactoryA(ProgramInfo &I) {
  class ArgFrontendActionFactory : public FrontendActionFactory {
  public:
    explicit ArgFrontendActionFactory(ProgramInfo &I) : Info(I) {}

    FrontendAction *create() override { return new T(Info); }

  private:
    ProgramInfo &Info;
  };

  return std::unique_ptr<FrontendActionFactory>(
      new ArgFrontendActionFactory(I));
}

template <typename T>
std::unique_ptr<FrontendActionFactory>
newFrontendActionFactoryB(ProgramInfo &I, std::set<std::string> &PS) {
  class ArgFrontendActionFactory : public FrontendActionFactory {
  public:
    explicit ArgFrontendActionFactory(ProgramInfo &I,
      std::set<std::string> &PS) : Info(I),Files(PS) {}

    FrontendAction *create() override { return new T(Info, Files); }

  private:
    ProgramInfo &Info;
    std::set<std::string> &Files;
  };

  return std::unique_ptr<FrontendActionFactory>(
    new ArgFrontendActionFactory(I, PS));
}

int main(int argc, const char **argv) {
  sys::PrintStackTraceOnErrorSignal(argv[0]);

  // Initialize targets for clang module support.
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmPrinters();
  InitializeAllAsmParsers();

  if (BaseDir.size() == 0) {
    SmallString<256>  cp;
    if (std::error_code ec = sys::fs::current_path(cp)) {
      errs() << "could not get current working dir\n";
      return 1;
    }

    BaseDir = cp.str();
  }

  CommonOptionsParser OptionsParser(argc, argv, ConvertCategory);

  tooling::CommandLineArguments args = OptionsParser.getSourcePathList();

  ClangTool Tool(OptionsParser.getCompilations(), args);
  std::set<std::string> inoutPaths;

  for (const auto &S : args) {
    SmallString<255> abs_path(S);
    if (std::error_code ec = sys::fs::make_absolute(abs_path))
      errs() << "could not make absolute\n";
    else
      inoutPaths.insert(abs_path.str());
  }

  if (OutputPostfix == "-" && inoutPaths.size() > 1) {
    errs() << "If rewriting more than one , can't output to stdout\n";
    return 1;
  }

  ProgramInfo Info;

  // 1. Gather constraints.
  std::unique_ptr<ToolAction> ConstraintTool = newFrontendActionFactoryA<
      GenericAction<ConstraintBuilderConsumer, ProgramInfo>>(Info);
  
  if (ConstraintTool)
    Tool.run(ConstraintTool.get());
  else
    llvm_unreachable("No action");

  if (!Info.link()) {
    errs() << "Linking failed!\n";
    return 1;
  }

  // 2. Solve constraints.
  if (Verbose)
    outs() << "Solving constraints\n";
  Constraints &CS = Info.getConstraints();
  std::pair<Constraints::ConstraintSet, bool> R = CS.solve();
  // TODO: In the future, R.second will be false when there's a conflict, 
  //       and the tool will need to do something about that. 
  assert(R.second == true);
  if (Verbose)
    outs() << "Constraints solved\n";
  if (DumpIntermediate)
    Info.dump();

  // 3. Re-write based on constraints.
  std::unique_ptr<ToolAction> RewriteTool =
      newFrontendActionFactoryB
      <GenericAction2<RewriteConsumer, ProgramInfo, std::set<std::string>>>(
          Info, inoutPaths);
  
  if (RewriteTool)
    Tool.run(RewriteTool.get());
  else
    llvm_unreachable("No action");

  if (DumpStats)
    Info.dump_stats(inoutPaths);

  return 0;
}
