# Special preprocessor wrapper that expands macros in source files in-place
# without inlining `#include`s. This lets 3C rewrite program elements in their
# original locations without macros getting in the way and without 3C having to
# match up copies of the same program element included from the same header file
# into different translation units.
#
# We leave all preprocessor directives (including `#define`) unmodified so that
# during actual compilation, the preprocessor state will (hopefully) be exactly
# the same at each point as it would have been if this module were not used.
# This is important for evaluating `#if` conditions and expanding macros that we
# aren't able to expand in this phase due to special cases.
#
# We don't want to reimplement macro expansion here. Instead, we run the
# preprocessor on each translation unit and associate each line of the output
# with the original source line based on the line markers inserted by the
# preprocessor. Depending on the use of `#include`, `#if`, etc., a given source
# line may correspond to zero, one, or more macro-expanded lines in total among
# the preprocessor outputs of the entire set of translation units. In the common
# case, we expect the content of any and all preprocessed lines of a given
# source line to be identical, though in some pathological cases, it may differ
# (for example, if a line of a header file references a macro that is defined
# differently in different translation units before the header file is
# included). If the preprocessed content is indeed the same, then we replace the
# source line with that content.
#
# We ran into various special cases for which we have implemented workarounds.
# There are probably more special cases that we haven't seen yet. To ensure we
# detect any problems, we verify that the preprocessor output for each
# translation unit using the _original_ compiler options (not using options
# specific to this module such as `undef_macros`) is identical before and after
# our edits.

from typing import List, NamedTuple, Dict
import collections
import logging
import os
import re
import subprocess
from common import TranslationUnitInfo, realpath_cached


class ExpandMacrosOptions(NamedTuple):
    # If `enable` is false, the other options aren't used.
    enable: bool
    includes_before_undefs: List[str]
    undef_macros: List[str]


class FileLinePos(NamedTuple):
    fname: str
    lineno: int

    def __str__(self):
        return f'{self.fname}:{self.lineno}'


class SourceLineInfo:
    # non-blank expanded content -> list of locations _in preprocessor output
    # files_ where that content was seen as an expansion of the source line
    # represented by this SourceLineInfo.
    expansion_occurrences: Dict[str, List[FileLinePos]]
    # Did we see at least one blank expansion of this source line?
    saw_blank_expansion: bool

    def __init__(self):
        self.saw_blank_expansion = False
        self.expansion_occurrences = collections.defaultdict(lambda: [])


class SourceFileInfo:
    lines: Dict[int, SourceLineInfo]

    def __init__(self):
        self.lines = collections.defaultdict(lambda: SourceLineInfo())


# While text is normally assumed to be UTF-8 these days (and that's the default
# for text I/O in Python 3), at least one benchmark (thttpd) uses a few
# ISO-8859-1 characters, which we'd fail to read as UTF-8. Since we are agnostic
# to the meaning of non-ASCII bytes in the source files, we just process them
# all using an arbitrary single-byte encoding, namely ISO-8859-1. One could
# argue for using binary I/O, but that would require switching to the "byte"
# versions of all the string manipulation APIs, which is distracting, so we
# won't do it now.
BINARY_ENCODING = 'iso-8859-1'


def preprocess(tu: TranslationUnitInfo,
               out_fname: str,
               custom_input_filename=None):
    input_filename = (custom_input_filename if custom_input_filename is not None
                      else tu.input_filename)
    subprocess.check_call([tu.compiler_path, '-E', '-o', out_fname] +
                          tu.compiler_args + [input_filename],
                          cwd=tu.target_directory)


