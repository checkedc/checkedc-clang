//=--ArrayBoundsInferenceConsumer.cpp ----------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation of all the methods of the class ArrayBoundsInferenceConsumer.
//
//===----------------------------------------------------------------------===//

#include "ArrayBoundsInferenceConsumer.h"
#include "ArrayBoundsInformation.h"
#include "Constraints.h"
#include "Utils.h"

#include "clang/AST/RecursiveASTVisitor.h"

#include <sstream>
#include <algorithm>

static std::set<std::string> LengthVarNamesPrefixes = {"len", "count",
                                                               "size", "num",
                                                               "siz"};
static std::set<std::string> LengthVarNamesSubstring = {"length"};
#define PREFIXPERCMATCH 50.0
#define COMMONSUBSEQUENCEPERCMATCH 80.0

// Name based heuristics.
static bool hasNameMatch(std::string PtrName, std::string FieldName) {
  // If the name field starts with ptrName?
  if (FieldName.rfind(PtrName, 0) == 0)
    return true;
}

std::string commonPrefixUtil(std::string Str1, std::string Str2) {
  auto MRes = std::mismatch(Str1.begin(), Str1.end(), Str2.begin());
  return Str1.substr(0, MRes.first - Str1.begin());
}

static bool prefixNameMatch(std::string PtrName, std::string FieldName) {
    std::string Prefix = commonPrefixUtil(PtrName, FieldName);

    if (Prefix.length() > 0) {
      return ((Prefix.length() * 100.0) / (PtrName.length() * 1.0)) >
             PREFIXPERCMATCH;
    }

    return false;
}

static bool nameSubStringMatch(std::string PtrName, std::string FieldName) {
  // Convert the names to lower case.
  std::transform(PtrName.begin(), PtrName.end(), PtrName.begin(),
                 [](unsigned char c){ return std::tolower(c); });
  std::transform(FieldName.begin(), FieldName.end(), FieldName.begin(),
                 [](unsigned char c){ return std::tolower(c); });
  unsigned SubSeqLen = longestCommonSubsequence(PtrName.c_str(), FieldName.c_str(),
                               PtrName.length(), FieldName.length());
  if (SubSeqLen > 0) {
    // Check if we get 80% match on the common subsequence matching on the
    // variable name of length and the name of array.
    return ((SubSeqLen * 100.0) / (PtrName.length() * 1.0)) >=
           COMMONSUBSEQUENCEPERCMATCH;
  }
  return false;

}

static bool fieldNameMatch(std::string FieldName) {
  // Convert the field name to lower case.
  std::transform(FieldName.begin(), FieldName.end(), FieldName.begin(),
                 [](unsigned char c){ return std::tolower(c); });
  for (auto &PName : LengthVarNamesPrefixes) {
    if (FieldName.rfind(PName, 0) == 0)
      return true;
  }

  for (auto &TmpName : LengthVarNamesSubstring) {
    if (FieldName.find(TmpName) != std::string::npos)
      return true;
  }
  return false;
}

static bool hasLengthKeyword(std::string VarName) {
  // Convert the field name to lower case.
  std::transform(VarName.begin(), VarName.end(), VarName.begin(),
                 [](unsigned char c){ return std::tolower(c); });

  std::set<std::string> LengthKeywords(LengthVarNamesPrefixes);
  LengthKeywords.insert(LengthVarNamesSubstring.begin(),
                           LengthVarNamesSubstring.end());
  for (auto &PName : LengthKeywords) {
    if (VarName.find(PName) != std::string::npos)
      return true;
  }
  return false;
}

// Check if the provided constraint variable is an array and it needs bounds.
static bool needArrayBounds(ConstraintVariable *CV,
                            Constraints::EnvironmentMap &E) {
  if (CV->hasArr(E)) {
    PVConstraint *PV = dyn_cast<PVConstraint>(CV);
    if (PV && PV->getArrPresent())
      return false;
    return true;
  }
  return false;
}

static bool needNTArrayBounds(ConstraintVariable *CV,
                              Constraints::EnvironmentMap &E) {
  if (CV->hasNtArr(E)) {
    PVConstraint *PV = dyn_cast<PVConstraint>(CV);
    if (PV && PV->getArrPresent())
      return false;
    return true;
  }
  return false;
}

static bool needArrayBounds(Decl *D, ProgramInfo &Info, ASTContext *C) {
  std::set<ConstraintVariable*> ConsVar = Info.getVariable(D, C);
  for (auto CurrCVar : ConsVar) {
    if (needArrayBounds(CurrCVar, Info.getConstraints().getVariables()))
      return true;
    return false;
  }
  return false;
}

