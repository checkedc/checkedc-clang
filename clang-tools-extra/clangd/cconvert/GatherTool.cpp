#include "clang/AST/RecursiveASTVisitor.h"

#include "clang/AST/Type.h"
#include "GatherTool.h"

using namespace llvm;
using namespace clang;

//ProgramInfo::getVariable(clang::Decl *D, clang::ASTContext *C, FunctionDecl *FD, int parameterIndex) {

class ParameterGatherer : public clang::RecursiveASTVisitor<ParameterGatherer>
{
    public:
        explicit ParameterGatherer(ASTContext *_C, ProgramInfo &_I, ParameterMap &_MF)
            : Context(_C), Info(_I), MF(_MF) {}

        bool VisitFunctionDecl(FunctionDecl *FD) {
            auto fn = FD->getNameAsString();
            if(FD->doesThisDeclarationHaveABody()) {
                std::vector<IsChecked> checked;
                int pi = 0;
                for(auto &param : FD->parameters()){
                    bool foundWild = false;
                    std::set<ConstraintVariable*> cvs = Info.getVariable(param, Context, FD, pi);
                    for(auto cv : cvs) {
                        foundWild |= cv->hasWild(Info.getConstraints().getVariables());
                    }
                    checked.push_back(foundWild ? WILD : CHECKED );
                    pi++;
                }
                MF[fn] = checked;
            }

            return false;
        }


    private:
        ASTContext *Context;
        ProgramInfo &Info;
        ParameterMap &MF;
};



void ArgGatherer::HandleTranslationUnit(ASTContext &Context) {
    Info.enterCompilationUnit(Context);
    ParameterGatherer PG(&Context, Info, MF);
    for(auto &D : Context.getTranslationUnitDecl()->decls()) {
        PG.TraverseDecl(D);
    }

    Info.merge_MF(MF);

    Info.exitCompilationUnit();
}

ParameterMap ArgGatherer::getMF() {
    return MF;
}
