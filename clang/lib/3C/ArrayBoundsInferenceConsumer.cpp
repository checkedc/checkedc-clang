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

#include "clang/3C/ArrayBoundsInferenceConsumer.h"
#include "clang/3C/3CGlobalOptions.h"
#include "clang/3C/ConstraintResolver.h"
#include "clang/Analysis/Analyses/Dominators.h"
#include "clang/Analysis/CFG.h"
#include <sstream>

static std::set<std::string> LengthVarNamesPrefixes = {"len", "count", "size",
                                                       "num", "siz"};
static std::set<std::string> LengthVarNamesSubstring = {"length"};
#define PREFIXPERCMATCH 50.0
#define COMMONSUBSEQUENCEPERCMATCH 80.0

// Name based heuristics.
static bool hasNameMatch(std::string PtrName, std::string FieldName) {
  // If the name field starts with ptrName?
  if (FieldName.rfind(PtrName, 0) == 0)
    return true;

  return false;
}

std::string commonPrefixUtil(std::string Str1, std::string Str2) {
  auto MRes = std::mismatch(Str1.begin(), Str1.end(), Str2.begin());
  return Str1.substr(0, MRes.first - Str1.begin());
}

static bool nameSubStringMatch(std::string PtrName, std::string FieldName) {
  // Convert the names to lower case.
  std::transform(PtrName.begin(), PtrName.end(), PtrName.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  std::transform(FieldName.begin(), FieldName.end(), FieldName.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  unsigned SubSeqLen = longestCommonSubsequence(
      PtrName.c_str(), FieldName.c_str(), PtrName.length(), FieldName.length());
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
                 [](unsigned char c) { return std::tolower(c); });
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
                 [](unsigned char c) { return std::tolower(c); });

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
static bool needArrayBounds(const ConstraintVariable *CV,
                            const EnvironmentMap &E) {
  if (CV->hasArr(E, 0)) {
    const PVConstraint *PV = dyn_cast<PVConstraint>(CV);
    return !PV || PV->isTopCvarUnsizedArr();
  }
  return false;
}

static bool needNTArrayBounds(const ConstraintVariable *CV,
                              const EnvironmentMap &E) {
  if (CV->hasNtArr(E, 0)) {
    const PVConstraint *PV = dyn_cast<PVConstraint>(CV);
    return !PV || PV->isTopCvarUnsizedArr();
  }
  return false;
}

static bool needArrayBounds(Expr *E, ProgramInfo &Info, ASTContext *C) {
  ConstraintResolver CR(Info, C);
  CVarSet ConsVar = CR.getExprConstraintVars(E);
  const auto &EnvMap = Info.getConstraints().getVariables();
  for (auto CurrCVar : ConsVar) {
    if (needArrayBounds(CurrCVar, EnvMap) ||
        needNTArrayBounds(CurrCVar, EnvMap))
      return true;
    return false;
  }
  return false;
}

static bool needArrayBounds(Decl *D, ProgramInfo &Info, ASTContext *C,
                            bool IsNtArr) {
  const auto &E = Info.getConstraints().getVariables();
  CVarOption CVar = Info.getVariable(D, C);
  if (CVar.hasValue()) {
    ConstraintVariable &CV = CVar.getValue();
    if ((!IsNtArr && needArrayBounds(&CV, E)) ||
        (IsNtArr && needNTArrayBounds(&CV, E)))
      return true;
    return false;
  }
  return false;
}

static bool needArrayBounds(Decl *D, ProgramInfo &Info, ASTContext *C) {
  return needArrayBounds(D, Info, C, false) ||
         needArrayBounds(D, Info, C, true);
}

// Map that contains association of allocator functions and indexes of
// parameters that correspond to the size of the object being assigned.
static std::map<std::string, std::set<unsigned>> AllocatorSizeAssoc = {
    {"malloc", {0}}, {"calloc", {0, 1}}};

// Get the name of the function called by this call expression.
static std::string getCalledFunctionName(const Expr *E) {
  const CallExpr *CE = dyn_cast<CallExpr>(E);
  assert(CE && "The provided expression should be a call expression.");
  const FunctionDecl *CalleeDecl = dyn_cast<FunctionDecl>(CE->getCalleeDecl());
  if (CalleeDecl && CalleeDecl->getDeclName().isIdentifier())
    return CalleeDecl->getName();
  return "";
}

bool tryGetBoundsKeyVar(Expr *E, BoundsKey &BK, ProgramInfo &Info,
                        ASTContext *Context) {
  ConstraintResolver CR(Info, Context);
  CVarSet CVs = CR.getExprConstraintVars(E);
  auto &ABInfo = Info.getABoundsInfo();
  return CR.resolveBoundsKey(CVs, BK) || ABInfo.tryGetVariable(E, *Context, BK);
}

bool tryGetBoundsKeyVar(Decl *D, BoundsKey &BK, ProgramInfo &Info,
                        ASTContext *Context) {
  ConstraintResolver CR(Info, Context);
  CVarOption CV = Info.getVariable(D, Context);
  auto &ABInfo = Info.getABoundsInfo();
  return CR.resolveBoundsKey(CV, BK) || ABInfo.tryGetVariable(D, BK);
}

// Check if the provided expression E is a call to one of the known
// memory allocators. Will only return true if the argument to the call
// is a simple expression, and then organizes the ArgVals for determining
// a possible bound
static bool isAllocatorCall(Expr *E, std::string &FName, ProgramInfo &I,
                            ASTContext *C, std::vector<Expr *> &ArgVals) {
  bool RetVal = false;
  if (CallExpr *CE = dyn_cast<CallExpr>(removeAuxillaryCasts(E)))
    if (CE->getCalleeDecl() != nullptr) {
      // Is this a call to a named function?
      FName = getCalledFunctionName(CE);
      // check if the called function is a known allocator?
      if (AllocatorSizeAssoc.find(FName) != AllocatorSizeAssoc.end()) {
        RetVal = true;
        BoundsKey Tmp;
        // First get all base expressions.
        std::vector<Expr *> BaseExprs;
        BaseExprs.clear();
        for (auto Pidx : AllocatorSizeAssoc[FName]) {
          Expr *PExpr = CE->getArg(Pidx)->IgnoreParenCasts();
          BinaryOperator *BO = dyn_cast<BinaryOperator>(PExpr);
          UnaryExprOrTypeTraitExpr *UExpr =
              dyn_cast<UnaryExprOrTypeTraitExpr>(PExpr);
          if (BO && BO->isMultiplicativeOp()) {
            BaseExprs.push_back(BO->getLHS());
            BaseExprs.push_back(BO->getRHS());
          } else if (UExpr && UExpr->getKind() == UETT_SizeOf) {
            BaseExprs.push_back(UExpr);
          } else if (tryGetBoundsKeyVar(PExpr, Tmp, I, C)) {
            BaseExprs.push_back(PExpr);
          } else {
            RetVal = false;
            break;
          }
        }

        // Check if each of the expression is either sizeof or a DeclRefExpr
        if (RetVal && !BaseExprs.empty()) {
          for (auto *TmpE : BaseExprs) {
            TmpE = TmpE->IgnoreParenCasts();
            UnaryExprOrTypeTraitExpr *UExpr =
                dyn_cast<UnaryExprOrTypeTraitExpr>(TmpE);
            if ((UExpr && UExpr->getKind() == UETT_SizeOf) ||
                tryGetBoundsKeyVar(TmpE, Tmp, I, C)) {
              ArgVals.push_back(TmpE);
            } else {
              RetVal = false;
              break;
            }
          }
        }
      }
    }
  return RetVal;
}

static void handleAllocatorCall(QualType LHSType, BoundsKey LK, Expr *E,
                                ProgramInfo &Info, ASTContext *Context) {
  auto &AVarBInfo = Info.getABoundsInfo();
  auto &ABStats = AVarBInfo.getBStats();
  ConstraintResolver CR(Info, Context);
  std::string FnName;
  std::vector<Expr *> ArgVals;
  // is the RHS expression a call to allocator function?
  if (isAllocatorCall(E, FnName, Info, Context, ArgVals)) {
    BoundsKey RK;
    bool FoundSingleKeyInAllocExpr = false;
    // We consider everything as byte_count unless we see a sizeof
    // expression in which case if the type matches we use count bounds.
    bool IsByteBound = true;
    for (auto *TmpE : ArgVals) {
      UnaryExprOrTypeTraitExpr *arg = dyn_cast<UnaryExprOrTypeTraitExpr>(TmpE);
      if (arg && arg->getKind() == UETT_SizeOf) {
        QualType STy = Context->getPointerType(arg->getTypeOfArgument());
        // This is a count bound.
        if (LHSType == STy) {
          IsByteBound = false;
        } else {
          FoundSingleKeyInAllocExpr = false;
          break;
        }
      } else if (tryGetBoundsKeyVar(TmpE, RK, Info, Context)) {
        // Is this variable?
        if (!FoundSingleKeyInAllocExpr) {
          FoundSingleKeyInAllocExpr = true;
        } else {
          // Multiple variables found.
          FoundSingleKeyInAllocExpr = false;
          break;
        }
      } else {
        // Unrecognized expression.
        FoundSingleKeyInAllocExpr = false;
        break;
      }
    }

    if (FoundSingleKeyInAllocExpr) {
      // If we found BoundsKey from the allocate expression?
      auto *PrgLVar = AVarBInfo.getProgramVar(LK);
      auto *PrgRVar = AVarBInfo.getProgramVar(RK);
      ABounds *LBounds = nullptr;
      if (IsByteBound) {
        LBounds = new ByteBound(RK);
      } else {
        LBounds = new CountBound(RK);
      }

      // Either both should be in same scope or the RHS should be constant.
      if (*(PrgLVar->getScope()) == *(PrgRVar->getScope()) ||
          PrgRVar->IsNumConstant()) {
        if (!AVarBInfo.mergeBounds(LK, Allocator, LBounds)) {
          delete (LBounds);
        } else {
          ABStats.AllocatorMatch.insert(LK);
        }
      } else if (*(PrgLVar->getScope()) != *(PrgRVar->getScope())) {
        // This means we are using a variable in allocator that is not
        // in the same scope of LHS.
        // We do a little indirection trick here:
        // We create a temporary key and create bounds for it.
        // and then we mimic the assignment between temporary key
        // and the LHS.
        // Example:
        // int count;
        // ...
        // p->arr = malloc(sizeof(int)*count);
        // --- which gets translated to:
        // tmp <- bounds(count)
        // p->arr <- tmp
        // TODO: This trick of introducing bounds for RHS expressions
        //  could be useful in other places (e.g., &{1,2,3} would have bounds 3)
        BoundsKey TmpKey = AVarBInfo.getRandomBKey();
        AVarBInfo.replaceBounds(TmpKey, Declared, LBounds);
        AVarBInfo.addAssignment(LK, TmpKey);
      } else {
        assert(LBounds != nullptr && "LBounds cannot be nullptr here.");
        delete (LBounds);
      }
    }
  }
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

void AllocBasedBoundsInference::HandleTranslationUnit(ASTContext &Context) {
  Info.enterCompilationUnit(Context);
  HandleArrayVariablesBoundsDetection(&Context, Info, false);
  Info.exitCompilationUnit();
}

// This visitor handles the bounds of function local array variables.

void GlobalABVisitor::SetParamHeuristicInfo(LocalVarABVisitor *LAB) {
  this->ParamInfo = LAB;
}

bool GlobalABVisitor::IsPotentialLengthVar(ParmVarDecl *PVD) {
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
    std::set<std::pair<std::string, BoundsKey>> PotLenFields;
    std::set<std::pair<std::string, BoundsKey>> IdentifiedArrVars;
    const auto &AllFields = RD->fields();
    auto &ABInfo = Info.getABoundsInfo();
    auto &ABStats = ABInfo.getBStats();
    for (auto *Fld : AllFields) {
      FieldDecl *FldDecl = dyn_cast<FieldDecl>(Fld);
      BoundsKey FldKey;
      std::string FldName = FldDecl->getNameAsString();
      // This is an integer field and could be a length field
      if (FldDecl->getType().getTypePtr()->isIntegerType() &&
          tryGetBoundsKeyVar(FldDecl, FldKey, Info, Context))
        PotLenFields.insert(std::make_pair(FldName, FldKey));
      // Is this an array field and has no declared bounds?
      if (needArrayBounds(FldDecl, Info, Context) &&
          tryGetBoundsKeyVar(FldDecl, FldKey, Info, Context) &&
          !ABInfo.getBounds(FldKey))
        IdentifiedArrVars.insert(std::make_pair(FldName, FldKey));
    }

    if (IdentifiedArrVars.size() > 0 && PotLenFields.size() > 0) {
      // First check for variable name match.
      for (auto &PtrField : IdentifiedArrVars) {
        for (auto &LenField : PotLenFields) {
          if (hasNameMatch(PtrField.first, LenField.first)) {
            ABounds *FldBounds = new CountBound(LenField.second);
            // If we find a field which matches both the pointer name and
            // variable name heuristic lets use it.
            if (hasLengthKeyword(LenField.first)) {
              ABStats.NamePrefixMatch.insert(PtrField.second);
              ABInfo.replaceBounds(PtrField.second, Heuristics, FldBounds);
              break;
            }
            ABStats.VariableNameMatch.insert(PtrField.second);
            ABInfo.replaceBounds(PtrField.second, Heuristics, FldBounds);
          }
        }
        // If the name-correspondence heuristics failed.
        // Then use the named based heuristics.
        /*if (!ArrBInfo.hasBoundsInformation(PtrField)) {
          for (auto LenField : PotLenFields) {
            if (fieldNameMatch(LenField->getNameAsString()))
              ArrBInfo.addBoundsInformation(PtrField, LenField);
          }
        }*/
      }
    }
  }
  return true;
}

bool GlobalABVisitor::VisitFunctionDecl(FunctionDecl *FD) {
  // If we have seen the body of this function? Then try to guess the length
  // of the parameters that are arrays.
  if (FD->isThisDeclarationADefinition() && FD->hasBody()) {
    auto &ABInfo = Info.getABoundsInfo();
    auto &ABStats = ABInfo.getBStats();
    const clang::Type *Ty = FD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
    const FunctionProtoType *FT = Ty->getAs<FunctionProtoType>();
    if (FT != nullptr) {
      std::map<ParmVarDecl *, std::set<ParmVarDecl *>> ArrVarLenMap;
      std::map<unsigned, std::pair<std::string, BoundsKey>> ParamArrays;
      std::map<unsigned, std::pair<std::string, BoundsKey>> ParamNtArrays;
      std::map<unsigned, std::pair<std::string, BoundsKey>> LengthParams;

      for (unsigned i = 0; i < FT->getNumParams(); i++) {
        ParmVarDecl *PVD = FD->getParamDecl(i);
        BoundsKey PK;

        // Does this parameter already has bounds?
        if (tryGetBoundsKeyVar(PVD, PK, Info, Context)) {
          auto PVal = std::make_pair(PVD->getNameAsString(), PK);

          // Here, we are using heuristics. So we only use heuristics when
          // there are no bounds already computed.
          if (!ABInfo.getBounds(PK)) {
            if (needArrayBounds(PVD, Info, Context, true)) {
              // Is this an NTArray?
              ParamNtArrays[i] = PVal;
            }
            if (needArrayBounds(PVD, Info, Context, false)) {
              // Is this an array?
              ParamArrays[i] = PVal;
            }
          }

          // If this is a length field?
          if (IsPotentialLengthVar(PVD))
            LengthParams[i] = PVal;
        }
      }
      if (!ParamArrays.empty() && !LengthParams.empty()) {
        // We have multiple parameters that are arrays and multiple params
        // that could be potentially length fields.
        for (auto &ArrParamPair : ParamArrays) {
          bool FoundLen = false;

          // If this is right next to the array param?
          // Then most likely this will be a length field.
          unsigned PIdx = ArrParamPair.first;
          BoundsKey PBKey = ArrParamPair.second.second;
          if (LengthParams.find(PIdx + 1) != LengthParams.end()) {
            ABounds *PBounds = new CountBound(LengthParams[PIdx + 1].second);
            ABInfo.replaceBounds(PBKey, Heuristics, PBounds);
            ABStats.NeighbourParamMatch.insert(PBKey);
            continue;
          }

          for (auto &LenParamPair : LengthParams) {
            // If the name of the length field matches.
            if (hasNameMatch(ArrParamPair.second.first,
                             LenParamPair.second.first)) {
              FoundLen = true;
              ABounds *PBounds = new CountBound(LenParamPair.second.second);
              ABInfo.replaceBounds(PBKey, Heuristics, PBounds);
              ABStats.NamePrefixMatch.insert(PBKey);
              break;
            }

            if (nameSubStringMatch(ArrParamPair.second.first,
                                   LenParamPair.second.first)) {
              FoundLen = true;
              ABounds *PBounds = new CountBound(LenParamPair.second.second);
              ABInfo.replaceBounds(PBKey, Heuristics, PBounds);
              ABStats.NamePrefixMatch.insert(PBKey);
              continue;
            }
          }

          if (!FoundLen) {
            for (auto &currLenParamPair : LengthParams) {
              // Check if the length parameter name matches our heuristics.
              if (fieldNameMatch(currLenParamPair.second.first)) {
                FoundLen = true;
                ABounds *PBounds =
                    new CountBound(currLenParamPair.second.second);
                ABInfo.replaceBounds(PBKey, Heuristics, PBounds);
                ABStats.VariableNameMatch.insert(PBKey);
              }
            }
          }

          if (!FoundLen) {
            llvm::errs() << "[-] Array variable length not found:"
                         << ArrParamPair.second.first << "\n";
          }
        }
      }

      for (auto &CurrNtArr : ParamNtArrays) {
        unsigned PIdx = CurrNtArr.first;
        BoundsKey PBKey = CurrNtArr.second.second;
        if (LengthParams.find(PIdx + 1) != LengthParams.end()) {
          if (fieldNameMatch(LengthParams[PIdx + 1].first)) {
            ABounds *PBounds = new CountBound(LengthParams[PIdx + 1].second);
            ABInfo.replaceBounds(PBKey, Heuristics, PBounds);
            ABStats.VariableNameMatch.insert(PBKey);
            continue;
          }
        }
      }
    }
  }
  return true;
}

void LocalVarABVisitor::handleAssignment(BoundsKey LK, QualType LHSType,
                                         Expr *RHS) {
  auto &ABoundsInfo = Info.getABoundsInfo();
  handleAllocatorCall(LHSType, LK, RHS, Info, Context);
  clang::StringLiteral *SL =
      dyn_cast_or_null<clang::StringLiteral>(RHS->IgnoreParenCasts());
  if (SL != nullptr) {
    ABounds *ByBounds =
        new ByteBound(ABoundsInfo.getConstKey(SL->getByteLength()));
    if (!ABoundsInfo.mergeBounds(LK, Allocator, ByBounds)) {
      delete (ByBounds);
    } else {
      ABoundsInfo.getBStats().AllocatorMatch.insert(LK);
    }
  }
}

bool LocalVarABVisitor::HandleBinAssign(BinaryOperator *O) {
  Expr *LHS = O->getLHS()->IgnoreParenCasts();
  Expr *RHS = O->getRHS()->IgnoreParenCasts();
  ConstraintResolver CR(Info, Context);
  std::string FnName;
  std::vector<Expr *> ArgVals;
  BoundsKey LK;
  // is the RHS expression a call to allocator function?
  if (needArrayBounds(LHS, Info, Context) &&
      tryGetBoundsKeyVar(LHS, LK, Info, Context)) {
    handleAssignment(LK, LHS->getType(), RHS);
  }

  // Any parameter directly used as a condition in ternary expression
  // cannot be length.
  if (ConditionalOperator *CO = dyn_cast<ConditionalOperator>(RHS))
    addUsedParmVarDecl(CO->getCond());

  return true;
}

void LocalVarABVisitor::addUsedParmVarDecl(Expr *CE) {
  if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(CE->IgnoreParenCasts()))
    if (ParmVarDecl *PVD = dyn_cast<ParmVarDecl>(DRE->getDecl()))
      NonLengthParameters.insert(PVD);
}

