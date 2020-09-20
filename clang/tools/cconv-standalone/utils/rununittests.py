#!/usr/bin/env python3
import subprocess
import sys
import re

testpath = sys.argv[1]

proc = subprocess.run(['./llvm-lit', testpath], capture_output=True)

testresults = proc.stdout.decode().split('\n')

def isfailingtest(line):
    return re.match('^\s*Clang ::', line) and ('BUG' not in line)

failing = list(filter(isfailingtest, testresults))
exit(0 if len(failing) == 0 else 1)
