#ifndef _STRUCTINIT_H
#define _STRUCTINIT_H

#include "clang/CConv/ConstraintResolver.h"
#include "clang/CConv/RewriteUtils.h"
#include "clang/CConv/Utils.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/CConv/ArrayBoundsInferenceConsumer.h"
#include "clang/CConv/CCGlobalOptions.h"
#include "clang/CConv/MappingVisitor.h"
#include "clang/CConv/CheckedRegions.h"
#include "clang/CConv/StructInit.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Tooling/Refactoring/SourceCode.h"
#include <sstream>

using namespace clang;
using namespace llvm;

class StructVariableInitializer 
  : public RecursiveASTVisitor<StructVariableInitializer>
{
  public:
    explicit StructVariableInitializer(ASTContext *_C, ProgramInfo &_I, RSet &R)
           : Context(_C), I(_I), RewriteThese(R)
             { RecordsWithCPointers.clear(); }
    bool VisitDeclStmt(DeclStmt *S);


  private:


    bool VariableNeedsInitializer(VarDecl *VD, DeclStmt *S);

    ASTContext* Context;
    ProgramInfo& I;
    RSet& RewriteThese;
    std::set<RecordDecl*> RecordsWithCPointers;


};

#endif
