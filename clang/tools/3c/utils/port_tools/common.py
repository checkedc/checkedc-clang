# Data structures that need to be imported by both generate_ccommands and
# expand_macros.

from typing import List, NamedTuple
import functools
import os

# We are assuming it's OK to cache canonical paths for the lifetime of any
# process that uses this code.
realpath_cached = functools.lru_cache(maxsize=None)(os.path.realpath)


class TranslationUnitInfo(NamedTuple):
    compiler_path: str
    # Any file paths in compiler_args (-I, etc.), input_filename, and
    # output_filename may be relative to target_directory.
    compiler_args: List[str]
    target_directory: str
    input_filename: str
    output_filename: str

    def realpath(self, path: str):
        return realpath_cached(os.path.join(self.target_directory, path))

    # Perhaps this could be cached. It's not a big cost though.
    @property
    def input_realpath(self):
        return self.realpath(self.input_filename)

    @property
    def output_realpath(self):
        return self.realpath(self.output_filename)
