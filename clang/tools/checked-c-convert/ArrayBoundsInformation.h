//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// This class contains bounds information of constraint variables.
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
    // invalid bounds.
    InvalidKind,
    // only constants.
    ConstantBound,
    // bounds with field members.
    LocalFieldBound,
    // bounds with function parameters.
    LocalParamBound,
    // bounds with local variables.
    LocalVarBound,
  };
  typedef std::pair<BoundsKind, std::string> BOUNDSINFOTYPE;

  ArrayBoundsInformation(ProgramInfo &I) : Info(I) {
    BoundsInfo.clear();
  }

  // helper methods for adding bounds information to various
  // declaration objects.
  bool addBoundsInformation(FieldDecl *arrFD, FieldDecl *lenFD);
  bool addBoundsInformation(FieldDecl *arrFD, Expr *expr);
  bool addBoundsInformation(FieldDecl *arrFD, BOUNDSINFOTYPE binfo);

  // for function parameters
  bool addBoundsInformation(ParmVarDecl *arrFD, ParmVarDecl *lenFD);
  bool addBoundsInformation(ParmVarDecl *arrFD, BOUNDSINFOTYPE binfo);

  // for local variables
  bool addBoundsInformation(VarDecl *arrFD, VarDecl *lenFD);
  bool addBoundsInformation(VarDecl *arrFD, Expr *expr);
  bool addBoundsInformation(VarDecl *arrFD, BOUNDSINFOTYPE binfo);

  // remove all the bounds information for the provided declaration.
  bool removeBoundsInformation(Decl *decl);

  // check if the provided declaration has bounds information.
  bool hasBoundsInformation(Decl *decl);

  // get bounds information for the provided declaration.
  BOUNDSINFOTYPE getBoundsInformation(Decl *decl);

  // get bounds info from expression. Here, srcField indicates
  // if the bounds is for structure field, in which case this method
  // tries to enforce certain restrictions on the type of bounds info.
  BOUNDSINFOTYPE getExprBoundsInfo(FieldDecl *srcField, Expr *expr);

  // combine the provided bounds info by using the provided
  // infix operator op
  BOUNDSINFOTYPE combineBoundsInfo(FieldDecl *srcField,
                                   BOUNDSINFOTYPE &bounds1,
                                   BOUNDSINFOTYPE &bounds2,
                                   std::string op);

private:
  // get the constraint key of the top level pointer of provided declaration.
  ConstraintKey getTopLevelConstraintVar(Decl *decl);

  // check if the provided bounds kind is valid for a field.
  bool isValidBoundKindForField(BoundsKind targetBoundKind);

  // map that contains the bounds informa
  std::map<ConstraintKey, std::set<BOUNDSINFOTYPE>> BoundsInfo;
  ProgramInfo &Info;
};

#endif
