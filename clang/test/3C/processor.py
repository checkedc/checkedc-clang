#!/usr/bin/env python3
import fileinput
import sys
import os
import re

import find_bin
bin_path = find_bin.bin_path

# Change to the subdirectory after calling `find_bin` (which assumes a working
# directory of `clang/test/3C`) but before doing anything else.
os.chdir("b_tests")

structs = """\
struct np {
  int x;
  int y;
};

struct p {
  int *x;
  char *y;
};

struct r {
  int data;
  struct r *next;
};"""


# Let's clear all the existing annotations to get a clean fresh file with only
# code
def strip_existing_annotations(filename):
    for line in fileinput.input(filename, inplace=1):
        if "//" in line:
            line = ""
        sys.stdout.write(line)


# Now split the file into clear processing units
def split_into_blocks(filename):
    susproto = sus = foo = bar = header = ""
    file = open(filename, "r")
    insus = infoo = inbar = prot_encountered = False
    for line in file.readlines():
        if (line.find("sus") != -1 and line.find(";") != -1 and
            (not (infoo or inbar or insus))):
            prot_encountered = True
            susproto = line
        elif line.find("sus") != -1 and line.find("{") != -1:
            prot_encountered = True
            insus = infoo = inbar = False
            insus = True

        # annotate the definition for foo
        elif line.find("foo") != -1:
            prot_encountered = True
            insus = infoo = inbar = False
            infoo = True

        # annotate the definition for bar
        elif line.find("bar") != -1:
            prot_encountered = True
            insus = infoo = inbar = False
            inbar = True

        elif line.find("struct np {") != -1:
            prot_encountered = True

        elif not prot_encountered:
            header += line

        if insus:
            sus += line
        elif infoo:
            foo += line
        elif inbar:
            bar += line

    return [
        header.strip(),
        susproto.strip(),
        sus.strip(),
        foo.strip(),
        bar.strip()
    ]


def process_file_smart(name, cnameNOALL, cnameALL):
    file = open(name, "r")
    noallfile = open(cnameNOALL, "r")
    allfile = open(cnameALL, "r")

    # gather all the lines
    lines = str(file.read()).split("\n")
    noall = str(noallfile.read()).split("\n")
    yeall = str(allfile.read()).split("\n")

    file.close()
    noallfile.close()
    allfile.close()
    os.system("rm -r tmp.checkedALL tmp.checkedNOALL")

    # ensure all lines are the same length
    assert len(lines) == len(noall) == len(yeall), "fix file " + name
    # our keywords that indicate we should add an annotation
    keywords = "int char struct double float".split(" ")
    ckeywords = "_Ptr _Array_ptr _Nt_array_ptr _Checked _Unchecked".split(" ")
    keywords_re = re.compile("\\b(" + "|".join(keywords) + ")\\b")
    ckeywords_re = re.compile("\\b(" + "|".join(ckeywords) + ")\\b")

    in_extern = False
    for i in range(0, len(lines)):
        line = lines[i]
        noline = noall[i]
        yeline = yeall[i]
        if "extern" in line:
            in_extern = True
        if (not in_extern and
            ((keywords_re.search(line) and line.find("*") != -1) or
             ckeywords_re.search(noline) or ckeywords_re.search(yeline))):
            indentation = re.search("^\s*", noline).group(0)
            # Hack to match how clang-format will indent the CHECK comments,
            # even though it would arguably be more logical to leave them at the
            # same indentation as the code they refer to.
            if noline.endswith("{"):
                indentation += "  "
            if noline == yeline:
                lines[i] += "\n" + indentation + "//CHECK: " + noline.lstrip()
            else:
                lines[i] += ("\n" + indentation + "//CHECK_NOALL: " +
                             noline.lstrip())
                lines[i] += ("\n" + indentation + "//CHECK_ALL: " +
                             yeline.lstrip())
        if ";" in line:
            in_extern = False

    run = f"""\
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/{name} -- | diff %t.checked/{name} -
"""

    file = open(name, "w+")
    file.write(run + "\n".join(lines))
    file.close()
    return


def process_smart(filename):
    strip_existing_annotations(filename)
    [header, susproto, sus, foo, bar] = split_into_blocks(filename)

    struct_needed = False
    if ("struct" in susproto or "struct" in sus or "struct" in foo or
            "struct" in bar):
        struct_needed = True

    cnameNOALL = "tmp.checkedNOALL/" + filename
    cnameALL = "tmp.checkedALL/" + filename

    test = [header, sus, foo, bar]
    if susproto != "" and struct_needed:
        test = [header, structs, susproto, foo, bar, sus]
    elif struct_needed:
        test = [header, structs, sus, foo, bar]
    elif susproto != "":
        test = [header, susproto, foo, bar, sus]

    file = open(filename, "w+")
    file.write('\n\n'.join(test) + '\n')
    file.close()

    os.system("{}3c -alltypes -addcr -output-dir=tmp.checkedALL {} --".format(
        bin_path, filename))
    os.system("{}3c -addcr -output-dir=tmp.checkedNOALL {} --".format(
        bin_path, filename))

    process_file_smart(filename, cnameNOALL, cnameALL)
    return


# yapf: disable
b_tests = [
    'b10_allsafepointerstruct.c',
    'b11_calleestructnp.c',
    'b12_callerstructnp.c',
    'b13_calleestructp.c',
    'b14_callerstructp.c',
    'b15_calleepointerstruct.c',
    'b16_callerpointerstruct.c',
    'b17_bothstructnp.c',
    'b18_bothstructp.c',
    'b19_bothpointerstruct.c',
    'b1_allsafe.c',
    'b20_allsafepointerstructproto.c',
    'b21_calleepointerstructproto.c',
    'b22_callerpointerstructproto.c',
    'b23_explicitunsafecast.c',
    'b23_retswitchexplicit.c',
    'b24_implicitunsafecast.c',
    'b24_retswitchimplicit.c',
    'b25_castprotosafe.c',
    'b26_castprotounsafe.c',
    'b26_castprotounsafeimplicit.c',
    'b26_castprotounsafeimplicitretswitch.c',
    'b27_structcastsafe.c',
    'b28_structcastexplicit.c',
    'b28_structcastimplicit.c',
    'b28_structimplicitretcast.c',
    'b29_structprotocastsafe.c',
    'b29_structprotocastsafeuseunsafe.c',
    'b2_calleeunsafe.c',
    'b30_structprotocastexplicitunsafeuseunsafe.c',
    'b30_structprotocastimplicitunsafeuseunsafe.c',
    'b30_structprotocastunsafeexplicit.c',
    'b30_structprotocastunsafeimplicit.c',
    'b30_structprotocastunsafeimplicitretswitch.c',
    'b30_structprotoconflict.c',
    'b30_structprotoconflictbodyconvert.c',
    'b3_onecallerunsafe.c',
    'b4_bothunsafe.c',
    'b5_calleeunsafeproto.c',
    'b6_callerunsafeproto.c',
    'b7_allsafeproto.c',
    'b8_allsafestructnp.c',
    'b9_allsafestructp.c',
]
# yapf: enable

for i in b_tests:
    process_smart(i)
