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
  bool addBoundsInformation(FieldDecl *ArrFd, FieldDecl *LenFD);
  bool addBoundsInformation(FieldDecl *ArrFd, Expr *E);
  bool addBoundsInformation(FieldDecl *ArrFd, BOUNDSINFOTYPE Binfo);

  // For function parameters.
  bool addBoundsInformation(ParmVarDecl *ArrFd, ParmVarDecl *LenFd);
  bool addBoundsInformation(ParmVarDecl *ArrFd, BOUNDSINFOTYPE Binfo);

  // For local variables.
  bool addBoundsInformation(VarDecl *ArrFd, VarDecl *LenFd);
  bool addBoundsInformation(VarDecl *ArrFd, Expr *E);
  bool addBoundsInformation(VarDecl *ArrFd, BOUNDSINFOTYPE Binfo);

  // Remove all the bounds information for the provided declaration.
  bool removeBoundsInformation(Decl *D);

  // Check if the provided declaration has bounds information.
  bool hasBoundsInformation(Decl *D);

  // Get bounds information for the provided declaration.
  BOUNDSINFOTYPE getBoundsInformation(Decl *D);

  // Get bounds info from expression. Here, srcField indicates
  // if the bounds is for structure field, in which case this method
  // tries to enforce certain restrictions on the type of bounds info.
  BOUNDSINFOTYPE getExprBoundsInfo(FieldDecl *Field, Expr *E);

  // Combine the provided bounds info by using the provided
  // infix operator op.
  BOUNDSINFOTYPE combineBoundsInfo(FieldDecl *Field,
                                   BOUNDSINFOTYPE &B1,
                                   BOUNDSINFOTYPE &B2,
                                   std::string OpStr);

private:

  // Check if the provided bounds kind is valid for a field.
  bool isValidBoundKindForField(BoundsKind BoundsKind);

  // Map that contains the bounds information of pointers w.r.t
  // their source loc.
  std::map<PersistentSourceLoc, std::set<BOUNDSINFOTYPE>> BoundsInfo;
  ProgramInfo &Info;
};

#endif
