//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Implementation of all the methods of the class ArrayBoundsInferenceConsumer.
//===----------------------------------------------------------------------===//

#include <sstream>
#include "clang/AST/RecursiveASTVisitor.h"

#include "Constraints.h"
#include "ArrayBoundsInferenceConsumer.h"
#include "Utils.h"
#include "ArrayBoundsInformation.h"

static std::set<std::string> possibleLengthVarNamesPrefixes = {"len", "count", "size", "num"};
static std::set<std::string> possibleLengthVarNamesSubstring = {"length"};
#define PREFIXLENRATIO 1

// Name based heuristics
static bool hasNameMatch(std::string ptrName, std::string lenFieldName) {
  // if the name field starts with ptrName?
  if (lenFieldName.rfind(ptrName, 0) == 0)
    return true;

  return false;
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
  for (auto &potentialName : possibleLengthVarNamesPrefixes) {
    if (lenFieldName.rfind(potentialName, 0) == 0)
      return true;
  }

  for (auto &potentialName : possibleLengthVarNamesSubstring) {
    if (lenFieldName.find(potentialName) != std::string::npos)
      return true;
  }
  return false;
}

static bool hasLengthKeyword(std::string varName) {
  // convert the field name to lower case
  std::transform(varName.begin(), varName.end(), varName.begin(),
                 [](unsigned char c){ return std::tolower(c); });

  std::set<std::string> allLengthKeywords(possibleLengthVarNamesPrefixes);
  allLengthKeywords.insert(possibleLengthVarNamesSubstring.begin(), possibleLengthVarNamesSubstring.end());
  for (auto &potentialName : allLengthKeywords) {
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

static bool needArrayBounds(Decl *decl, ProgramInfo &Info, ASTContext *Context) {
  std::set<ConstraintVariable*> consVar = Info.getVariable(decl, Context);
  for (auto currCVar: consVar) {
    if (needArrayBounds(currCVar, Info.getConstraints().getVariables()))
      return true;
    return false;
  }
  return false;
}

// map that contains association of allocator functions and indexes of
// parameters that correspond to the size of the object being assigned.
static std::map<std::string, std::set<int>> AllocatorSizeAssoc = {
                                            {"malloc", {0}},
                                            {"calloc", {0, 1}}};


// get the name of the function called by this call expression
std::string getCalledFunctionName(Expr *E) {
  CallExpr *CE = dyn_cast<CallExpr>(E);
  assert(CE && "The provided expression should be a call expression.");
  FunctionDecl *calleeDecl = dyn_cast<FunctionDecl>(CE->getCalleeDecl());
  if (calleeDecl && calleeDecl->getDeclName().isIdentifier())
    return calleeDecl->getName();
  return "";
}

// check if the provided expression is a call to
// one of the known memory allocators.
static bool isAllocatorCall(Expr *E) {
  if (CallExpr *CE = dyn_cast<CallExpr>(removeAuxillaryCasts(E)))
    if (CE->getCalleeDecl() != nullptr) {
      // Is this a call to a named function?
      std::string funcName = getCalledFunctionName(CE);
      // check if the called function is a known allocator?
      return AllocatorSizeAssoc.find(funcName) !=
             AllocatorSizeAssoc.end();
    }
  return false;
}

static ArrayBoundsInformation::BOUNDSINFOTYPE getAllocatedSizeExpr(Expr *E, ASTContext *C,
                                                                   ProgramInfo &Info, FieldDecl *isField = nullptr) {
  assert(isAllocatorCall(E) && "The provided expression should be a call to "
                                "to a known allocator function.");
  auto &arrBoundsInfo = Info.getArrayBoundsInformation();
  CallExpr *CE = dyn_cast<CallExpr>(removeAuxillaryCasts(E));
  std::string funcName = getCalledFunctionName(CE);
  std::string sizeExpr = "";
  ArrayBoundsInformation::BOUNDSINFOTYPE previousBoundsInfo;
  bool isFirstExpr = true;
  for (auto parmIdx : AllocatorSizeAssoc[funcName]) {
    Expr *e = CE->getArg(parmIdx);
    auto currBoundsInfo = arrBoundsInfo.getExprBoundsInfo(isField, e);
    if (!isFirstExpr) {
      currBoundsInfo = arrBoundsInfo.combineBoundsInfo(isField, previousBoundsInfo, currBoundsInfo, "*");
      isFirstExpr = false;
    }
    previousBoundsInfo = currBoundsInfo;
  }
  return previousBoundsInfo;

}

// check if expression is a simple local variable
// i.e., ptr = .
// if yes, return the referenced local variable as the return
// value of the argument.
bool isExpressionSimpleLocalVar(Expr *toCheck, VarDecl **targetDecl) {
  if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(toCheck))
    if (DeclaratorDecl *FD = dyn_cast<DeclaratorDecl>(DRE->getDecl()))
      if (!dyn_cast<FieldDecl>(FD) && !dyn_cast<ParmVarDecl>(FD))
        if (VarDecl *VD = dyn_cast<VarDecl>(FD))
          if (!VD->hasGlobalStorage()) {
            *targetDecl = VD;
            return true;
          }
  return false;
}

bool isExpressionStructField(Expr *toCheck, FieldDecl **targetDecl) {
  if (MemberExpr *DRE = dyn_cast<MemberExpr>(toCheck)) {
    if (FieldDecl *FD = dyn_cast<FieldDecl>(DRE->getMemberDecl())) {
      *targetDecl = FD;
      return true;
    }
  }
  return false;
}

// This visitor handles the bounds of function local array variables.

// This handles the length based heuristics for structure fields.
bool GlobalABVisitor::VisitRecordDecl(RecordDecl *RD) {
  // for each of the struct or union types.
  if (RD->isStruct() || RD->isUnion()) {
    // Get fields that are identified as arrays and also fields that could be
    // potential be the length fields
    std::set<FieldDecl*> potentialLengthFields;
    std::set<FieldDecl*> identifiedArrayVars;
    const auto &allFields = RD->fields();
    auto &arrBoundInfo = Info.getArrayBoundsInformation();
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
              arrBoundInfo.removeBoundsInformation(ptrField);
              arrBoundInfo.addBoundsInformation(ptrField, lenField);
              break;
            }
            arrBoundInfo.addBoundsInformation(ptrField, lenField);
          }
        }
        // if the name-correspondence heuristics failed.
        // Then use the named based heuristics.
        if (!arrBoundInfo.hasBoundsInformation(ptrField)) {
          for (auto lenField: potentialLengthFields) {
            if (fieldNameMatch(lenField->getNameAsString()))
              arrBoundInfo.addBoundsInformation(ptrField, lenField);
          }
        }
      }
    }
  }
  return true;
}

