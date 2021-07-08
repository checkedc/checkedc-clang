"""

"""
from typing import List
import re
import os
import sys
import json
import traceback
import subprocess
import logging
from common import TranslationUnitInfo
from expand_macros import expandMacros, ExpandMacrosOptions

SLASH = os.sep
# file in which the individual commands will be stored
INDIVIDUAL_COMMANDS_FILE = os.path.realpath("convert_individual.sh")
# file in which the total commands will be stored.
TOTAL_COMMANDS_FILE = os.path.realpath("convert_all.sh")

VSCODE_SETTINGS_JSON = os.path.realpath("settings.json")

# to separate multiple commands in a line
CMD_SEP = " &&"
DEFAULT_ARGS = ["-dump-stats"]
if os.name == "nt":
    DEFAULT_ARGS.append("-extra-arg-before=--driver-mode=cl")
    CMD_SEP = " ;"


class VSCodeJsonWriter():

    def __init__(self):
        self.clangd_path = ""
        self.args = []

    def setClangdPath(self, cdpath):
        self.clangd_path = cdpath

    def addClangdArg(self, arg):
        if isinstance(arg, list):
            self.args.extend(arg)
        else:
            self.args.append(arg)

    def writeJsonFile(self, outputF):
        fp = open(outputF, "w")
        fp.write("{\"clangd.path\":\"" + self.clangd_path + "\",\n")
        fp.write("\"clangd.arguments\": [\n")
        argsstrs = map(lambda x: "\"" + x + "\"", self.args)
        argsstrs = ",\n".join(argsstrs)
        fp.write(argsstrs)
        fp.write("]\n")
        fp.write("}")
        fp.close()


def getCheckedCArgs(argument_list):
    """
      Adjust the compilation arguments. This is now used only by
      expand_macros_before_conversion since 3c takes the arguments directly from
      the compilation database. Thus, we no longer use -extra-arg-before here.

    :param argument_list: list of compiler argument.
    :return: (checked c args, output filename)
    """
    # New approach: Rather than keeping only specific flags, try keeping
    # everything except `-c` (because we will add `-E` if we preprocess the
    # translation unit) and the source file name (assumed to be the last
    # argument) because it's hard to know what flags different benchmarks might
    # be using that might affect the default preprocessor state. We rely on
    # setting the working directory instead of trying to recognize all paths
    # that might need to be made absolute here.
    clang_x_args = []
    source_filename = argument_list[-1]
    assert source_filename.endswith('.c')
    # By default; may be overwritten below.
    output_filename = source_filename[:-len('.c')] + '.o'
    idx = 0
    while idx < len(argument_list) - 1:
        arg = argument_list[idx]
        idx += 1
        if arg == '-c':
            pass
        elif arg == '-o':
            # Remove the output filename from the argument list and save it
            # separately.
            output_filename = argument_list[idx]
            idx += 1
        else:
            clang_x_args.append(arg)
    # Disable all Clang warnings. Generally, we don't want to do anything about
    # them and they are just distracting.
    clang_x_args.append('-w')
    return (clang_x_args, output_filename)


def tryFixUp(s):
    """
    Fix-up for a failure between cmake and nmake.
    """
    b = open(s, 'r').read()
    b = re.sub(r'@<<\n', "", b)
    b = re.sub(r'\n<<', "", b)
    f = open(s, 'w')
    f.write(b)
    f.close()
    return


