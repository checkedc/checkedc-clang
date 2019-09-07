//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Implementation of all the methods of the class ArrayBoundsInferenceConsumer.
//===----------------------------------------------------------------------===//

#include "clang/AST/RecursiveASTVisitor.h"

#include "Constraints.h"
#include "ArrayBoundsInferenceConsumer.h"
#include "Utils.h"

static std::set<std::string> possibleLengthVarNames = {"len", "count", "size", "num"};
#define PREFIXLENRATIO 1

// Name based heuristics
static bool hasNameMatch(std::string ptrName, std::string lenFieldName) {
  // if the name field starts with ptrName?
  if (lenFieldName.rfind(ptrName, 0) == 0)
    return true;
}

std::string commonPrefixUtil(std::string str1, std::string str2) {
  std::string result;
  int n1 = str1.length(), n2 = str2.length();

  // Compare str1 and str2
  for (int i=0, j=0; i<=n1-1 && j<=n2-1; i++,j++) {
    if (str1[i] != str2[j])
      break;
    result.push_back(str1[i]);
  }
  return (result);
}

static bool prefixNameMatch(std::string ptrName, std::string lenFieldName) {
    std::string commonPrefix = commonPrefixUtil(ptrName, lenFieldName);
    if (commonPrefix.length() > 0)
      return (ptrName.length() / commonPrefix.length()) <= PREFIXLENRATIO;

    return false;
}

static bool fieldNameMatch(std::string lenFieldName) {
  // convert the field name to lower case
  std::transform(lenFieldName.begin(), lenFieldName.end(), lenFieldName.begin(),
                 [](unsigned char c){ return std::tolower(c); });
  for (auto &potentialName : possibleLengthVarNames) {
    if (lenFieldName.rfind(potentialName, 0) == 0)
      return true;
  }
  return false;
}

static bool hasLengthKeyword(std::string varName) {
  // convert the field name to lower case
  std::transform(varName.begin(), varName.end(), varName.begin(),
                 [](unsigned char c){ return std::tolower(c); });
  for (auto &potentialName : possibleLengthVarNames) {
    if (varName.find(potentialName) != std::string::npos)
      return true;
  }
  return false;
}

// check if the provided constraint variable is an array and it needs bounds.
static bool needArrayBounds(ConstraintVariable *CV, Constraints::EnvironmentMap &envMap) {
  if (CV->hasArr(envMap)) {
    PVConstraint *PV = dyn_cast<PVConstraint>(CV);
    if (PV && PV->getArrPresent())
      return false;
    return true;
  }
  return false;
}

// This visitor handles the bounds of function local array variables.

// This handles the length based heuristics for structure fields.
bool HeuristicBasedABVisitor::VisitRecordDecl(RecordDecl *RD) {
  // for each of the struct or union types.
  if (RD->isStruct() || RD->isUnion()) {
    // Get fields that are identified as arrays and also fields that could be
    // potential be the length fields
    std::set<FieldDecl*> potentialLengthFields;
    std::set<FieldDecl*> identifiedArrayVars;
    const auto &allFields = RD->fields();
    auto &envMap = Info.getConstraints().getVariables();
    for (auto *fld: allFields) {
      FieldDecl *fldDecl = dyn_cast<FieldDecl>(fld);
      // this is an integer field and could be a length field
      if (fldDecl->getType().getTypePtr()->isIntegerType())
        potentialLengthFields.insert(fldDecl);

      std::set<ConstraintVariable*> consVar = Info.getVariable(fldDecl, Context);
      for (auto currCVar: consVar) {
        // is this an array field?
        if (needArrayBounds(currCVar, envMap)) {
          identifiedArrayVars.insert(fldDecl);
        }

      }
    }

    if (identifiedArrayVars.size() > 0 && potentialLengthFields.size() > 0) {
      // first check for variable name match?
      for (auto ptrField : identifiedArrayVars) {
        for (auto lenField: potentialLengthFields) {
          if (hasNameMatch(ptrField->getNameAsString(), lenField->getNameAsString())) {
            // if we find a field which matches both the pointer name and
            // variable name heuristic lets use it.
            if (hasLengthKeyword(lenField->getNameAsString())) {
              Info.removeArrayBoundsVar(ptrField);
              Info.addArrayBoundsVar(ptrField, lenField);
              break;
            }
            Info.addArrayBoundsVar(ptrField, lenField);
          }
        }
        // if the name-correspondence heuristics failed.
        // Then use the named based heuristics.
        if (!Info.hasArrSizeInfo(ptrField)) {
          for (auto lenField: potentialLengthFields) {
            if (fieldNameMatch(lenField->getNameAsString()))
              Info.addArrayBoundsVar(ptrField, lenField);
          }
        }
      }
    }
  }
  return true;
}

