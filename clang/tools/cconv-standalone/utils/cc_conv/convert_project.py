"""
Given the path to the project and checked-c-convert binary,
this script runs checked-c-convert on all the files.

Specifically, it changes all the .c and .h files so that
they contain checked.h headers rather than regular header files.
Next, it gets compilation commands from compile_commands.json
and generate command line to run checked-c-convert.

This script requires that there exists a compile_commands.json
in the project folder.
"""

import os
import sys
import argparse
import logging
from includes_updater import updateProjectIncludes
from generate_ccommands import runCheckedCConvert

logging.basicConfig(format='%(asctime)s.%(msecs)03d %(levelname)s %(module)s - %(funcName)s: %(message)s',
                    datefmt='%Y-%m-%d %H:%M:%S',
                    level=logging.DEBUG)

# This default value will be overwritten if an alternate path is provided
# in an argument.
CHECKEDC_INCLUDE_REL_PATH = "projects/checkedc-wrapper/checkedc/include"
checkedcHeaderDir = os.path.abspath(
    os.path.join("../../../../..",
                 CHECKEDC_INCLUDE_REL_PATH))


# If the arg is a valid filename, returns the absolute path to it
def parseTheArg():
    global CHECKEDC_INCLUDE_REL_PATH
    global checkedcHeaderDir
    # get the directory based on `LLVM_SRC` environment variable.
    pathBasedDir = ""
    if 'LLVM_SRC' in os.environ:
        pathBasedDir = os.path.join(os.environ['LLVM_SRC'], CHECKEDC_INCLUDE_REL_PATH)
    checked_c_convert_bin = ""
    if 'LLVM_OBJ' in os.environ:
        checked_c_convert_bin = os.path.join(os.environ['LLVM_OBJ'], "bin/checked-c-convert")

    parser = argparse.ArgumentParser(description='Convert the provided project into Checked C.')

    parser.add_argument('--includeDir',
                        default=checkedcHeaderDir if os.path.exists(checkedcHeaderDir) else pathBasedDir,
                        required=False,
                        dest='includeDir',
                        help='Path to the checkedC headers, run from a checkedCclang repo')
    parser.add_argument("-p", "--prog_name", dest='prog_name', type=str, default=checked_c_convert_bin,
                        help='Program name to run. i.e., path to checked-c-converts')

    parser.add_argument("-pr", "--project_path", dest='project_path', type=str, required=True,
                        help='Path to the folder containing all project sources.')

    parser.add_argument("--skip", dest='skip_paths', action='append', type=str, default=[],
                        help='Relative path to source files that should be skipped.')

    parser.add_argument("-dr", dest='skip_exec', action='store_true', default=False,
                        help='Do not run the conversion. Just create the conversion script.')

    args = parser.parse_args()

    if not args.skip_exec and (not args.prog_name or not os.path.isfile(args.prog_name)):
        logging.error("Error: --prog_name argument is not a valid file..")
        logging.error("Provided argument: {} is not a file.".format(args.prog_name))
        sys.exit()

    if not args.includeDir or not os.path.isdir(args.includeDir):
        logging.error("Error: --includeDir argument must be the name of a directory.")
        logging.error("Provided argument: {} is not a directory.".format(args.includeDir))
        sys.exit()

    if not args.project_path or not os.path.isdir(args.project_path):
        logging.error("Error: --project_path argument must be the name of a directory.")
        logging.error("Provided argument: {} is not a directory.".format(args.project_path))
        sys.exit()

    return args


if __name__ == "__main__":
    # get the args
    progArgs = parseTheArg()
    # check compile_commands.json file.
    compileCmdsJson = os.path.join(progArgs.project_path, "compile_commands.json")
    if not os.path.exists(compileCmdsJson):
        logging.error("Error: Project folder does not contain compile_commands.json.")
        logging.error("compile_commands.json file: {} is not a directory.".format(compileCmdsJson))
        sys.exit()
    # replace include files
    logging.info("Updating include lines of all the source files.")
    updateProjectIncludes(progArgs.project_path, progArgs.includeDir)
    logging.info("Finished updating project files.")

    logging.info("Trying to convert all the source files to header files")
    runCheckedCConvert(progArgs.prog_name, compileCmdsJson, progArgs.includeDir,
                       progArgs.skip_paths, progArgs.skip_exec)
    logging.info("Finished converting all the files to checkedc files.")



