//=--StructInit.cpp-----------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This class contains implementation of the functions and
// classes of StructInit.h
//===----------------------------------------------------------------------===//

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/CConv/MappingVisitor.h"
#include "clang/CConv/StructInit.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Tooling/Refactoring/SourceCode.h"
#include <sstream>

using namespace clang;


bool StructVariableInitializer::VariableNeedsInitializer(VarDecl *VD,
                                                         DeclStmt *S) {
  RecordDecl *RD = VD->getType().getTypePtr()->getAsRecordDecl();
  if (RecordDecl *Definition = RD->getDefinition()) {
    // See if we already know that this structure has a checked pointer.
    if (RecordsWithCPointers.find(Definition) !=
        RecordsWithCPointers.end()) {
      return true;
    }
    for (const auto &D : Definition->fields()) {
      if (D->getType()->isPointerType() || D->getType()->isArrayType()) {
        CVarSet FieldConsVars =
            I.getVariable(D, Context);
        for (auto CV : FieldConsVars) {
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
bool StructVariableInitializer::VisitDeclStmt(DeclStmt *S) {

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