// Map that contains association of allocator functions and indexes of
// parameters that correspond to the size of the object being assigned.
static std::map<std::string, std::set<int>> AllocatorSizeAssoc = {
                                            {"malloc", {0}},
                                            {"calloc", {0, 1}}};


// Get the name of the function called by this call expression.
static std::string getCalledFunctionName(const Expr *E) {
  const CallExpr *CE = dyn_cast<CallExpr>(E);
  assert(CE && "The provided expression should be a call expression.");
  const FunctionDecl *CalleeDecl = dyn_cast<FunctionDecl>(CE->getCalleeDecl());
  if (CalleeDecl && CalleeDecl->getDeclName().isIdentifier())
    return CalleeDecl->getName();
  return "";
}

// Check if the provided expression is a call to one of the known
// memory allocators.
static bool isAllocatorCall(Expr *E) {
  if (CallExpr *CE = dyn_cast<CallExpr>(removeAuxillaryCasts(E)))
    if (CE->getCalleeDecl() != nullptr) {
      // Is this a call to a named function?
      std::string FName = getCalledFunctionName(CE);
      // check if the called function is a known allocator?
      return AllocatorSizeAssoc.find(FName) !=
             AllocatorSizeAssoc.end();
    }
  return false;
}

static ArrayBoundsInformation::BOUNDSINFOTYPE
getAllocatedSizeExpr(Expr *E, ASTContext *C, ProgramInfo &Info,
                     FieldDecl *IsField = nullptr) {
  assert(isAllocatorCall(E) && "The provided expression should be a call to "
                                "to a known allocator function.");
  auto &ArrBInfo = Info.getArrayBoundsInformation();
  CallExpr *CE = dyn_cast<CallExpr>(removeAuxillaryCasts(E));
  assert(CE != nullptr && "Auxillary expression cannot be nullptr");
  std::string FName = getCalledFunctionName(CE);
  std::string SzExprStr = "";
  ArrayBoundsInformation::BOUNDSINFOTYPE PrevBInfo;
  bool IsFirstExpr = true;
  for (auto PIdx : AllocatorSizeAssoc[FName]) {
    Expr *e = CE->getArg(PIdx);
    auto CurrBInfo = ArrBInfo.getExprBoundsInfo(IsField, e);
    if (!IsFirstExpr) {
      CurrBInfo =
          ArrBInfo.combineBoundsInfo(
          IsField, PrevBInfo, CurrBInfo, "*");
    }
    PrevBInfo = CurrBInfo;
    IsFirstExpr = false;
  }
  return PrevBInfo;

}

// Check if expression is a simple local variable i.e., ptr = .if yes, return
// the referenced local variable as the return value of the argument.
bool isExpressionSimpleLocalVar(Expr *ToCheck, VarDecl **TargetDecl) {
  if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(ToCheck))
    if (DeclaratorDecl *FD = dyn_cast<DeclaratorDecl>(DRE->getDecl()))
      if (!dyn_cast<FieldDecl>(FD) && !dyn_cast<ParmVarDecl>(FD))
        if (VarDecl *VD = dyn_cast<VarDecl>(FD))
          if (!VD->hasGlobalStorage()) {
            *TargetDecl = VD;
            return true;
          }
  return false;
}

bool isExpressionStructField(Expr *ToCheck, FieldDecl **TargetDecl) {
  if (MemberExpr *DRE = dyn_cast<MemberExpr>(ToCheck)) {
    if (FieldDecl *FD = dyn_cast<FieldDecl>(DRE->getMemberDecl())) {
      *TargetDecl = FD;
      return true;
    }
  }
  return false;
}

// This visitor handles the bounds of function local array variables.

void GlobalABVisitor::SetParamHeuristicInfo(LocalVarABVisitor *LAB) {
  this->ParamInfo = LAB;
}

bool GlobalABVisitor::IsPotentialLengthVar(ParmVarDecl* PVD) {
  if (PVD->getType().getTypePtr()->isIntegerType()) {
    return ParamInfo == nullptr || !ParamInfo->isNonLengthParameter(PVD);
  }
  return false;
}

