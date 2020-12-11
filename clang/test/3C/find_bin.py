# Set bin_path to the path that should be prepended to '3c', etc. to run them.
# bin_path will be empty (so $PATH is used) or have a trailing slash.
#
# TODO: Do we have other tools that should use this?

import sys
import os
os.chdir(os.path.dirname(__file__))
# Relative paths are now relative to clang/test/3C/ .

sys.path.insert(0, '../../../llvm/utils/lit')
import lit.util
sys.path.pop(0)

def die(msg):
  sys.stderr.write('Error: %s\n' % msg)
  sys.exit(1)

llvm_obj_maybe = os.environ.get('LLVM_OBJ')
standard_build_dir = '../../../build'
if llvm_obj_maybe is not None:
  bin_path = llvm_obj_maybe + '/bin/'
  if not os.path.isfile(bin_path + '3c'):
    die('$LLVM_OBJ is set but the bin directory does not contain 3c.')
elif os.path.isdir(standard_build_dir):
  bin_path = standard_build_dir + '/bin/'
  if not os.path.isfile(bin_path + '3c'):
    die('The standard build directory exists but does not contain 3c.')
elif lit.util.which('3c') is not None:
  # TODO: To help prevent mistakes, validate that the `3c` we found is under a
  # build directory linked to the current source tree? Or might users want to
  # do unusual things?
  bin_path = ''
else:
  die('Could not find 3c via $LLVM_OBJ, the standard build directory, or $PATH.')
