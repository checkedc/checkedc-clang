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

#include "clang/3C/StructInit.h"
#include "clang/3C/MappingVisitor.h"
#include "clang/3C/RewriteUtils.h"
#include "clang/Tooling/Transformer/SourceCode.h"
#include <sstream>

using namespace clang;

bool StructVariableInitializer::hasCheckedMembers(DeclaratorDecl *DD) {
  // If this this isn't a structure or union, it can't have checked members.
  if (!isStructOrUnionType(DD))
    return false;

  // If this is a structure or union, then we can get the declaration for that
  // record and examine each member.
  RecordDecl *RD = DD->getType()->getAsRecordDecl();
  if (RecordDecl *Definition = RD->getDefinition()) {
    // See if we already know that this structure has a checked pointer.
    if (RecordsWithCPointers.find(Definition) != RecordsWithCPointers.end())
      return true;

    for (auto *const D : Definition->fields()) {
      if (D->getType()->isPointerType() || D->getType()->isArrayType()) {
        CVarOption CV = I.getVariable(D, Context);
        if (CV.hasValue()) {
          PVConstraint *PV = dyn_cast<PVConstraint>(&CV.getValue());
          if (PV && PV->isSolutionChecked(I.getConstraints().getVariables())) {
            // Ok this contains a pointer that is checked. Store it.
            RecordsWithCPointers.insert(Definition);
            return true;
          }
        }
      } else if (hasCheckedMembers(D)) {
        // A field of a structure can be another structure. If the inner
        // structure would need an initializer, we have to put it on the outer.
        RecordsWithCPointers.insert(Definition);
        return true;
      }
    }
  }
  return false;
}

// Insert the declaration and correct replacement text for the declaration into
// the set of required rewritings.
bool StructVariableInitializer::VisitVarDecl(VarDecl *VD) {
  // Check if we need to add an initializer.
  // TODO: Centralize initialization logic for all types:
  // https://github.com/correctcomputation/checkedc-clang/issues/645#issuecomment-876474200

  // The first two conditions are the same as in Sema::ActOnUninitializedDecl.
  if (VD->hasLocalStorage() && !isa<ParmVarDecl>(VD) && !VD->hasInit() &&
      hasCheckedMembers(VD)) {
    // Create replacement declaration text with an initializer.
    std::string ToReplace = mkStringForDeclWithUnchangedType(VD, I) + " = {}";
    RewriteThese.insert(
        std::make_pair(VD, new MultiDeclMemberReplacement(VD, ToReplace)));
  }
  return true;
}
