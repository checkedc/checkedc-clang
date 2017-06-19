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

DEFAULT_ARGS = ["-verbose", "-dump-stats", "-extra-arg-before=--driver-mode=cl", "-output-postfix=checked"]

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
    s.add(os.path.realpath(i['file']))

  print s
  args = []
  args.append(args.prog_name)
  args.extend(DEFAULT_ARGS)
  args.extend(list(s))
  f = open('bla', 'w')
  f.write(" ".join(args))
  f.close()
  subprocess.check_call(args)

  return

if __name__ == '__main__':
  parser = argparse.ArgumentParser("runner")
  parser.add_argument("compile_commands", type=str)
  parser.add_argument("prog_name", type=str)
  args = parser.parse_args()
  runMain(args)