// This handles the length based heuristics for structure fields.
bool GlobalABVisitor::VisitRecordDecl(RecordDecl *RD) {
  // For each of the struct or union types.
  if (RD->isStruct() || RD->isUnion()) {
    // Get fields that are identified as arrays and also fields that could be
    // potential be the length fields
    std::set<FieldDecl *> PotLenFields;
    std::set<FieldDecl *> IdentifiedArrVars;
    const auto &AllFields = RD->fields();
    auto &ArrBInfo = Info.getArrayBoundsInformation();
    auto &E = Info.getConstraints().getVariables();
    for (auto *Fld : AllFields) {
      FieldDecl *FldDecl = dyn_cast<FieldDecl>(Fld);
      // This is an integer field and could be a length field
      if (FldDecl->getType().getTypePtr()->isIntegerType())
        PotLenFields.insert(FldDecl);

      std::set<ConstraintVariable*> ConsVars =
          Info.getVariable(FldDecl, Context);
      for (auto CurrCVar : ConsVars) {
        // Is this an array field?
        if (needArrayBounds(CurrCVar, E)) {
          IdentifiedArrVars.insert(FldDecl);
        }

      }
    }

    if (IdentifiedArrVars.size() > 0 && PotLenFields.size() > 0) {
      // First check for variable name match.
      for (auto PtrField : IdentifiedArrVars) {
        for (auto LenField : PotLenFields) {
          if (hasNameMatch(PtrField->getNameAsString(),
                           LenField->getNameAsString())) {
            // If we find a field which matches both the pointer name and
            // variable name heuristic lets use it.
            if (hasLengthKeyword(LenField->getNameAsString())) {
              ArrBInfo.removeBoundsInformation(PtrField);
              ArrBInfo.addBoundsInformation(PtrField, LenField);
              break;
            }
            ArrBInfo.addBoundsInformation(PtrField, LenField);
          }
        }
        // If the name-correspondence heuristics failed.
        // Then use the named based heuristics.
        if (!ArrBInfo.hasBoundsInformation(PtrField)) {
          for (auto LenField : PotLenFields) {
            if (fieldNameMatch(LenField->getNameAsString()))
              ArrBInfo.addBoundsInformation(PtrField, LenField);
          }
        }
      }
    }
  }
  return true;
}

bool GlobalABVisitor::VisitFunctionDecl(FunctionDecl *FD) {
  // If we have seen the body of this function? Then try to guess the length
  // of the parameters that are arrays.
  if (FD->isThisDeclarationADefinition() && FD->hasBody()) {
    auto &ArrBInfo = Info.getArrayBoundsInformation();
    const Type *Ty = FD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
    const FunctionProtoType *FT = Ty->getAs<FunctionProtoType>();
    if (FT != nullptr) {
      std::map<ParmVarDecl *, std::set<ParmVarDecl *>> ArrVarLenMap;
      std::map<unsigned , ParmVarDecl *> ParamArrays;
      std::map<unsigned, ParmVarDecl *> ParamNtArrays;
      std::map<unsigned , ParmVarDecl *> LengthParams;

      for (unsigned i = 0; i < FT->getNumParams(); i++) {
        ParmVarDecl *PVD = FD->getParamDecl(i);
        auto &E = Info.getConstraints().getVariables();
        std::set<ConstraintVariable *> DefCVars =
            Info.getVariable(PVD, Context, true);
        if (!DefCVars.empty()) {
          for (auto CurrCVar : DefCVars) {
            // Is this an array?
            if (needArrayBounds(CurrCVar, E))
              ParamArrays[i] = PVD;
            // Is this an NTArray?
            if (needNTArrayBounds(CurrCVar, E))
              ParamNtArrays[i] = PVD;
          }
        }
        // If this is a length field?
        if (IsPotentialLengthVar(PVD))
          LengthParams[i] = PVD;
      }
      if (!ParamArrays.empty() && !LengthParams.empty()) {
        // We have multiple parameters that are arrays and multiple params
        // that could be potentially length fields.
        for (auto &ArrParamPair : ParamArrays) {
          bool FoundLen = false;

          // If this is right next to the array param?
          // Then most likely this will be a length field.
          unsigned PIdx = ArrParamPair.first;
          if (LengthParams.find(PIdx +1) != LengthParams.end()) {
            ArrBInfo.addBoundsInformation(ArrParamPair.second,
                                          LengthParams[PIdx +1]);
            continue;
          }
          if (PIdx > 0 && LengthParams.find(PIdx -1) != LengthParams.end()) {
            if (prefixNameMatch(ArrParamPair.second->getNameAsString(),
                                LengthParams[PIdx -1]->getNameAsString()))
              ArrBInfo.addBoundsInformation(ArrParamPair.second,
                                            LengthParams[PIdx -1]);
            continue;

          }

          for (auto &LenParamPair : LengthParams) {
            // If the name of the length field matches.
            if (hasNameMatch(ArrParamPair.second->getNameAsString(),
                             LenParamPair.second->getNameAsString())) {
              FoundLen = true;
              ArrBInfo.removeBoundsInformation(ArrParamPair.second);
              ArrBInfo.addBoundsInformation(ArrParamPair.second,
                                            LenParamPair.second);
              break;
            }

            if (nameSubStringMatch(
                    ArrParamPair.second->getNameAsString(),
                                   LenParamPair.second->getNameAsString())) {
              FoundLen = true;
              ArrBInfo.removeBoundsInformation(ArrParamPair.second);
              ArrBInfo.addBoundsInformation(ArrParamPair.second,
                                            LenParamPair.second);
              continue;
            }
          }

          if (!FoundLen) {
            for (auto &currLenParamPair: LengthParams) {
              // Check if the length parameter name matches our heuristics.
              if (fieldNameMatch(currLenParamPair.second->getNameAsString())) {
                FoundLen = true;
                ArrBInfo.addBoundsInformation(ArrParamPair.second,
                                                   currLenParamPair.second);
              }
            }
          }

          if (!FoundLen) {
            llvm::errs() << "[-] Array variable length not found.\n";
            ArrParamPair.second->dump();
          }

        }
      }

      for (auto &CurrNtArr : ParamNtArrays) {
        unsigned PIdx = CurrNtArr.first;
        if (LengthParams.find(PIdx +1) != LengthParams.end()) {
          if (fieldNameMatch(LengthParams[PIdx +1]->getNameAsString())) {
            ArrBInfo.addBoundsInformation(CurrNtArr.second,
                                          LengthParams[PIdx +1]);
            continue;
          }
        }
      }
    }
  }
  return true;
}