bool GlobalABVisitor::VisitFunctionDecl(FunctionDecl *FD) {
  // if we have seen the body of this function? Then try to guess the length
  // of the parameters that are arrays.
  if (FD->isThisDeclarationADefinition() && FD->hasBody()) {
    auto &arrBoundsInfo = Info.getArrayBoundsInformation();
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
            arrBoundsInfo.addBoundsInformation(currArrParamPair.second, potentialLengthParams[paramIdx+1]);
            continue;
          }
          if (paramIdx > 0 && potentialLengthParams.find(paramIdx-1) != potentialLengthParams.end()) {
            if (prefixNameMatch(currArrParamPair.second->getNameAsString(),
                                potentialLengthParams[paramIdx-1]->getNameAsString()))
              arrBoundsInfo.addBoundsInformation(currArrParamPair.second, potentialLengthParams[paramIdx-1]);
            continue;

          }

          for (auto &currLenParamPair: potentialLengthParams) {
            // if the name of the length field matches
            if (hasNameMatch(currArrParamPair.second->getNameAsString(),
                             currLenParamPair.second->getNameAsString())) {
              foundLen = true;
              arrBoundsInfo.addBoundsInformation(currArrParamPair.second, currLenParamPair.second);
              break;
            }
            // check if the length parameter name matches our heuristics.
            if (fieldNameMatch(currLenParamPair.second->getNameAsString())) {
              foundLen = true;
              arrBoundsInfo.addBoundsInformation(currArrParamPair.second, currLenParamPair.second);
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


bool LocalVarABVisitor::VisitBinAssign(BinaryOperator *O) {
  Expr *LHS = O->getLHS()->IgnoreImpCasts();
  Expr *RHS = O->getRHS()->IgnoreImpCasts();
  auto &arrBoundsInfo = Info.getArrayBoundsInformation();
  Expr *sizeExpression;
  auto &envMap = Info.getConstraints().getVariables();
  // is the RHS expression a call to allocator function?
  if (isAllocatorCall(RHS)) {
    // if this is an allocator function then sizeExpression contains the
    // argument used for size argument

    // if LHS is just a variable or struct field i.e., ptr = .., get the AST node of the
    // target variable
    VarDecl *targetVar = nullptr;
    FieldDecl *structField = nullptr;
    if (isExpressionSimpleLocalVar(LHS, &targetVar) && needArrayBounds(targetVar, Info, Context)) {
      arrBoundsInfo.addBoundsInformation(targetVar, getAllocatedSizeExpr(RHS, Context, Info));
    } else if (isExpressionStructField(LHS, &structField) && needArrayBounds(structField, Info, Context)) {
      if (!arrBoundsInfo.hasBoundsInformation(structField))
        arrBoundsInfo.addBoundsInformation(structField, getAllocatedSizeExpr(RHS, Context, Info, structField));
    }
  }
  return true;
}

bool LocalVarABVisitor::VisitDeclStmt(DeclStmt *S) {
  // Build rules based on initializers.
  auto &arrBoundsInfo = Info.getArrayBoundsInformation();
  for (const auto &D : S->decls())
    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      Expr *InitE = VD->getInit();
      if (needArrayBounds(VD, Info, Context) && InitE && isAllocatorCall(InitE)) {
        arrBoundsInfo.addBoundsInformation(VD, getAllocatedSizeExpr(InitE, Context, Info));
      }
    }

  return true;
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
      } else if(FD->getNameInfo().getAsString() == std::string("main") && FT->getNumParams() == 2) {
        // If the function is `main` then we know second arg is _Array_ptr
        ParmVarDecl *argv = FD->getParamDecl(1);
        assert(argv != NULL);
        auto &CS = I.getConstraints();
        auto &envMap = CS.getVariables();
        std::set<ConstraintVariable*> defsCVar = I.getVariable(argv, C, true);
        for(auto constraintVar : defsCVar) {
          if(PVConstraint *PV = dyn_cast<PVConstraint>(constraintVar)) {
            auto &cVars = PV->getCvars();
            llvm::errs() << cVars.size() << "\n";
            if(cVars.size() == 2) {
              std::vector<ConstraintKey> vars(cVars.begin(), cVars.end());
              auto outerCVar = CS.getOrCreateVar(vars[0]);
              auto innerCVar = CS.getOrCreateVar(vars[1]);
              outerCVar->setShouldBeArr();
              innerCVar->setShouldBeNtArr();
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
  // first visit all the structure members.
  for (const auto &D : TUD->decls()) {
    GlobABV.TraverseDecl(D);
  }
  // next try to guess the bounds information for function locals.
  for (const auto &D : TUD->decls()) {
    if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
      if (FD->hasBody() && FD->isThisDeclarationADefinition()) {
        Stmt *Body = FD->getBody();
        LocalVarABVisitor LFV = LocalVarABVisitor(C, I);
        LFV.TraverseStmt(Body);
      }
    }
  }
}
