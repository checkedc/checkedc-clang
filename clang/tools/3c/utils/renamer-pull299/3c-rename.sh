#!/bin/bash
#
# Script for the automated portion of the 3C rename
# (https://github.com/correctcomputation/checkedc-clang/pull/299).
# The initial and final manual changes have to be cherry-picked manually.

set -e
. "$(dirname "$0")/3c-rename-functions.sh"

# FILE RENAMES, including references

# Special cases
commit_git_mv clang/docs/checkedc/{CheckedCConvert,3C}.md
commit_git_mv clang/tools/{3c,3c}
commit_git_mv clang/tools/3c/utils/{cc_conv,port_tools}
commit_git_mv clang/include/clang/CConv/{CC,3C}GlobalOptions.h
commit_git_mv clang/test/{3C,3C}

repl_basename_pfx CConvert 3C '^clang'
repl_basename_pfx CConv 3C '^clang'
# There is nothing with "cconv" that we want to rename.

# Occurrences of filenames in file content.

repl_word 3c 3c  # Catches CMakeLists, actions file, lit tests, etc.
repl_word clang/tools/3c/utils/{cc_conv,port_tools} '^\.github/workflows/main\.yml$'
repl_word {CC,3C}GlobalOptions
repl_word 3C 3C  # Only in actions file
repl '\<CConv(ert)?' 3C '/CMakeLists\.txt$'
# Without this, the next line would be insufficient to handle '#include "clang/CConv/CConv.h"'.
# This seems like a decent solution compared to running the next line twice or using a custom
# sed command to replace multiple ["/]CConv on a line containing #include .
repl_cpp '#include "clang/CConv/' '#include "clang/3C/'
repl_cpp '(#include.*["/])CConv(ert)?' '\13C'
# Match CConvert first before CConv messes it up.
repl '//=--CConvert([^-]*-)' '//=--3C\1------'
repl '//=--CConv([^-]*-)' '//=--3C\1---'

# Include guards
repl_cpp '\<LLVM_CLANG_TOOLS_EXTRA_CLANGD_CCONVERT' LLVM_CLANG_TOOLS_EXTRA_CLANGD_3C
repl_cpp_word _CCGLOBALOPTIONS_H _3CGLOBALOPTIONS_H
repl_cpp_word '_CCONV([A-Z]*)_H' '_3C\1_H'

# OTHERS

# The action file will be removed from checkedc-clang soon, but we need to do
# this so that the tree is clean of "cconv" now /and/ to generate the change
# to cherry-pick to the new actions repository.
repl_word 'cconv(build|scripts)' '3c-\1' '^\.github/workflows/main\.yml$'

# C++ identifiers

# Initial of various forms
repl_cpp_word {CC,_3C}Interface
repl_cpp_word 'cconv(CollectAndBuildInitialConstraints|CloseDocument)' '_3C\1'
repl_cpp_word 'CConv(Interface|Inter|Main|Sec|DiagInfo|LSPCallBack)' '_3C\1'
repl_cpp_word {CCONV,_3C}SOURCE
repl_cpp '\<(Command|ExecuteCommandParams)::CCONV_' '\1::_3C_'
repl '\<CCONV_' '_3C_' '^clang-tools-extra/clangd/Protocol\.h$'
repl_cpp_word {Convert,_3C}Category
repl_cpp_word 'CConvert(Options|ManualFix|Diagnostics)' '_3C\1'
repl_cpp_word {ccConvert,_3C}ManualFix
repl_cpp_word ccConvResultsReady _3CResultsReady
repl_word CheckedCConverter _3C  # Appears in clang/include/clang/Basic/LangOptions.def too
# Internal
repl_cpp_word execute{CConv,3C}Command
repl_cpp_word send{CConv,3C}Message
repl_cpp_word Is{CConv,3C}Command
repl_cpp_word '(clear|report)CConvDiagsForAllFiles' '\13CDiagsForAllFiles'
repl_word INTERACTIVE{CCCONV,3C}  # Appears in some CMake files too

# Various other occurrences in C++ code
repl_cpp '"CConv_(RealWild|AffWild)"' '"3C_\1"'
repl_cpp '"cconv\.(onlyThisPtr|applyAllPtr)"' '"3c.\1"'
repl_cpp '\("CConv(:)? ' '("3C\1 '  # Logging

repl_word run{CheckedCConvert,3C}  # for clang/tools/3c/utils/cc_conv

repl_word icconv clangd3c

repl_word checked_c_convert_bin '_3c_bin'  # Variable in clang/tools/3c/utils
repl_word checked-c-convert '3c'  # Various references from clang/tools/3c/utils and documentation

repl 'Checked C rewriter tool' 3C '^clang/test/3C/'

repl_word cconvClangDaemon 3cClangDaemon '/CMakeLists\.txt$'

# As of the original starting point, this looks right in all cases, but REVIEW IT CAREFULLY!
repl_word '[Cc]?[Cc][Cc]onv' 3C '(^|/)3[Cc]|^clang-tools-extra/clangd|^clang/include/clang'

repl fcheckedc_convert_tool f3c_tool  # Preceded by OPT_ in some instances
repl fcheckedc-convert-tool f3c-tool