bool LocalVarABVisitor::VisitBinAssign(BinaryOperator *O) {
  Expr *LHS = O->getLHS()->IgnoreImpCasts();
  Expr *RHS = O->getRHS()->IgnoreImpCasts();
  auto &arrBoundsInfo = Info.getArrayBoundsInformation();
  // is the RHS expression a call to allocator function?
  if (isAllocatorCall(RHS)) {
    // If this is an allocator function then sizeExpression contains the
    // argument used for size argument.

    // If LHS is just a variable or struct field i.e., ptr = .., get the AST
    // node of the target variable.
    VarDecl *TargetVar = nullptr;
    FieldDecl *StructField = nullptr;
    if (isExpressionSimpleLocalVar(LHS, &TargetVar) &&
        needArrayBounds(TargetVar, Info, Context)) {
      arrBoundsInfo.addBoundsInformation(
          TargetVar,
                                         getAllocatedSizeExpr(RHS,
                                                              Context, Info));
    } else if (isExpressionStructField(LHS, &StructField) &&
               needArrayBounds(StructField, Info, Context)) {
      if (!arrBoundsInfo.hasBoundsInformation(StructField))
        arrBoundsInfo.addBoundsInformation(
            StructField,
            getAllocatedSizeExpr(RHS, Context,Info, StructField));
    }
  }

  // Any parameter directly used as a condition in ternary expression
  // cannot be length.
  if (ConditionalOperator *CO = dyn_cast<ConditionalOperator>(RHS))
    addUsedParmVarDecl(CO->getCond());

  return true;
}

void LocalVarABVisitor::addUsedParmVarDecl(Expr *CE) {
  if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(CE->IgnoreParenImpCasts()))
    if (ParmVarDecl *PVD = dyn_cast<ParmVarDecl>(DRE->getDecl()))
      NonLengthParameters.insert(PVD);
}

bool LocalVarABVisitor::VisitIfStmt(IfStmt *IFS) {
  if (BinaryOperator *BO = dyn_cast<BinaryOperator>(IFS->getCond())) {
    BinaryOperator::Opcode BOpcode = BO->getOpcode();
    if (BOpcode == BinaryOperator::Opcode::BO_EQ ||
        BOpcode == BinaryOperator::Opcode::BO_NE) {
      addUsedParmVarDecl(BO->getLHS());
      addUsedParmVarDecl(BO->getRHS());
    }
  }
}

