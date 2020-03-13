#ifndef __GATHERTOOL_H_
#define __GATHERTOOL_H_

#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/ASTContext.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "ProgramInfo.h"

using namespace clang;

typedef std::map<std::string, std::vector<bool>> ParameterMap;

class ArgGatherer : public ASTConsumer {
public:
    explicit ArgGatherer(ProgramInfo &I, ASTContext *Context, std::string &OPostfix)
            : Info(I), OutputPostfix(OPostfix) {}
    virtual void HandleTranslationUnit(ASTContext &Context);
    ParameterMap getMF();

private:
    ProgramInfo &Info;
    std::string &OutputPostfix;
    ParameterMap MF;
};

#endif // __GATHERTOOL_H_
