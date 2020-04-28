//=--IterativeItypeHelper.h---------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This file contains the helper classes that are used for constraint solving
// based on iterative itype refinement.
//===----------------------------------------------------------------------===//

#ifndef _ITYPECONSTRAINTDETECTOR_H
#define _ITYPECONSTRAINTDETECTOR_H

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "ProgramInfo.h"

using namespace clang;

// This method resets the constraint graph by removing
// equality edges involving itype variables.
unsigned long resetWithitypeConstraints(Constraints &CS);

// Identify the functions which have the constraint variables of parameters
// or return changed from previous iteration.
bool identifyModifiedFunctions(Constraints &CS,
                               std::set<std::string> &modifiedFunctions);

// This method detects and updates the newly detected
// (in the previous iteration) itype parameters and return values for all the
// provided set of functions (modifiedFunctions). Note that, these are the
// detections made by the tool, i.e., not the ones provided by user
unsigned long detectAndUpdateITypeVars(ProgramInfo &Info,
                                       std::set<std::string>
                                           &modifiedFunctions);

// Set up a map of constraint variables so that we know if a
// function constraint variables are modified.
bool performConstraintSetup(ProgramInfo &Info);

#endif //_ITYPECONSTRAINTDETECTOR_H