bool isValidBinOpForLen(BinaryOperator::Opcode COP) {
  bool Valid = true;
  switch (COP) {
  // Invalid BinOPs
  // ==, !=, &, &=, &&, |, |=, ^, ^=
  case BinaryOperator::Opcode::BO_EQ:
  case BinaryOperator::Opcode::BO_NE:
  case BinaryOperator::Opcode::BO_And:
  case BinaryOperator::Opcode::BO_AndAssign:
  case BinaryOperator::Opcode::BO_LAnd:
  case BinaryOperator::Opcode::BO_Or:
  case BinaryOperator::Opcode::BO_OrAssign:
  case BinaryOperator::Opcode::BO_LOr:
  case BinaryOperator::Opcode::BO_Xor:
  case BinaryOperator::Opcode::BO_XorAssign:
    Valid = false;
    break;
  // Rest all Ops are okay.
  default:
    break;
  }
  return Valid;
}

bool LocalVarABVisitor::VisitBinaryOperator(BinaryOperator *BO) {
  BinaryOperator::Opcode BOpcode = BO->getOpcode();
  // Is this not a valid bin op for a potential length parameter?
  if (!isValidBinOpForLen(BOpcode)) {
    addUsedParmVarDecl(BO->getLHS());
    addUsedParmVarDecl(BO->getRHS());
  }
  if (BOpcode == BinaryOperator::Opcode::BO_Assign) {
    HandleBinAssign(BO);
  }
  return true;
}