bool HeuristicBasedABVisitor::VisitFunctionDecl(FunctionDecl *FD) {
  // if we have seen the body of this function? Then try to guess the length
  // of the parameters that are arrays.
  if (FD->isThisDeclarationADefinition() && FD->hasBody()) {
    const Type *Ty = FD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
    const FunctionProtoType *FT = Ty->getAs<FunctionProtoType>();
    if (FT != nullptr) {
      std::map<ParmVarDecl *, std::set<ParmVarDecl *>> arrayVarLenCorrespondence;
      std::map<unsigned , ParmVarDecl *> identifiedParamArrays;
      std::map<unsigned , ParmVarDecl *> potentialLengthParams;

      for (unsigned i = 0; i < FT->getNumParams(); i++) {
        ParmVarDecl *PVD = FD->getParamDecl(i);
        auto &envMap = Info.getConstraints().getVariables();
        std::set<ConstraintVariable *> defsCVar = Info.getVariable(PVD, Context, true);
        if (!defsCVar.empty()) {
          for (auto currCVar: defsCVar) {
            // is this an array?
            if (needArrayBounds(currCVar, envMap))
              identifiedParamArrays[i] = PVD;
          }
        }
        // if this is a length field?
        if (PVD->getType().getTypePtr()->isIntegerType())
          potentialLengthParams[i] = PVD;
      }
      if (!identifiedParamArrays.empty() && !potentialLengthParams.empty()) {
        // We have multiple parameters that are arrays and multiple params
        // that could be potentially length fields
        for (auto &currArrParamPair: identifiedParamArrays) {
          bool foundLen = false;

          // If this is right next to the array param?
          // Then most likely this will be a length field.
          unsigned paramIdx = currArrParamPair.first;
          if (potentialLengthParams.find(paramIdx+1) != potentialLengthParams.end()) {
            Info.addArrayBoundsVar(currArrParamPair.second, potentialLengthParams[paramIdx+1]);
            continue;
          }
          if (paramIdx > 0 && potentialLengthParams.find(paramIdx-1) != potentialLengthParams.end()) {
            if (prefixNameMatch(currArrParamPair.second->getNameAsString(),
                                potentialLengthParams[paramIdx-1]->getNameAsString()))
              Info.addArrayBoundsVar(currArrParamPair.second, potentialLengthParams[paramIdx-1]);
            continue;

          }

          for (auto &currLenParamPair: potentialLengthParams) {
            // if the name of the length field matches
            if (hasNameMatch(currArrParamPair.second->getNameAsString(),
                             currLenParamPair.second->getNameAsString())) {
              foundLen = true;
              Info.addArrayBoundsVar(currArrParamPair.second, currLenParamPair.second);
              break;
            }
            // check if the length parameter name matches our heuristics.
            if (fieldNameMatch(currLenParamPair.second->getNameAsString())) {
              foundLen = true;
              Info.addArrayBoundsVar(currArrParamPair.second, currLenParamPair.second);
              continue;
            }
          }

          if (!foundLen) {
            llvm::errs() << "[-] Array variable length not found.\n";
            currArrParamPair.second->dump();
          }

        }
      }
    }
  }
  return true;
}

