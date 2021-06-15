"""
Script that helps in copying and restoring .checked
to and from original files.
"""

import os
import argparse

ORIG_POSTFIX = ".orig"


def collect_all_files(curr_d, containts_str = None, postfix=None):
  ret_val = set()
  for f_name in os.listdir(curr_d):
    full_f = os.path.join(curr_d, f_name)
    if os.path.isdir(full_f):
      ret_val.update(collect_all_files(full_f, containts_str, postfix))
    elif containts_str is not None and containts_str in f_name:
        ret_val.add(full_f)
    elif postfix is not None and f_name.endswith(postfix):
        ret_val.add(full_f)

  return ret_val


def copy_checked_files(curr_files):
  global ORIG_POSTFIX
  for curr_f in curr_files:
    curr_d_path = os.path.dirname(curr_f)
    f_name = os.path.basename(curr_f)
    original_file_path = os.path.join(curr_d_path, f_name.split(".checked.")[0] + f_name[-2:])
    os.system("cp " + curr_f + " " + original_file_path)


def restore_original_files(curr_files):
  global ORIG_POSTFIX
  for curr_f in curr_files:
    curr_d_path = os.path.dirname(curr_f)
    f_name = os.path.basename(curr_f)
    original_file_path = os.path.join(curr_d_path, f_name.split(ORIG_POSTFIX)[0])
    os.system("cp " + curr_f + " " + original_file_path)


if __name__ == '__main__':
  parser = argparse.ArgumentParser(__file__, description="Script that takes care of replacing files.")
  parser.add_argument("-c", dest='backupc', action='store_true', default=False,
                      help='Backup original sources with checked c headers')
  parser.add_argument("-b", dest='backup', action='store_true', default=False,
                      help='Backup original sources (even without checked c headers)')
  parser.add_argument("-d", dest='src_dir',
                      type=str, required=True,
                      help='Path to the source directory.')
  args = parser.parse_args()
  if args.backupc:
    print("[*] Restoring backed-up files (with checked c headers) as original files.")
    all_files = collect_all_files(args.src_dir, None, ".backc")
    restore_original_files(all_files)
  elif args.backup:
    print("[*] Restoring backed-up files (without checked c headers) as original files.")
    all_files = collect_all_files(args.src_dir, None, ".orig")
    restore_original_files(all_files)
    print("[+] Restoring original files finished.")
  else:
    print("[*] Copying checked files as original files.")
    all_files = collect_all_files(args.src_dir, ".checked.", None)
    copy_checked_files(all_files)
    print("[+] Copying checked files finished.")
