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
#include "llvm/Option/OptTable.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetSelect.h"

#include <algorithm>
#include <map>
#include <sstream>

#include "Constraints.h"

#include "ConstraintBuilder.h"
#include "NewTyp.h"
#include "PersistentSourceLoc.h"
#include "ProgramInfo.h"

using namespace clang::driver;
using namespace clang::tooling;
using namespace clang;
using namespace llvm;

static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::extrahelp MoreHelp("");

static cl::OptionCategory ConvertCategory("checked-c-convert options");
static cl::opt<bool> DumpIntermediate("dump-intermediate",
                                      cl::desc("Dump intermediate information"),
                                      cl::init(false),
                                      cl::cat(ConvertCategory));

static cl::opt<bool> Verbose( "verbose",
                              cl::desc("Print verbose information"),
                              cl::init(false),
                              cl::cat(ConvertCategory));

static cl::opt<std::string>
    OutputPostfix("output-postfix",
                  cl::desc("Postfix to add to the names of rewritten files, if "
                           "not supplied writes to STDOUT"),
                  cl::init("-"), cl::cat(ConvertCategory));

static cl::opt<bool> RewriteHeaders(
    "rewrite-headers",
    cl::desc("Rewrite header files as well as the specified source files"),
    cl::init(false), cl::cat(ConvertCategory));

// Visit each Decl in toRewrite and apply the appropriate pointer type
// to that Decl. The state of the rewrite is contained within R, which
// is both input and output. R is initialized to point to the 'main'
// source file for this transformation. toRewrite contains the set of
// declarations to rewrite. S is passed for source-level information
// about the current compilation unit.
void rewrite(Rewriter &R, std::set<NewTyp *> &toRewrite, SourceManager &S,
             ASTContext &A, std::set<FileID> &Files) {
  std::set<NewTyp *> skip;

  for (const auto &N : toRewrite) {
    Decl *D = N->getDecl();
    DeclStmt *Where = N->getWhere();

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
          for (const auto &I : FD->params()) {
            if (I == PV) {
              parmIndex = c;
              break;
            }
            c++;
          }
          assert(parmIndex >= 0);

          for (FunctionDecl *toRewrite = FD; toRewrite != NULL;
               toRewrite = toRewrite->getPreviousDecl()) {
            // TODO these declarations could get us into deeper header files.
            ParmVarDecl *Rewrite = toRewrite->getParamDecl(parmIndex);
            assert(Rewrite != NULL);
            SourceRange TR =
                Rewrite->getTypeSourceInfo()->getTypeLoc().getSourceRange();

            // Get a FullSourceLoc for the start location and add it to the
            // list of file ID's we've touched.
            FullSourceLoc FSL(TR.getBegin(), S);
            Files.insert(FSL.getFileID());
            R.ReplaceText(TR, N->mkStr());
          }
        } else
          llvm_unreachable("no function or method");
      } else
        llvm_unreachable("no parent function or method for decl");

    } else if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      assert(Where != NULL);
      SourceRange TR = VD->getTypeSourceInfo()->getTypeLoc().getSourceRange();

      // Is it a variable type? This is the easy case, we can re-write it
      // locally, at the site of the declaration.

      // Get a FullSourceLoc for the start location and add it to the
      // list of file ID's we've touched.
      FullSourceLoc FSL(TR.getBegin(), S);
      Files.insert(FSL.getFileID());
      if (Where->isSingleDecl())
        R.ReplaceText(TR, N->mkStr());
      else if (!(Where->isSingleDecl()) && skip.find(N) == skip.end()) {
        // Hack time!
        // Sometimes, like in the case of a decl on a single line, we'll need to
        // do multiple NewTyps at once. In that case, in the inner loop, we'll
        // re-scan and find all of the NewTyps related to that line and do
        // everything at once. That means sometimes we'll get NewTyps that
        // we don't want to process twice. We'll skip them here.

        // Step 1: get the re-written types.
        std::set<NewTyp *> rewritesForThisDecl;
        auto I = toRewrite.find(N);
        while (I != toRewrite.end()) {
          NewTyp *tmp = *I;
          if (tmp->getWhere() == Where)
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
          NewTyp *N = NULL;
          VarDecl *VDL = dyn_cast<VarDecl>(DL);
          assert(VDL != NULL);

          for (const auto &NLT : rewritesForThisDecl)
            if (NLT->getDecl() == DL) {
              N = NLT;
              break;
            }

          if (N) {
            newMLDecl << N->mkStr();
            newMLDecl << " " << VDL->getNameAsString();
            if (Expr *E = VDL->getInit()) {
              newMLDecl << " = ";
              E->printPretty(newMLDecl, nullptr, A.getPrintingPolicy());
            }
            newMLDecl << ";\n";
          } else {
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
      }
    } else if (FunctionDecl *UD = dyn_cast<FunctionDecl>(D)) {
      // TODO: If the return type is a fully-specified function pointer, 
      //       then clang will give back an invalid source range for the 
      //       return type source range. For now, check that the source
      //       range is valid. 
      SourceRange SR = UD->getReturnTypeSourceRange();
      if(SR.isValid())
        R.ReplaceText(SR, N->mkStr());
    }
    else if (FieldDecl *FD = dyn_cast<FieldDecl>(D)) {
      SourceRange SR = FD->getTypeSourceInfo()->getTypeLoc().getSourceRange();
      if (SR.isValid())
        R.ReplaceText(SR, N->mkStr());
    }
  }
}