bool LocalVarABVisitor::VisitDeclStmt(DeclStmt *S) {
  // Build rules based on initializers.
  auto &ArrBInfo = Info.getArrayBoundsInformation();
  for (const auto &D : S->decls())
    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      Expr *InitE = VD->getInit();
      if (needArrayBounds(VD, Info, Context) &&
          InitE && isAllocatorCall(InitE)) {
        ArrBInfo.addBoundsInformation(VD,
                                           getAllocatedSizeExpr(InitE,
                                                                Context,
                                                                Info));
      }
    }

  return true;
}

bool LocalVarABVisitor::VisitSwitchStmt(SwitchStmt *S) {
  VarDecl *CondVar = S->getConditionVariable();

  // If this is a parameter declaration? Then this parameter cannot be length.
  if (CondVar != nullptr)
    if (ParmVarDecl *PD = dyn_cast<ParmVarDecl>(CondVar))
      NonLengthParameters.insert(PD);
}

// Check if the provided parameter cannot be a length of an array.
bool LocalVarABVisitor::isNonLengthParameter(ParmVarDecl* PVD) {
  if (PVD->getType().getTypePtr()->isEnumeralType())
    return true;
  return NonLengthParameters.find(PVD) != NonLengthParameters.end();
}

void AddArrayHeuristics(ASTContext *C, ProgramInfo &I, FunctionDecl *FD) {
  if (FD->isThisDeclarationADefinition() && FD->hasBody()) {
    // Heuristic: If the function has just a single parameter
    // and we found that it is an array then it must be an Nt_array.
    const Type *Ty = FD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
    const FunctionProtoType *FT = Ty->getAs<FunctionProtoType>();
    if (FT != nullptr) {
      if (FT->getNumParams() == 1) {
        ParmVarDecl *PVD = FD->getParamDecl(0);
        auto &CS = I.getConstraints();
        std::set<ConstraintVariable *> defsCVar = I.getVariable(PVD, C, true);
        for (auto constraintVar: defsCVar)
          if (PVConstraint *PV = dyn_cast<PVConstraint>(constraintVar)) {
            auto &Cvars = PV->getCvars();
            if (Cvars.size() > 0) {
              // We should constraint only the outer most constraint variable.
              auto CVar = *(Cvars.begin());
              CS.getOrCreateVar(CVar)->setNtArrayIfArray();
            }
          }
      } else if (FD->getNameInfo().getAsString() == std::string("main") &&
                 FT->getNumParams() == 2) {
        // If the function is `main` then we know second argument is _Array_ptr.
        ParmVarDecl *Argv = FD->getParamDecl(1);
        assert(Argv != NULL);
        auto &CS = I.getConstraints();
        std::set<ConstraintVariable*> DefCVars = I.getVariable(Argv, C, true);
        for (auto ConsVar : DefCVars) {
          if (PVConstraint *PV = dyn_cast<PVConstraint>(ConsVar)) {
            auto &Cvars = PV->getCvars();
            llvm::errs() << Cvars.size() << "\n";
            if (Cvars.size() == 2) {
              std::vector<ConstraintKey> NCvars(Cvars.begin(), Cvars.end());
              auto OuterCVar = CS.getOrCreateVar(NCvars[0]);
              auto InnerCVar = CS.getOrCreateVar(NCvars[1]);
              OuterCVar->setShouldBeArr();
              InnerCVar->setShouldBeNtArr();
            }
          }
        }
      }
    }
  }

}

void HandleArrayVariablesBoundsDetection(ASTContext *C, ProgramInfo &I) {
  // Run array bounds
  GlobalABVisitor GlobABV(C, I);
  TranslationUnitDecl *TUD = C->getTranslationUnitDecl();
  bool GlobalTraversed;
  // First visit all the structure members.
  for (const auto &D : TUD->decls()) {
    GlobalTraversed = false;
    if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
      if (FD->hasBody() && FD->isThisDeclarationADefinition()) {
        // Try to guess the bounds information for function locals.
        Stmt *Body = FD->getBody();
        LocalVarABVisitor LFV = LocalVarABVisitor(C, I);
        LFV.TraverseStmt(Body);
        // Set information collected after analyzing the function body.
        GlobABV.SetParamHeuristicInfo(&LFV);
        GlobABV.TraverseDecl(D);
        GlobalTraversed = true;
      }
    }
    // If this is not already traversed?
    if (!GlobalTraversed)
      GlobABV.TraverseDecl(D);
    GlobABV.SetParamHeuristicInfo(nullptr);
  }
}
