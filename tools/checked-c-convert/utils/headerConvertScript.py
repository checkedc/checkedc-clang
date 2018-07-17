"""
This program takes a list of .c files, e.g. in a convert.sh file,
and converts any included headers for which checkedc has _checked 
to the checked versions. The list of checked headers is found in
the llvm/projects/checkedc-wrapper/checkedc/include directory.

NOTE: This does not  any local / project-specific headers
that have .checked.h versions from a previous run of checked-c-convert
Those must still be updated manually.
"""

import os, re, sys
import argparse

# This assumes the file is inside the tools/checked-c-convert/utils
# directory of a tools/checkedc-clang repo, which is inside an llvm repo
# containing the checkedc-wrapper. Adjust if the headers are elsewhere.
CHECKEDC_HEADER_DIR=os.path.abspath(
    os.path.join("../../../../.." ,
                 "projects/checkedc-wrapper/checkedc/include"))

# If the arg is a valid filename, returns the absolute path to it
def parseTheArg():
    parser = argparse.ArgumentParser(
        description='Convert includes of standard headers to their checked versions for a list of c files.')
    parser.add_argument(
        'filename', default="",
        help='Filename containing list of C files to have includes converted')
    args = parser.parse_args()

    if not args.filename or not os.path.isfile(args.filename):
        print("Error: Argument must be the name of a file.")
        print("Provided argument: {} is not a file.".format(args.filename))
        sys.exit()

    return os.path.abspath(args.filename)

# Initializes the find replace function so it can be run on multiple files
def makeFindReplace():    
    hFiles = ["<"+f+">" for f in os.listdir(CHECKEDC_HEADER_DIR)
              if os.path.isfile(os.path.join(CHECKEDC_HEADER_DIR, f))
              and not f.startswith("_builtin") and f.endswith("_checked.h")]

    replaceDict = dict((n.split('_', 1)[0]+".h>", n) for n in hFiles)

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
