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
SLASH = "/"

DEFAULT_ARGS = ["-dump-stats", "-output-postfix=checked"]
if os.name == "nt":
  DEFAULT_ARGS.append("-extra-arg-before=--driver-mode=cl")
  SLASH = "\\"

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
    if file_to_add.endswith(".cpp"):
      continue # Checked C extension doesn't support cpp files yet

    # BEAR uses relative paths for 'file' rather than absolute paths. It also 
    # has a field called 'arguments' instead of 'command' in the cmake style.
    # Use that to detect BEAR and add the directory.
    if 'arguments' in i and not 'command' in i:
      # BEAR. Need to add directory.
      file_to_add = i['directory'] + SLASH + file_to_add
    file_to_add = os.path.realpath(file_to_add)
    s.add(file_to_add)

  print s

  prog_name = args.prog_name
  args = []
  args.append(prog_name)
  args.extend(DEFAULT_ARGS)
  args.extend(list(s))
  f = open('convert.sh', 'w')
  f.write(" \\\n".join(args))
  f.close()
  subprocess.check_call(args)

  return

if __name__ == '__main__':
  parser = argparse.ArgumentParser("runner")
  parser.add_argument("compile_commands", type=str)
  parser.add_argument("prog_name", type=str)
  args = parser.parse_args()
  runMain(args)
