//=--ArrayBoundsInformation.h-------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This class contains bounds information of constraint variables.
//
//===----------------------------------------------------------------------===//

#ifndef _ARRAYBOUNDSINFORMATION_H
#define _ARRAYBOUNDSINFORMATION_H

#include <clang/AST/Decl.h>
#include "Constraints.h"

class ProgramInfo;
using namespace clang;

// This class holds information about the bounds of various array variables.
class ArrayBoundsInformation {
public:
  enum BoundsKind {
    // Invalid bounds.
    InvalidKind,
    // Only constants.
    ConstantBound,
    // Bounds with field members.
    LocalFieldBound,
    // Bounds with function parameters.
    LocalParamBound,
    // Bounds with local variables.
    LocalVarBound,
  };
  typedef std::pair<BoundsKind, std::string> BOUNDSINFOTYPE;

  ArrayBoundsInformation(ProgramInfo &I) : Info(I) {
    BoundsInfo.clear();
  }

  // Helper methods for adding bounds information to various
  // declaration objects.
  bool addBoundsInformation(FieldDecl *arrFD, FieldDecl *lenFD);
  bool addBoundsInformation(FieldDecl *arrFD, Expr *expr);
  bool addBoundsInformation(FieldDecl *arrFD, BOUNDSINFOTYPE binfo);

  // For function parameters.
  bool addBoundsInformation(ParmVarDecl *arrFD, ParmVarDecl *lenFD);
  bool addBoundsInformation(ParmVarDecl *arrFD, BOUNDSINFOTYPE binfo);

  // For local variables.
  bool addBoundsInformation(VarDecl *arrFD, VarDecl *lenFD);
  bool addBoundsInformation(VarDecl *arrFD, Expr *expr);
  bool addBoundsInformation(VarDecl *arrFD, BOUNDSINFOTYPE binfo);

  // Remove all the bounds information for the provided declaration.
  bool removeBoundsInformation(Decl *decl);

  // Check if the provided declaration has bounds information.
  bool hasBoundsInformation(Decl *decl);

  // Get bounds information for the provided declaration.
  BOUNDSINFOTYPE getBoundsInformation(Decl *decl);

  // Get bounds info from expression. Here, srcField indicates
  // if the bounds is for structure field, in which case this method
  // tries to enforce certain restrictions on the type of bounds info.
  BOUNDSINFOTYPE getExprBoundsInfo(FieldDecl *srcField, Expr *expr);

  // Combine the provided bounds info by using the provided
  // infix operator op.
  BOUNDSINFOTYPE combineBoundsInfo(FieldDecl *srcField,
                                   BOUNDSINFOTYPE &bounds1,
                                   BOUNDSINFOTYPE &bounds2,
                                   std::string op);

private:
  // Get the constraint key of the top level pointer of provided declaration.
  ConstraintKey getTopLevelConstraintVar(Decl *decl);

  // Check if the provided bounds kind is valid for a field.
  bool isValidBoundKindForField(BoundsKind targetBoundKind);

  // Map that contains the bounds information.
  std::map<ConstraintKey, std::set<BOUNDSINFOTYPE>> BoundsInfo;
  ProgramInfo &Info;
};

#endif
