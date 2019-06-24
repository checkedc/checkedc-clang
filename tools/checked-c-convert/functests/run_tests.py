import os
import sys
import argparse
import filecmp


class bcolors:
  HEADER = '\033[95m'
  OKBLUE = '\033[94m'
  OKGREEN = '\033[92m'
  WARNING = '\033[93m'
  FAIL = '\033[91m'
  ENDC = '\033[0m'
  BOLD = '\033[1m'
  UNDERLINE = '\033[4m'


def get_all_tests(dir_folder):
  tests = []
  for curr_f in os.listdir(dir_folder):
    curr_f = os.path.join(dir_folder, curr_f)
    if os.path.isdir(curr_f):
      tests.extend(get_all_tests(curr_f))
    else:
      if not curr_f.endswith("expected.c"):
        expected_file = curr_f[:-1] + "expected.c"
        if os.path.exists(expected_file):
          tests.append((curr_f, expected_file))
  return tests


def test_diff(actual_out_file, expected_out_file):
  return filecmp.cmp(actual_out_file, expected_out_file)

def run_tests(prog, tests):
  allgood = True
  for act_file, expected_file in tests:
    cmd_line = prog + " -output-postfix=actual " + act_file + " >/dev/null 2>/dev/null"
    print(bcolors.OKBLUE + "[*] Testing:" + act_file +  bcolors.ENDC)
    os.system(cmd_line)
    actual_out = act_file[:-1] + "actual.c"
    if not test_diff(actual_out, expected_file):
      print(bcolors.FAIL + "[-] Expected file:" + expected_file + " and Actual file:" +
            actual_out + " doesn't match." + bcolors.ENDC)
      allgood = False
    else:
      print(bcolors.OKGREEN + "[+] Test " + act_file + " Passed." + bcolors.ENDC)
  return allgood


if __name__ == '__main__':

  parser = argparse.ArgumentParser("FuntionalityTester", description="Script that checks functionality of "
                                                                     "checked-c-convert tool")

  parser.add_argument("-p","--prog_name", dest='prog_name',type=str, required=True,
                      help='Program name to run. i.e., path to checked-c-convert')
  args = parser.parse_args()
  if not args.prog_name or not os.path.isfile(args.prog_name):
    print("Error: Path to the program to run is invalid.")
    print("Provided argument: {} is not a file.".format(args.prog_name))
    sys.exit()

  testfolder = os.path.dirname(os.path.abspath(__file__))
  all_tests = get_all_tests(testfolder)
  print(bcolors.HEADER + "[*] Got:" + str(len(all_tests)) + " tests." + bcolors.ENDC)
  print(bcolors.HEADER + "[*] Running Tests." + bcolors.ENDC)
  if run_tests(args.prog_name, all_tests):
    print(bcolors.OKGREEN + "[+] ALL TESTS PASSED." + bcolors.ENDC)
  else:
    print(bcolors.FAIL + "[-] TESTS FAILED." + bcolors.ENDC)