// check if the provided expression is a call
// to known memory allocators.
// if yes, return true along with the argument used as size
// assigned to the second parameter i.e., sizeArgument
bool HeuristicBasedABVisitor::isAllocatorCall(Expr *currExpr, Expr **sizeArgument) {
  if (currExpr != nullptr) {
    currExpr = removeAuxillaryCasts(currExpr);
    // check if this is a call expression.
    if (CallExpr *CA = dyn_cast<CallExpr>(currExpr)) {
      if(CA->getCalleeDecl() != nullptr) {
        // Is this a call to a named function?
        FunctionDecl *calleeDecl = dyn_cast<FunctionDecl>(CA->getCalleeDecl());
        if (calleeDecl && calleeDecl->getDeclName().isIdentifier()) {
          StringRef funcName = calleeDecl->getName();
          // check if the called function is a known allocator?
          if (HeuristicBasedABVisitor::AllocatorFunctionNames.find(funcName) !=
              HeuristicBasedABVisitor::AllocatorFunctionNames.end()) {
            if (sizeArgument != nullptr) {
              *sizeArgument = CA->getArg(0);
            }
            return true;
          }
        }
      }
    }
  }
  return false;
}

// check if expression is a simple local variable
// i.e., ptr = .
// if yes, return the referenced local variable as the return
// value of the argument.
bool HeuristicBasedABVisitor::isExpressionSimpleLocalVar(Expr *toCheck, Decl **targetDecl) {
  if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(toCheck)) {
    if (DeclaratorDecl *FD = dyn_cast<DeclaratorDecl>(DRE->getDecl())) {
      if (Decl *V = dyn_cast<Decl>(FD)) {
        *targetDecl = V;
        return true;
      }
    }
  }
  return false;
}

Expr *HeuristicBasedABVisitor::removeImpCasts(Expr *toConvert) {
  if(ImplicitCastExpr *impCast =dyn_cast<ImplicitCastExpr>(toConvert)) {
    return impCast->getSubExpr();
  }
  return toConvert;
}

Expr *HeuristicBasedABVisitor::removeCHKCBindTempExpr(Expr *toVeri) {
  if(CHKCBindTemporaryExpr *toChkExpr = dyn_cast<CHKCBindTemporaryExpr>(toVeri)) {
    return toChkExpr->getSubExpr();
  }
  return toVeri;
}

Expr *HeuristicBasedABVisitor::removeAuxillaryCasts(Expr *srcExpr) {
  srcExpr = removeCHKCBindTempExpr(srcExpr);
  if (CStyleCastExpr *C = dyn_cast<CStyleCastExpr>(srcExpr)) {
    srcExpr = C->getSubExpr();
  }
  srcExpr = removeCHKCBindTempExpr(srcExpr);
  srcExpr = removeImpCasts(srcExpr);
  return srcExpr;
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
        auto &envMap = CS.getVariables();
        std::set<ConstraintVariable *> defsCVar = I.getVariable(PVD, C, true);
        for (auto constraintVar: defsCVar)
          if (PVConstraint *PV = dyn_cast<PVConstraint>(constraintVar)) {
            auto &cVars = PV->getCvars();
            if (cVars.size() > 0) {
              // we should constraint only the outer most constraint variable.
              auto cVar = *(cVars.begin());
              CS.getOrCreateVar(cVar)->setNtArrayIfArray();
            }
          }
      }
    }
  }
}

std::set<std::string> HeuristicBasedABVisitor::AllocatorFunctionNames = {"malloc", "calloc"};

void HandleArrayVariablesBoundsDetection(ASTContext *C, ProgramInfo &I) {
  // Run array bounds
  HeuristicBasedABVisitor HBABV(C, I);
  TranslationUnitDecl *TUD = C->getTranslationUnitDecl();
  for (const auto &D : TUD->decls()) {
    HBABV.TraverseDecl(D);
  }
}