bool LocalVarABVisitor::VisitArraySubscriptExpr(ArraySubscriptExpr *E) {
  addUsedParmVarDecl(E->getIdx());
  return true;
}

bool LocalVarABVisitor::VisitDeclStmt(DeclStmt *S) {
  // Build rules based on initializers.
  auto &ABoundsInfo = Info.getABoundsInfo();
  for (const auto &D : S->decls())
    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      Expr *InitE = VD->getInit();
      BoundsKey DeclKey;
      if (InitE != nullptr && needArrayBounds(VD, Info, Context) &&
          tryGetBoundsKeyVar(VD, DeclKey, Info, Context)) {
        handleAssignment(DeclKey, VD->getType(), InitE);
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

  return true;
}

// Check if the provided parameter cannot be a length of an array.
bool LocalVarABVisitor::isNonLengthParameter(ParmVarDecl *PVD) {
  if (PVD->getType().getTypePtr()->isEnumeralType())
    return true;
  return NonLengthParameters.find(PVD) != NonLengthParameters.end();
}

void AddMainFuncHeuristic(ASTContext *C, ProgramInfo &I, FunctionDecl *FD) {
  if (FD->isThisDeclarationADefinition() && FD->hasBody()) {
    // Heuristic: If the function has just a single parameter
    // and we found that it is an array then it must be an Nt_array.
    const clang::Type *Ty = FD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
    const FunctionProtoType *FT = Ty->getAs<FunctionProtoType>();
    auto &ABInfo = I.getABoundsInfo();
    if (FT != nullptr) {
      if (FD->getNameInfo().getAsString() == std::string("main") &&
          FT->getNumParams() == 2) {
        // If the function is `main` then we know second argument is _Array_ptr.
        ParmVarDecl *Argv = FD->getParamDecl(1);
        assert(Argv != nullptr && "Argument cannot be nullptr");
        BoundsKey ArgvKey;
        BoundsKey ArgcKey;
        if (needArrayBounds(Argv, I, C) &&
            tryGetBoundsKeyVar(Argv, ArgvKey, I, C) &&
            tryGetBoundsKeyVar(FD->getParamDecl(0), ArgcKey, I, C)) {
          ABounds *ArgcBounds = new CountBound(ArgcKey);
          ABInfo.replaceBounds(ArgvKey, Declared, ArgcBounds);
        }
      }
    }
  }
}

// Given a variable I, this visitor collects all the variables that are used as
// RHS operand of < and I >=  expression.
// i.e., for all I < X expressions, it collects X.
class ComparisionVisitor : public RecursiveASTVisitor<ComparisionVisitor> {
public:
  explicit ComparisionVisitor(ProgramInfo &In, ASTContext *AC, BoundsKey I,
                              std::set<BoundsKey> &PossB)
      : I(In), C(AC), IndxBKey(I), PB(PossB), CurrStmt(nullptr) {
    CR = new ConstraintResolver(In, AC);
  }
  virtual ~ComparisionVisitor() {
    if (CR != nullptr) {
      delete (CR);
      CR = nullptr;
    }
  }

  // Here, we save the most recent statement we have visited.
  // This is a way to keep track of the statement to which currently
  // processing expression belongs.
  // This is okay, because RecursiveASTVisitor traverses the AST in
  // pre-order, so we always visit the parent i.e., the statement
  // before visiting the subexpressions under it.
  bool VisitStmt(Stmt *S) {
    CurrStmt = S;
    return true;
  }

  bool VisitBinaryOperator(BinaryOperator *BO) {
    // We care about < and >= operator.
    if (BO->getOpcode() == BO_LT || BO->getOpcode() == BO_GE) {
      Expr *LHS = BO->getLHS()->IgnoreParenCasts();
      Expr *RHS = BO->getRHS()->IgnoreParenCasts();
      auto LHSCVars = CR->getExprConstraintVars(LHS);
      auto RHSCVars = CR->getExprConstraintVars(RHS);

      if (!CR->containsValidCons(LHSCVars) &&
          !CR->containsValidCons(RHSCVars)) {
        BoundsKey LKey, RKey;
        auto &ABI = I.getABoundsInfo();
        if ((CR->resolveBoundsKey(LHSCVars, LKey) ||
             ABI.tryGetVariable(LHS, *C, LKey)) &&
            (CR->resolveBoundsKey(RHSCVars, RKey) ||
             ABI.tryGetVariable(RHS, *C, RKey))) {

          // If this the left hand side of a < comparison and
          // the LHS is the index used in array indexing operation?
          // Then add the RHS to the possible bounds key.
          bool IsRKeyBound = (LKey == IndxBKey);
          if (BO->getOpcode() == BO_GE) {
            // If we have: x >= y, then this has to be an IfStmt to
            // consider Y as upper bound.
            // Why? This is to distinguish between following cases:
            // In the following case, we should not
            // consider y as the bound.
            // for (i=n-1; i >= y; i--) {
            //      arr[i] = ..
            // }
            // Where as the following is a valid case. MAX_LEN is the bound.
            // if (i >= MAX_LEN) {
            //     return -1;
            //  }
            //  arr[i] = ..
            IsRKeyBound &= (CurrStmt != nullptr && isa<IfStmt>(CurrStmt));
          }

          if (IsRKeyBound)
            PB.insert(RKey);
        }
      }
    }
    return true;
  }

private:
  ProgramInfo &I;
  ASTContext *C;
  // Index variable used in dereference.
  BoundsKey IndxBKey;
  // Possible Bounds.
  std::set<BoundsKey> &PB;
  // Helper objects.
  ConstraintResolver *CR;
  // Current statement: The statement to which the processing
  // node belongs. This is to avoid walking the AST.
  Stmt *CurrStmt;
};

LengthVarInference::LengthVarInference(ProgramInfo &In, ASTContext *AC,
                                       FunctionDecl *F)
    : I(In), C(AC), FD(F), CurBB(nullptr) {

  Cfg = CFG::buildCFG(nullptr, FD->getBody(), AC, CFG::BuildOptions());
  for (auto *CBlock : *(Cfg.get())) {
    for (auto &CfgElem : *CBlock) {
      if (CfgElem.getKind() == clang::CFGElement::Statement) {
        const Stmt *TmpSt = CfgElem.castAs<CFGStmt>().getStmt();
        StMap[TmpSt] = CBlock;
      }
    }
  }

  CDG = new ControlDependencyCalculator(Cfg.get());

  CR = new ConstraintResolver(I, C);
}

LengthVarInference::~LengthVarInference() {
  if (CDG != nullptr) {
    delete (CDG);
    CDG = nullptr;
  }
  if (CR != nullptr) {
    delete (CR);
    CR = nullptr;
  }
}

void LengthVarInference::VisitStmt(Stmt *St) {
  for (auto *Child : St->children()) {
    if (Child) {
      if (StMap.find(St) != StMap.end()) {
        CurBB = StMap[St];
      }
      Visit(Child);
    }
  }
}

// Consider the following example:
//
// int foo(int *a, int *b, unsigned l) {
//  unsigned i = 0;
//  for (i=0; i<l; i++) {
//    a[i] = b[i];
//  }
// }
// From the above code, it is obvious that the length of a and b should be l.
//
// Consider the following a bit more complex example:
//
// struct f {
//  int *a;
//  unsigned l;
// };
// void clear(struct f *b, int idx) {
//  unsigned n = b->l;
//  if (idx >= n) {
//    return;
//  }
//  b->a[idx] = 0;
// }
// Here, we can see that the length of the f's struct member a is l.
//
// We can capture these facts by using control dependencies. Specifically, for
// each array indexing operation, i.e., arr[i], we find all the statements that
// the indexing statement is control dependent on.
// Then, for each of the control dependent nodes, we check if there is any
// relational comparison of the form i < X or i >= X, then we consider X
// (or any assignments of X to the variables of the same scope as arr) to be
// the size of arr.
void LengthVarInference::VisitArraySubscriptExpr(ArraySubscriptExpr *ASE) {
  assert(CurBB != nullptr && "Array dereference does not belong "
                             "to any basic block");
  // First, get the BoundsKey for the base.
  Expr *BE = ASE->getBase()->IgnoreParenCasts();

  // If this is a multi-level array dereference i.e., a[i][j],
  // then try-processing the base ASE i.e., a[i].
  if (ArraySubscriptExpr *SubASE = dyn_cast_or_null<ArraySubscriptExpr>(BE)) {
    VisitArraySubscriptExpr(SubASE);
    return;
  }
  auto BaseCVars = CR->getExprConstraintVars(BE);
  // Next get the index used.
  Expr *IdxExpr = ASE->getIdx()->IgnoreParenCasts();
  auto IdxCVars = CR->getExprConstraintVars(IdxExpr);

  // Get the bounds key of the base and index.
  if (CR->containsValidCons(BaseCVars) && !CR->containsValidCons(IdxCVars)) {
    BoundsKey BasePtr, IdxKey;
    auto &ABI = I.getABoundsInfo();
    if (CR->resolveBoundsKey(BaseCVars, BasePtr) &&
        (CR->resolveBoundsKey(IdxCVars, IdxKey) ||
         ABI.tryGetVariable(IdxExpr, *C, IdxKey))) {
      std::set<BoundsKey> PossibleLens;
      PossibleLens.clear();
      ComparisionVisitor CV(I, C, IdxKey, PossibleLens);
      auto &CDNodes = CDG->getControlDependencies(CurBB);
      if (!CDNodes.empty()) {
        // Next try to find all the nodes that the CurBB is
        // control dependent on.
        // For each of the control dependent node, check if we are comparing the
        // index variable with another variable.
        for (auto &CDGNode : CDNodes) {
          // Collect the possible length bounds keys.
          CV.TraverseStmt(CDGNode->getTerminatorStmt());
        }
        ABI.updatePotentialCountBounds(BasePtr, PossibleLens);
      }
    }
  }
}

void HandleArrayVariablesBoundsDetection(ASTContext *C, ProgramInfo &I,
                                         bool UseHeuristics) {
  // Run array bounds
  for (auto FuncName : AllocatorFunctions) {
    AllocatorSizeAssoc[FuncName] = {0};
  }
  GlobalABVisitor GlobABV(C, I);
  TranslationUnitDecl *TUD = C->getTranslationUnitDecl();
  LocalVarABVisitor LFV = LocalVarABVisitor(C, I);
  bool GlobalTraversed;
  // First visit all the structure members.
  for (const auto &D : TUD->decls()) {
    GlobalTraversed = false;
    if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
      if (FD->hasBody() && FD->isThisDeclarationADefinition()) {
        // Try to guess the bounds information for function locals.
        Stmt *Body = FD->getBody();
        LFV.TraverseStmt(Body);

        if (UseHeuristics) {
          // Set information collected after analyzing the function body.
          GlobABV.SetParamHeuristicInfo(&LFV);
          GlobABV.TraverseDecl(D);
        }
        AddMainFuncHeuristic(C, I, FD);
        GlobalTraversed = true;
      }
    }

    if (UseHeuristics) {
      // If this is not already traversed?
      if (!GlobalTraversed)
        GlobABV.TraverseDecl(D);
      GlobABV.SetParamHeuristicInfo(nullptr);
    }
  }
}
