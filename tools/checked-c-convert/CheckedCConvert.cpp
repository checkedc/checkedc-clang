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

const Type *getNextTy(const Type *Ty) {
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

typedef std::pair<Decl*, DeclStmt*> DeclNStmt;
typedef std::pair<DeclNStmt, std::string> DAndReplace;

// Visit each Decl in toRewrite and apply the appropriate pointer type
// to that Decl. The state of the rewrite is contained within R, which
// is both input and output. R is initialized to point to the 'main'
// source file for this transformation. toRewrite contains the set of
// declarations to rewrite. S is passed for source-level information
// about the current compilation unit.
void rewrite(Rewriter &R, std::set<DAndReplace> &toRewrite, SourceManager &S,
             ASTContext &A, std::set<FileID> &Files) {
  std::set<DAndReplace> skip;

  for (const auto &N : toRewrite) {
    DeclNStmt DN = N.first;
    Decl *D = DN.first;
    DeclStmt *Where = DN.second;
    assert(D != nullptr);

    if (Verbose) {
      errs() << "Replacing type of decl:\n";
      D->dump();
      errs() << "with " << N.second << "\n";
    }

    // Get a FullSourceLoc for the start location and add it to the
    // list of file ID's we've touched.
    SourceRange tTR = D->getSourceRange();
    FullSourceLoc tFSL(tTR.getBegin(), S);
    Files.insert(tFSL.getFileID());

    if (ParmVarDecl *PV = dyn_cast<ParmVarDecl>(D)) {
      assert(Where == NULL);
      // Is it a parameter type?

      // First, find all the declarations of the containing function.
      if (DeclContext *DF = PV->getParentFunctionOrMethod()) {
        if (FunctionDecl *FD = dyn_cast<FunctionDecl>(DF)) {
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
              std::string sRewrite = N.second;

              if (canRewrite(R, TR))
                R.ReplaceText(TR, sRewrite);
            }
          }
        } else
          llvm_unreachable("no function or method");
      } else
        llvm_unreachable("no parent function or method for decl");

    } else if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      if (Where != NULL) {
        if (Verbose) {
          errs() << "VarDecl at:\n";
          Where->dump();
        }
        SourceRange TR = VD->getSourceRange();
        std::string sRewrite = N.second;

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
              R.ReplaceText(possible, N.second);
              std::string newStr = " " + VD->getName().str();
              R.InsertTextAfter(VD->getLocation(), newStr);
            } else {
              if (Verbose) {
                errs() << "Still don't know how to re-write VarDecl\n";
                VD->dump();
                errs() << "at\n";
                Where->dump();
                errs() << "with " << N.second << "\n";
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
          std::set<DAndReplace> rewritesForThisDecl;
          auto I = toRewrite.find(N);
          while (I != toRewrite.end()) {
            DAndReplace tmp = *I;
            if (tmp.first.second == Where)
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
              if (NLT.first.first == DL) {
                N = NLT;
                found = true;
                break;
              }

            if (found) {
              newMLDecl << N.second;
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
            errs() << "with " << N.second << "\n";
          }
        }
      } else {
        if (Verbose) {
          errs() << "Don't know where to rewrite a VarDecl! ";
          VD->dump();
          errs() << "\n";
        }
      }
    } else if (FunctionDecl *UD = dyn_cast<FunctionDecl>(D)) {
      // TODO: If the return type is a fully-specified function pointer, 
      //       then clang will give back an invalid source range for the 
      //       return type source range. For now, check that the source
      //       range is valid. 
      //       Additionally, a source range can be (mis) identified as 
      //       spanning multiple files. We don't know how to re-write that,
      //       so don't.
    
      SourceRange SR = UD->getReturnTypeSourceRange();
      if (canRewrite(R, SR))
        R.ReplaceText(SR, N.second);
    } else if (FieldDecl *FD = dyn_cast<FieldDecl>(D)) {
      SourceRange SR = FD->getSourceRange();
      std::string sRewrite = N.second;

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
  explicit CastPlacementVisitor(ASTContext *C, ProgramInfo &I, 
      Rewriter &R, std::set<FileID> &Files)
    : Context(C), Info(I), R(R), Files(Files) {} 

  bool VisitCallExpr(CallExpr *);
private:
  std::set<unsigned int> getParamsForExtern(std::string);
  bool anyTop(std::set<ConstraintVariable*>);
  ASTContext *Context;
  ProgramInfo &Info;
  Rewriter &R;
  std::set<FileID> &Files;
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

bool CastPlacementVisitor::VisitCallExpr(CallExpr *E) {

  // Find the target of this call. 
  if (Decl *D = E->getCalleeDecl()) {
    if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
      // Find the parameter placement for this call instruction. 
      std::set<unsigned int> P = getParamsForExtern(FD->getName());

      for (unsigned int i : P) {
        // Get the constraints for the ith parameter to the call. 
        Expr *EP = E->getArg(i)->IgnoreImpCasts();
        std::set<ConstraintVariable*> EPC = Info.getVariable(EP, Context);
        
        // Get the type of the ith parameter to the call. 
        QualType EPT = EP->getType(); 
        QualType PTF = FD->getParamDecl(i)->getType();

        // If they aren't equal, and the constraints in EPC are non-top, 
        // insert a cast. 
        if (EPT != PTF && !anyTop(EPC)) {
          // Insert a cast. 
          SourceLocation CL = EP->getExprLoc();
          R.InsertTextBefore(CL, "("+PTF.getAsString()+")");
        }
      }
    }
  }

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

    // Unification is done, so visit and see if we need to place any casts
    // in the program. 
    CastPlacementVisitor CPV = CastPlacementVisitor(&Context, Info, R, Files);
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

    MappingVisitor V(keys, Context);
    TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
    for (const auto &D : TUD->decls())
      V.TraverseDecl(D);

    std::tie(PSLMap, VDLToStmtMap) = V.getResults();

    std::set<DAndReplace> rewriteThese;
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
      Type *T = nullptr;

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
          rewriteThese.insert(DAndReplace(DeclNStmt(D, DS), newTy));
        } else if (FV && FV->anyChanges(Info.getConstraints().getVariables())) {
          // Rewrite a function variables return value.
          std::set<ConstraintVariable*> V = FV->getReturnVars();
          if (V.size() > 0) {
            std::string newTy = 
              (*V.begin())->mkString(Info.getConstraints().getVariables());
            rewriteThese.insert(DAndReplace(DeclNStmt(D, DS), newTy));
          }
        }
      }
    }

    rewrite(R, rewriteThese, Context.getSourceManager(), Context, Files);

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