# We no longer take the checkedc_include_dir here because we assume the working
# tree is set up so that the Checked C headers get used automatically by 3c.
def run3C(checkedc_bin,
          extra_3c_args,
          compilation_base_dir,
          compile_commands_json,
          skip_paths,
          expand_macros_opts: ExpandMacrosOptions,
          skip_running=False,
          run_individual=False):
    global INDIVIDUAL_COMMANDS_FILE
    global TOTAL_COMMANDS_FILE
    runs = 0
    cmds = None
    filters = []
    for i in skip_paths:
        filters.append(re.compile(i))
    while runs < 2:
        runs = runs + 1
        try:
            cmds = json.load(open(compile_commands_json, 'r'))
        except:
            traceback.print_exc()
            tryFixUp(compile_commands_json)

    if cmds == None:
        logging.error("failed to get commands from compile commands json:" +
                      compile_commands_json)
        return

    translation_units: List[TranslationUnitInfo] = []
    all_files = []
    for i in cmds:
        file_to_add = i['file']
        compiler_path = None  # XXX Clean this up
        compiler_x_args = []
        output_filename = None
        target_directory = ""
        if file_to_add.endswith(".cpp"):
            continue  # Checked C extension doesn't support cpp files yet

        # BEAR uses relative paths for 'file' rather than absolute paths. It
        # also has a field called 'arguments' instead of 'command' in the cmake
        # style. Use that to detect BEAR and add the directory.
        if 'arguments' in i and not 'command' in i:
            # BEAR. Need to add directory.
            file_to_add = i['directory'] + SLASH + file_to_add
            compiler_path = i['arguments'][0]
            # get the compiler arguments
            (compiler_x_args,
             output_filename) = getCheckedCArgs(i["arguments"][1:])
            # get the directory used during compilation.
            target_directory = i['directory']
        file_to_add = os.path.realpath(file_to_add)
        matched = False
        for j in filters:
            if j.match(file_to_add) is not None:
                matched = True
        if not matched:
            all_files.append(file_to_add)
            tu = TranslationUnitInfo(compiler_path, compiler_x_args,
                                     target_directory, file_to_add,
                                     output_filename)
            translation_units.append(tu)

    expandMacros(expand_macros_opts, compilation_base_dir, translation_units)

    prog_name = checkedc_bin
    f = open(INDIVIDUAL_COMMANDS_FILE, 'w')
    f.write("#!/bin/bash\n")
    for tu in translation_units:
        args = []
        # get the command to change the working directory
        target_directory = tu.target_directory
        change_dir_cmd = ""
        if len(target_directory) > 0:
            change_dir_cmd = "cd " + target_directory + CMD_SEP
        else:
            # default working directory
            target_directory = os.getcwd()
        args.append(prog_name)
        args.extend(DEFAULT_ARGS)
        args.extend(extra_3c_args)
        # Even when we run 3c on a single file, we can let it read the compiler
        # options from the compilation database.
        args.append('-p')
        args.append(compile_commands_json)
        # ...but we need to add -w, as in getCheckedCArgs.
        args.append('-extra-arg=-w')
        args.append('-base-dir="' + compilation_base_dir + '"')
        args.append('-output-dir="' + compilation_base_dir + '/out.checked"')
        args.append(tu.input_filename)
        # run individual commands.
        if run_individual:
            logging.debug("Running:" + ' '.join(args))
            subprocess.check_call(' '.join(args),
                                  cwd=target_directory,
                                  shell=True)
        # prepend the command to change the working directory.
        if len(change_dir_cmd) > 0:
            args = [change_dir_cmd] + args
        f.write(" \\\n".join(args))
        f.write("\n")
    f.close()
    logging.debug("Saved all the individual commands into the file:" +
                  INDIVIDUAL_COMMANDS_FILE)
    os.system("chmod +x " + INDIVIDUAL_COMMANDS_FILE)

    vcodewriter = VSCodeJsonWriter()
    # get path to clangd3c
    #
    # clangd3c is believed not to work, but since this code has been here for a
    # while and no one has been bothered by the fact that it didn't work, we
    # won't bother removing it now; hopefully clangd3c will eventually be back.
    vcodewriter.setClangdPath(
        os.path.join(os.path.dirname(prog_name), "clangd3c"))
    args = []
    args.append(prog_name)
    args.extend(DEFAULT_ARGS)
    args.extend(extra_3c_args)
    args.append('-p')
    args.append(compile_commands_json)
    args.append('-extra-arg=-w')
    vcodewriter.addClangdArg("-log=verbose")
    vcodewriter.addClangdArg(args[1:])
    args.append('-base-dir="' + compilation_base_dir + '"')
    vcodewriter.addClangdArg('-base-dir=' + compilation_base_dir)
    # Try to choose a name unlikely to collide with anything in any real
    # project.
    args.append('-output-dir="' + compilation_base_dir + '/out.checked"')
    args.extend(list(set(all_files)))
    vcodewriter.addClangdArg(list(set(all_files)))
    vcodewriter.writeJsonFile(VSCODE_SETTINGS_JSON)

    f = open(TOTAL_COMMANDS_FILE, 'w')
    f.write("#!/bin/bash\n")
    f.write(" \\\n".join(args))
    f.close()
    os.system("chmod +x " + TOTAL_COMMANDS_FILE)
    # run whole command
    if not run_individual and not skip_running:
        logging.info("Running:" + str(' '.join(args)))
        subprocess.check_call(' '.join(args), shell=True)
    logging.debug("Saved the total command into the file:" +
                  TOTAL_COMMANDS_FILE)
    os.system("cp " + TOTAL_COMMANDS_FILE + " " + os.path.join(
        compilation_base_dir, os.path.basename(TOTAL_COMMANDS_FILE)))
    logging.debug("Saved to:" + os.path.join(
        compilation_base_dir, os.path.basename(TOTAL_COMMANDS_FILE)))
    os.system("cp " + INDIVIDUAL_COMMANDS_FILE + " " + os.path.join(
        compilation_base_dir, os.path.basename(INDIVIDUAL_COMMANDS_FILE)))
    logging.debug("Saved to:" + os.path.join(
        compilation_base_dir, os.path.basename(INDIVIDUAL_COMMANDS_FILE)))
    logging.debug("VSCode Settings json saved to:" + VSCODE_SETTINGS_JSON)
    return