void emit(Rewriter &R, ASTContext &C, std::set<FileID> &Files) {
  // Check if we are outputing to stdout or not, if we are, just output the
  // main file ID to stdout.
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

          StringRef pfName = sys::path::filename(FE->getName());
          StringRef dirName = sys::path::parent_path(FE->getName());
          StringRef fileName = sys::path::remove_leading_dotslash(pfName);
          StringRef ext = sys::path::extension(fileName);
          StringRef stem = sys::path::stem(fileName);
          Twine nFileName = stem + "." + OutputPostfix + ext;
          Twine nFile = dirName + sys::path::get_separator() + nFileName;

          if (ext != ".h" || (ext == ".h" && RewriteHeaders)) {
            std::error_code EC;
            raw_fd_ostream out(nFile.str(), EC, sys::fs::F_None);

            if (!EC) {
              if (Verbose)
                outs() << "writing out " << nFile.str() << "\n";
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

class RewriteConsumer : public ASTConsumer {
public:
  explicit RewriteConsumer(ProgramInfo &I, ASTContext *Context) : Info(I) {}

  virtual void HandleTranslationUnit(ASTContext &Context) {
    Info.enterCompilationUnit(Context);

    std::set<NewTyp *> rewriteThese;
    Constraints &CS = Info.getConstraints();
    for (const auto &V : Info.getVarMap()) {
      Decl *J = V.first;
      DeclStmt *K = NULL;
      Info.getDeclStmtForDecl(J, K);

      NewTyp *NT = NewTyp::mkTypForConstrainedType(CS, J, K, Info.getVarMap());
      rewriteThese.insert(NT);
    }

    Rewriter R(Context.getSourceManager(), Context.getLangOpts());
    std::set<FileID> Files;
    rewrite(R, rewriteThese, Context.getSourceManager(), Context, Files);

    // Output files.
    emit(R, Context, Files);

    Info.exitCompilationUnit();
    return;
  }

private:
  ProgramInfo &Info;
};

template <typename T, typename V>
class GenericAction : public ASTFrontendAction {
public:
  GenericAction(ProgramInfo &I) : Info(I) {}

  virtual std::unique_ptr<ASTConsumer>
  CreateASTConsumer(CompilerInstance &Compiler, StringRef InFile) {
    return std::unique_ptr<ASTConsumer>(new T(Info, &Compiler.getASTContext()));
  }

private:
  V &Info;
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

int main(int argc, const char **argv) {
  sys::PrintStackTraceOnErrorSignal();

  // Initialize targets for clang module support.
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmPrinters();
  InitializeAllAsmParsers();

  CommonOptionsParser OptionsParser(argc, argv, ConvertCategory);

  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  if (OutputPostfix == "-" && RewriteHeaders == true) {
    errs() << "If rewriting headers, can't output to stdout\n";
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
    outs() << "solving constraints\n";
  Constraints &CS = Info.getConstraints();
  CS.solve();
  if (Verbose)
    outs() << "constraints solved\n";
  if (DumpIntermediate)
    CS.dump();

  // 3. Re-write based on constraints.
  std::unique_ptr<ToolAction> RewriteTool =
      newFrontendActionFactoryA<GenericAction<RewriteConsumer, ProgramInfo>>(
          Info);

  if (RewriteTool)
    Tool.run(RewriteTool.get());
  else
    llvm_unreachable("No action");

  return 0;
}
