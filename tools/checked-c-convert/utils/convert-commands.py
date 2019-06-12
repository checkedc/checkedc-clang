import re
import os
import json
import argparse
import traceback
import subprocess

"""
This tool will invoke checked-c-convert on a compile_commands.json database. 
It contains some work-arounds for cmake+nmake generated compile_commands.json 
files, where the files are malformed. 
"""
SLASH = os.sep
# to separate multiple commands in a line
CMD_SEP = " &"

DEFAULT_ARGS = ["-dump-stats", "-output-postfix=checked"]
if os.name == "nt":
  DEFAULT_ARGS.append("-extra-arg-before=--driver-mode=cl")
  CMD_SEP = " ;"

def getCheckedCArgs(argument_list):
  """
    Convert the compilation arguments (include folder and #defines)
    to checked C format.
  :param argument_list: list of compiler argument.
  :return: argument string
  """
  clang_x_args = []
  for curr_arg in argument_list:
    if curr_arg.startswith("-D") or curr_arg.startswith("-I"):
      clang_x_args.append('-extra-arg=' + curr_arg)
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

def runMain(args):
  runs = 0
  cmds = None
  while runs < 2:
    runs = runs + 1
    try:
      cmds = json.load(open(args.compile_commands, 'r'))
    except:
      traceback.print_exc()
      tryFixUp(args.compile_commands)

  if cmds == None:
    print "failed"
    return

  s = set()
  for i in cmds:
    file_to_add = i['file']
    compiler_args = ""
    target_directory = ""
    if file_to_add.endswith(".cpp"):
      continue # Checked C extension doesn't support cpp files yet

    # BEAR uses relative paths for 'file' rather than absolute paths. It also 
    # has a field called 'arguments' instead of 'command' in the cmake style.
    # Use that to detect BEAR and add the directory.
    if 'arguments' in i and not 'command' in i:
      # BEAR. Need to add directory.
      file_to_add = i['directory'] + SLASH + file_to_add
      # get the compiler arguments
      compiler_args = getCheckedCArgs(i["arguments"])
      # get the directory used during compilation.
      target_directory = i['directory']
    file_to_add = os.path.realpath(file_to_add)
    s.add((frozenset(compiler_args), target_directory, file_to_add))

  prog_name = args.prog_name
  f = open('convert.sh', 'w')
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
    args.extend(DEFAULT_ARGS)
    args.append(src_file)
    print str(args)
    subprocess.check_call(args, cwd=target_directory)
    # prepend the command to change the working directory.
    if len(change_dir_cmd) > 0:
      args = [change_dir_cmd] + args
    f.write(" \\\n".join(args))
    f.write("\n")
  f.close()
  return

if __name__ == '__main__':
  parser = argparse.ArgumentParser("runner")
  parser.add_argument("compile_commands", type=str)
  parser.add_argument("prog_name", type=str)
  args = parser.parse_args()
  runMain(args)
