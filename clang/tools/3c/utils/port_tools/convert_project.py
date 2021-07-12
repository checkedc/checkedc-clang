#!/usr/bin/env python3
"""
Given the path to the project and 3c binary,
this script runs 3c on all the files.

Specifically, it changes all the .c and .h files so that
they contain checked.h headers rather than regular header files.
Next, it gets compilation commands from compile_commands.json
and generate command line to run 3c.

This script requires that there exists a compile_commands.json
in the project folder.
"""

import os
import sys
import argparse
import logging
from generate_ccommands import run3C
from expand_macros import ExpandMacrosOptions

logging.basicConfig(format=('%(asctime)s.%(msecs)03d %(levelname)s '
                            '%(module)s - %(funcName)s: %(message)s'),
                    datefmt='%Y-%m-%d %H:%M:%S',
                    level=logging.DEBUG)


# If the arg is a valid filename, returns the absolute path to it
def parseTheArg():
    _3c_bin = ""
    if 'LLVM_OBJ' in os.environ:
        _3c_bin = os.path.join(os.environ['LLVM_OBJ'], "bin/3c")

    parser = argparse.ArgumentParser(
        description='Convert the provided project into Checked C.')

    parser.add_argument("-p",
                        "--prog_name",
                        dest='prog_name',
                        type=str,
                        default=_3c_bin,
                        help='Program name to run. i.e., path to 3c')

    parser.add_argument("--extra-3c-arg",
                        dest='extra_3c_args',
                        action='append',
                        type=str,
                        default=[],
                        help=('Extra argument to pass to 3c. '
                              'Multiple -extra-3c-arg options can be used.'))

    parser.add_argument(
        "-pr",
        "--project_path",
        dest='project_path',
        type=str,
        required=True,
        help='Path to the folder containing all project sources.')

    parser.add_argument(
        "--build_dir",
        dest='build_dir',
        type=str,
        help='Path to the folder containing compile_commands.json. '
        'Default: same as --project_path.')

    parser.add_argument(
        "--skip",
        dest='skip_paths',
        action='append',
        type=str,
        default=[],
        help='Relative path to source files that should be skipped.')

    parser.add_argument(
        "--expand_macros_before_conversion",
        dest='expand_macros_before_conversion',
        action='store_true',
        default=False,
        help=
        ('Before running 3c, attempt to expand macros in place in all source '
         'files based on the compiler options in compile_commands.json. This '
         'will help stop macros from interfering with 3C\'s ability to perform '
         'rewrites.'))
    parser.add_argument(
        '--include_before_undefs',
        dest='includes_before_undefs',
        action='append',
        # Start a combined list here rather than potentially duplicating the
        # options for every benchmark. TBD what we want to do longer term.
        default=['<signal.h>', '<ctype.h>'],
        help=
        ('With --expand_macros_before_conversion, #include the given filename '
         '(which should contain the double quotes or angle brackets) in each '
         'translation unit before undefining the macros specified via '
         '--undef_macro. Assuming the header has a multiple inclusion guard, '
         'this can be used to prevent a subsequent inclusion from defining the '
         'macros again.'))
    parser.add_argument(
        '--undef_macro',
        dest='undef_macros',
        action='append',
        default=['sa_handler', 'toupper', 'tolower'],
        help=
        ('With --expand_macros_before_conversion, #undef the given macro name '
         'in each translation unit. Can be used to prevent problematic '
         '(e.g., recursive) macros from being expanded.'))

    parser.add_argument(
        "-dr",
        dest='skip_exec',
        action='store_true',
        default=False,
        help='Do not run the conversion. Just create the conversion script.')

    args = parser.parse_args()

    if not args.skip_exec and (not args.prog_name or
                               not os.path.isfile(args.prog_name)):
        logging.error("Error: --prog_name argument is not a valid file..")
        logging.error("Provided argument: {} is not a file.".format(
            args.prog_name))
        sys.exit(1)

    if not args.project_path or not os.path.isdir(args.project_path):
        logging.error(
            "Error: --project_path argument must be the name of a directory.")
        logging.error("Provided argument: {} is not a directory.".format(
            args.project_path))
        sys.exit(1)

    if not args.build_dir:
        args.build_dir = args.project_path
    if not os.path.isdir(args.build_dir):
        logging.error(
            "Error: --build_dir argument must be the name of a directory.")
        logging.error("Provided argument: {} is not a directory.".format(
            args.build_dir))
        sys.exit(1)

    return args


if __name__ == "__main__":
    # get the args
    progArgs = parseTheArg()
    # check compile_commands.json file.
    compileCmdsJson = os.path.join(progArgs.build_dir, "compile_commands.json")
    if not os.path.exists(compileCmdsJson):
        logging.error(
            "Error: Build directory does not contain compile_commands.json.")
        logging.error("compile_commands.json file: {} does not exist.".format(
            compileCmdsJson))
        sys.exit(1)

    logging.info("Trying to convert all the source files to header files")
    run3C(
        progArgs.prog_name, progArgs.extra_3c_args, progArgs.project_path,
        compileCmdsJson, progArgs.skip_paths,
        ExpandMacrosOptions(progArgs.expand_macros_before_conversion,
                            progArgs.includes_before_undefs,
                            progArgs.undef_macros), progArgs.skip_exec)
    logging.info("Finished converting all the files to checkedc files.")