def expandMacros(opts: ExpandMacrosOptions, compilation_base_dir: str,
                 translation_units: List[TranslationUnitInfo]):
    if not opts.enable:
        return

    # If this somehow happens (e.g., it happened in one build configuration of
    # thttpd), fail up front rather than producing mysterious verification
    # failures later.
    tu_output_realpaths = set()
    for tu in translation_units:
        assert tu.output_realpath not in tu_output_realpaths, (
            f'Multiple compilation database entries with output file '
            f'{tu.output_realpath}: not supported by expand_macros')
        tu_output_realpaths.add(tu.output_realpath)

    compilation_base_dir = realpath_cached(compilation_base_dir)

    for tu in translation_units:
        # Since there may be multiple compilation database entries / translation
        # units for the same input file (presumably with different compiler
        # options), translation units must always be identified by their output
        # files (which must be distinct, otherwise it doesn't make sense).
        logging.info(
            f'Saving original preprocessor output for {tu.output_realpath}...')
        preprocess(tu, f'{tu.output_realpath}.pre-em.i')

    # The keys are canonical (os.path.realpath) source file paths.
    source_files: Dict[str, SourceFileInfo] = (
        collections.defaultdict(lambda: SourceFileInfo()))

    # Map the preprocessed output of all translation units back to source lines.
    for tu in translation_units:
        logging.info(f'Scanning customized preprocessor output '
                     f'for {tu.output_realpath}...')
        # Construct a custom source file that we can use to preprocess this
        # translation unit with the given ExpandMacrosOptions.
        em_c_fname = tu.output_realpath + '.em.c'
        with open(em_c_fname, 'w', encoding=BINARY_ENCODING) as em_c_f:
            for inc in opts.includes_before_undefs:
                em_c_f.write(f'#include {inc}\n')
            for macro in opts.undef_macros:
                em_c_f.write(f'#undef {macro}\n')
            em_c_f.write(f'#include "{tu.input_realpath}"\n')
        # Now preprocess it.
        em_i_fname = tu.output_realpath + '.em.i'
        preprocess(tu, em_i_fname, em_c_fname)

        with open(em_i_fname, 'r', encoding=BINARY_ENCODING) as em_i_f:
            src_fname, src_lineno = None, None
            for i_lineno0, i_line_content in enumerate(
                    l.rstrip('\n') for l in em_i_f.readlines()):
                i_lineno = i_lineno0 + 1
                m = re.search(r'^# (\d+) "(.*)"[^"]*', i_line_content)
                if m is not None:
                    # XXX Unescape the filename?
                    src_fname = tu.realpath(m.group(2))
                    src_lineno = int(m.group(1))
                else:
                    i_loc = FileLinePos(em_i_fname, i_lineno)
                    assert src_lineno is not None, (
                        f'{i_loc}: source line without preceding line marker')
                    src_line_info = source_files[src_fname].lines[src_lineno]
                    if i_line_content == '':
                        src_line_info.saw_blank_expansion = True
                    else:
                        src_line_info.expansion_occurrences[
                            i_line_content].append(i_loc)
                    src_lineno += 1

    # Update source files.
    for src_fname, src_finfo in source_files.items():
        if not src_fname.startswith(compilation_base_dir + '/'):
            logging.info(f'Not updating source file {src_fname} '
                         f'because it is outside the base directory.')
            continue
        if src_fname.endswith('.em.c'):
            # The preprocessor is giving us blank lines in these files. Just
            # ignore them.
            continue
        logging.info(f'Updating source file {src_fname}...')
        src_fname_new = src_fname + '.new'
        # There doesn't seem to be a better way to line-break a multi-`with`
        # statement than backslash line continuation. :/
        with open(src_fname, 'r', encoding=BINARY_ENCODING) as src_f, \
             open(src_fname_new, 'w', encoding=BINARY_ENCODING) as src_f_new:
            in_preprocessor_directive = False
            for src_lineno0, src_line_content in enumerate(
                    l.rstrip('\n') for l in src_f.readlines()):
                src_lineno = src_lineno0 + 1
                src_loc = FileLinePos(src_fname, src_lineno)
                if (not in_preprocessor_directive and
                        re.search(r'^\s*#', src_line_content)):
                    in_preprocessor_directive = True
                src_line_info = src_finfo.lines[src_lineno]

                # List of (non-blank expanded content, list of locations where
                # it was seen). So expansion_contents_and_locs[0] is the pair
                # for the first distinct expanded content (in arbitrary order),
                # expansion_contents_and_locs[0][0] is the content,
                # expansion_contents_and_locs[0][1] is the list of locations,
                # etc.
                expansion_contents_and_locs = list(
                    src_line_info.expansion_occurrences.items())
                if in_preprocessor_directive:
                    if len(expansion_contents_and_locs) > 0:
                        logging.warning(
                            f'{src_loc}: in a preprocessor directive '
                            f'but had a non-blank expansion: '
                            f'{repr(expansion_contents_and_locs[0][0])} '
                            f'at {expansion_contents_and_locs[0][1][0]}')
                    # Pass the directive through, but not the portion of any
                    # comment that starts later on the line. libtiff has code
                    # like this:
                    #
                    # #define FOO 1   /* first line of comment
                    #                    second line of comment */
                    #
                    # The preprocessor will blank the whole thing. If we kept
                    # the entire #define line but let the second line get
                    # blanked as usual, we'd end up with an unterminated
                    # comment.
                    #
                    # HOWEVER, before we do this, try to remove /*...*/ comments
                    # within the directive itself. In this case (also seen in
                    # libtiff):
                    #
                    # #define FOO /* blah */ one_line_of_code; \
                    #                        another_line_of_code;
                    #
                    # if we didn't remove /* blah */ first, then we would remove
                    # everything starting from the /* through the line
                    # continuation backslash, which would break things badly.
                    #
                    # XXX: This will break if // or /* appears inside a string
                    # literal and maybe some other obscure cases. Rely on the
                    # final verification to catch these.
                    directive_content = re.sub('/\*.*?\*/', '',
                                               src_line_content)
                    directive_content = re.sub('/[/*].*', '', directive_content)
                    src_f_new.write(directive_content + '\n')
                    # Preprocessor directives can be continued onto the next
                    # line with a backslash.
                    #
                    # XXX: This doesn't interact properly with the above comment
                    # stripping. Again, if this case comes up, the verification
                    # will catch it.
                    if not re.search(r'\\\s*$', src_line_content):
                        in_preprocessor_directive = False
                elif len(expansion_contents_and_locs) == 0:
                    if src_line_info.saw_blank_expansion:
                        src_f_new.write('\n')
                    else:
                        # This line number was never reached in the preprocessed
                        # output. However, we've seen examples where a
                        # preprocessed line was logically blank but the
                        # preprocessor skipped it by emitting a line marker with
                        # a later line number rather than passing the blank line
                        # through. So guess that the line is supposed to be
                        # blank.
                        src_f_new.write('\n')
                elif len(expansion_contents_and_locs) == 1:
                    # If we also saw a blank expansion, assume that was an `#if`
                    # branch not taken and keep the non-blank expansion. If we
                    # are wrong, we'll catch it in the final verification.
                    src_f_new.write(expansion_contents_and_locs[0][0] + '\n')
                else:
                    # This line was expanded differently at different inclusion
                    # sites. Try keeping the original content. This might be
                    # wrong if it was part of a macro call that spanned several
                    # lines; if so, we'll catch that in the final verification.
                    logging.warning(
                        f'{src_loc}: had several different expansions, '
                        f'including {repr(expansion_contents_and_locs[0][0])} '
                        f'at {expansion_contents_and_locs[0][1][0]} '
                        f'and {repr(expansion_contents_and_locs[1][0])} '
                        f'at {expansion_contents_and_locs[1][1][0]}; '
                        f'keeping original content')
                    src_f_new.write(src_line_content + '\n')
        os.rename(src_fname, src_fname + '.pre-em')
        os.rename(src_fname_new, src_fname)

    # Check that we didn't change the preprocessed output.
    verification_ok = True
    for tu in translation_units:
        logging.info(
            f'Verifying preprocessor output for {tu.output_realpath}...')
        preprocess(tu, f'{tu.output_realpath}.post-em.i')
        # `diff` exits nonzero if there is a difference.
        returncode = subprocess.call([
            'diff', '-u', f'{tu.output_realpath}.pre-em.i',
            f'{tu.output_realpath}.post-em.i'
        ])
        if returncode != 0:
            verification_ok = False
    assert verification_ok, (
        'Verification of preprocessed output failed: see diffs above.')
