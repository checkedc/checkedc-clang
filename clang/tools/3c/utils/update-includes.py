#!/usr/bin/env python
"""
This program takes a list of .c or .h files, e.g. in a convert.sh file,
and converts any included c library headers which have _checked versions
in the checkedC repo to the _checked versions. It takes a second optional
argument --includeDir to set the location of the _checked versions of the
headers. If this program is being run from inside a checkedC clang repo,
the list of checked headers might be found in the (default) directory:
llvm/projects/checkedc-wrapper/checkedc/include 

NOTE: This program does not convert any local / project-specific headers
that have .checked.h versions from a previous run of 3c.
Those must still be updated manually.
"""

import os, re, sys
import argparse

# This default value will be overwritten if an alternate path is provided
# in an argument.
#
# Relative path from "llvm" dir (LLVM_SRC if specified)
CHECKEDC_INCLUDE_REL_PATH = "projects/checkedc-wrapper/checkedc/include"

checkedcHeaderDir = os.path.abspath(
    os.path.join(os.path.dirname(__file__),
                 # Relative path from clang/tools/3c/utils to llvm
                 "../../../../llvm",
                 CHECKEDC_INCLUDE_REL_PATH))


# If the arg is a valid filename, returns the absolute path to it
def parseTheArg():
    global checkedcHeaderDir
    global CHECKEDC_INCLUDE_REL_PATH
    # get the directory based on `LLVM_SRC` environment variable.
    pathBasedDir = ""
    if 'LLVM_SRC' in os.environ:
      pathBasedDir = os.path.join(os.environ['LLVM_SRC'], CHECKEDC_INCLUDE_REL_PATH)

    parser = argparse.ArgumentParser(description='Convert includes of standard headers to their '
                                                 'checked versions for a list of c files.')
    parser.add_argument('filename', default="",
                        help='Filename containing list of C files to have includes converted')
    parser.add_argument('--includeDir',
                        default=checkedcHeaderDir if os.path.exists(checkedcHeaderDir) else pathBasedDir,
                        required=False,
                        help='Path to the checkedC headers, run from a checkedCclang repo')
    args = parser.parse_args()

    if not args.filename or not os.path.isfile(args.filename):
        print("Error: First argument must be the name of a file.")
        print("Provided argument: {} is not a file.".format(args.filename))
        sys.exit()

    if not args.includeDir or not os.path.isdir(args.includeDir):
        print("Error: --includeDir argument must be the name of a directory.")
        print("Provided argument: {} is not a directory.".format(args.includeDir))
        sys.exit()
        
    checkedcHeaderDir = os.path.abspath(args.includeDir)

    return os.path.abspath(args.filename)


# Initializes the find replace function so it can be run on multiple files
def makeFindReplace():    
    hFiles = ["<"+f+">" for f in os.listdir(checkedcHeaderDir)
              if os.path.isfile(os.path.join(checkedcHeaderDir, f))
              and not f.startswith("_builtin") and f.endswith("_checked.h")]

    replaceDict = dict((n.split('_', 1)[0]+".h>", n) for n in hFiles)

    if not replaceDict:
        print("Error: No _checked header files found as replacements. Is your --includeDir set to the file that contains the _checked headers?")
        print("Current includeDir is {}".format(checkedcHeaderDir))
        sys.exit()

    rx = re.compile('|'.join(map(re.escape, replaceDict)))
    def oneMatch(match):
        return replaceDict[match.group(0)]
    def findReplace(text):
        return rx.sub(oneMatch, text)
    return findReplace


if __name__ == "__main__":
    pathToListFile = parseTheArg()
    findReplace = makeFindReplace()
    with open(pathToListFile, 'r') as listFile:
        for maybeCodeFile in listFile.readlines():
            if not re.search("\.c\s|\.h\s", maybeCodeFile):
                continue
            contents = ""
            codeFile = maybeCodeFile.rstrip('\\\r\n ')
            with open(codeFile, 'r') as f:
                contents = f.readlines()
            new_contents = [findReplace(line) for line in contents]
            with open(codeFile, 'w') as f:
                f.writelines(new_contents)
