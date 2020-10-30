"""

"""
import re
import os
import sys
import json
import traceback
import subprocess
import logging

SLASH = os.sep
# file in which the individual commands will be stored
INDIVIDUAL_COMMANDS_FILE = os.path.realpath("convert_individual.sh")
# file in which the total commands will be stored.
TOTAL_COMMANDS_FILE = os.path.realpath("convert_all.sh")

VSCODE_SETTINGS_JSON = os.path.realpath("settings.json")

# to separate multiple commands in a line
CMD_SEP = " &"
DEFAULT_ARGS = ["-dump-stats", "-output-postfix=checked", "-dump-intermediate"]
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
        argsstrs = map(lambda x : "\"" + x + "\"", self.args)
        argsstrs = ",\n".join(argsstrs)
        fp.write(argsstrs)
        fp.write("]\n")
        fp.write("}")
        fp.close()

def getCheckedCArgs(argument_list, checkedc_include_dir, work_dir):
    """
      Convert the compilation arguments (include folder and #defines)
      to checked C format.
    :param argument_list: list of compiler argument.
    :param checkedc_include_dir: Directory in which Checked C header files are located.
    :param work_dir: Path to the working directory from which
                     the compilation command was run.
    :return: checked c args
    """
    clang_x_args = []
    new_arg_list = []
    new_arg_list.extend(argument_list)
    new_arg_list.append("-I" + checkedc_include_dir)
    for curr_arg in new_arg_list:
        if curr_arg.startswith("-D") or curr_arg.startswith("-I"):
            if curr_arg.startswith("-I"):
                # if this is relative path,
                # convert into absolute path
                if not os.path.isabs(curr_arg[2:]):
                    curr_arg = "-I" + os.path.abspath(os.path.join(work_dir, curr_arg[2:]))
            clang_x_args.append('-extra-arg-before=' + curr_arg)
    # disable all warnings.
    clang_x_args.append('-extra-arg-before=-w')
    return clang_x_args


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


def run3C(checkedc_bin, compile_commands_json, checkedc_include_dir, skip_paths,
          skip_running=False, run_individual=False):
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
        logging.error("failed to get commands from compile commands json:" + compile_commands_json)
        return

    s = set()
    total_x_args = []
    all_files = []
    for i in cmds:
        file_to_add = i['file']
        compiler_x_args = []
        target_directory = ""
        if file_to_add.endswith(".cpp"):
            continue  # Checked C extension doesn't support cpp files yet

        # BEAR uses relative paths for 'file' rather than absolute paths. It also
        # has a field called 'arguments' instead of 'command' in the cmake style.
        # Use that to detect BEAR and add the directory.
        if 'arguments' in i and not 'command' in i:
            # BEAR. Need to add directory.
            file_to_add = i['directory'] + SLASH + file_to_add
            # get the 3c and compiler arguments
            compiler_x_args = getCheckedCArgs(i["arguments"], checkedc_include_dir, i['directory'])
            total_x_args.extend(compiler_x_args)
            # get the directory used during compilation.
            target_directory = i['directory']
        file_to_add = os.path.realpath(file_to_add)
        matched = False
        for j in filters:
            if j.match(file_to_add) is not None:
                matched = True
        if not matched:
            all_files.append(file_to_add)
            s.add((frozenset(compiler_x_args), target_directory, file_to_add))

    # get the common path of the files as the base directory
    compilation_base_dir = os.path.commonprefix(all_files)
    # if this is not a directory? get the directory
    # this can happen when we have files like: /a/b/c1.c, /a/b/c2.c
    # the common prefix will be /a/b/c which is not correct, what we need is:
    # /a/b
    if len(all_files) > 0 and \
            not os.path.exists(os.path.join(compilation_base_dir, os.path.basename(all_files[0]))):
        compilation_base_dir = os.path.dirname(compilation_base_dir)
    prog_name = checkedc_bin
    f = open(INDIVIDUAL_COMMANDS_FILE, 'w')
    f.write("#!/bin/bash\n")
    for compiler_args, target_directory, src_file in s:
        args = []
        # get the command to change the working directory
        change_dir_cmd = ""
        if len(target_directory) > 0:
            change_dir_cmd = "cd " + target_directory + CMD_SEP
        else:
            # default working directory
            target_directory = os.getcwd()
        args.append(prog_name)
        if len(compiler_args) > 0:
            args.extend(list(compiler_args))
        args.append('-base-dir="' + compilation_base_dir + '"')
        args.extend(DEFAULT_ARGS)
        args.append(src_file)
        # run individual commands.
        if run_individual:
            logging.debug("Running:" + ' '.join(args))
            subprocess.check_call(' '.join(args), cwd=target_directory, shell=True)
        # prepend the command to change the working directory.
        if len(change_dir_cmd) > 0:
            args = [change_dir_cmd] + args
        f.write(" \\\n".join(args))
        f.write("\n")
    f.close()
    logging.debug("Saved all the individual commands into the file:" + INDIVIDUAL_COMMANDS_FILE)
    os.system("chmod +x " + INDIVIDUAL_COMMANDS_FILE)

    vcodewriter = VSCodeJsonWriter()
    # get path to clangd3c
    vcodewriter.setClangdPath(os.path.join(os.path.dirname(prog_name), "clangd3c"))
    args = []
    args.append(prog_name)
    args.extend(DEFAULT_ARGS)
    args.extend(list(set(total_x_args)))
    vcodewriter.addClangdArg("-log=verbose")
    vcodewriter.addClangdArg(args[1:])
    args.append('-base-dir="' + compilation_base_dir + '"')
    vcodewriter.addClangdArg('-base-dir=' + compilation_base_dir)
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
    logging.debug("Saved the total command into the file:" + TOTAL_COMMANDS_FILE)
    os.system("cp " + TOTAL_COMMANDS_FILE + " " + os.path.join(compilation_base_dir, os.path.basename(TOTAL_COMMANDS_FILE)))
    logging.debug("Saved to:" + os.path.join(compilation_base_dir, os.path.basename(TOTAL_COMMANDS_FILE)))
    os.system("cp " + INDIVIDUAL_COMMANDS_FILE + " " + os.path.join(compilation_base_dir, os.path.basename(INDIVIDUAL_COMMANDS_FILE)))
    logging.debug("Saved to:" + os.path.join(compilation_base_dir, os.path.basename(INDIVIDUAL_COMMANDS_FILE)))
    logging.debug("VSCode Settings json saved to:" + VSCODE_SETTINGS_JSON)
    return
