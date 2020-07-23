"""

"""
import os, re, sys
import argparse
import logging


def makeFindReplace(checkedcHeaderDir):
    """
       Initializes the find replace function so it can be run on multiple files
    :param checkedcHeaderDir:
    :return: Function
    """
    hFiles = ["<" + f + ">" for f in os.listdir(checkedcHeaderDir)
              if os.path.isfile(os.path.join(checkedcHeaderDir, f))
              and not f.startswith("_builtin") and f.endswith("_checked.h")]

    replaceDict = dict((n.split('_', 1)[0] + ".h>", n) for n in hFiles)

    if not replaceDict:
        logging.error(
            "Error: No _checked header files found as replacements. Is your --includeDir set to the file that contains the _checked headers?")
        logging.error("Current includeDir is %s", checkedcHeaderDir)
        sys.exit()

    rx = re.compile('|'.join(map(re.escape, replaceDict)))

    def oneMatch(match):
        return replaceDict[match.group(0)]

    def findReplace(text):
        return rx.sub(oneMatch, text)

    return findReplace


def updateProjectIncludes(project_dir, checkedc_header_dir):
    """
        Update all the source files in the project directory
        by changing headers to checked headers.
    :param project_dir: Directory of the source project.
    :param checkedc_header_dir: Directory containing checkedC headers.
    :return:
    """
    headerReplacer = makeFindReplace(checkedc_header_dir)
    all_source_files = [os.path.abspath(os.path.join(dp, f)) for dp, dn, filenames in os.walk(project_dir)
                        for f in filenames if f.endswith(".c") or f.endswith(".h")]
    logging.debug("Got %s source files to process", str(len(all_source_files)))
    for cfile in all_source_files:
        logging.debug("Processing file: %s", cfile)
        contents = ""
        with open(cfile, 'r') as f:
            contents = f.readlines()
        backup_source_file = cfile + ".orig"
        # if the source file is not backed up?
        # back up original sources
        if not os.path.exists(backup_source_file):
            with open(backup_source_file, 'w') as f:
                f.writelines(contents)
        # replace the header files.
        new_contents = [headerReplacer(line) for line in contents]
        with open(cfile, 'w') as f:
            f.writelines(new_contents)
        backup_source_file = cfile + ".backc"
        with open(backup_source_file, 'w') as f:
            f.writelines(new_contents)
    logging.debug("Replaced header files in all the source files of the project.")
