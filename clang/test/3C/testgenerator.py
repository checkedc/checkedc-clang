#!/usr/bin/env python3
# Author: Shilpa Roy
# Last updated: June 16, 2020

import itertools as it
import os
import subprocess
import textwrap
import re

import find_bin
bin_path = find_bin.bin_path

# Change to the subdirectory after calling `find_bin` (which assumes a working
# directory of `clang/test/3C`) but before doing anything else.
subdir_name = "generated_tests"
# Since testgenerator deletes tests under some conditions and Git may delete
# empty directories, theoretically the directory might not exist and we might
# need to create it here.
if not os.path.isdir(subdir_name):
    os.mkdir(subdir_name)
os.chdir(subdir_name)

prefixes = [
    "arr", "arrstruct", "arrinstruct", "arrofstruct", "safefptrarg",
    "unsafefptrarg", "fptrsafe", "fptrunsafe", "fptrarr", "fptrarrstruct",
    "fptrinstruct", "fptrarrinstruct", "ptrTOptr"
]  #, "safefptrs", "unsafefptrs", "arrOFfptr"]
addendums = ["", "proto", "multi"]

# casts are a whole different ballgame so leaving out for now,
# but they can always be added in later by adding them to the cartesian product
# casts = ["", "expcastunsafe", "expcastsafe", "impcast"]

suffixes = ["safe", "callee", "caller", "both"]

# generate testnames by taking the cartesian product of the above
testnames = []
for e in it.product(prefixes, addendums, suffixes):
    testnames.append([e[0], e[1], e[2]])

### FILE GENERATION ###

# A typical program:
#####################################################################
#   header (llvm-lit run command, #defines, stdlib checked protos)
#
#   definitions (struct definitions, function prototypes)
#   CHECK annotation for definitions
#
#   f1 (foo, bar, sus)
#   CHECK annotation for f1
#
#   f2 (foo, bar, sus) - (f1)
#   CHECK annotation for f2
#
#   f3 (foo, bar, sus) - (f1, f2)
#   CHECK annotation for f3
#####################################################################

# header that should top every file
header = """\
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
"""

# miscallaneous struct definitions that may or may not be used by the files
# above
definitions = """
struct general {
  int data;
  struct general *next;
};

struct warr {
  int data1[5];
  char *name;
};

struct fptrarr {
  int *values;
  char *name;
  int (*mapper)(int);
};

struct fptr {
  int *value;
  int (*func)(int);
};

struct arrfptr {
  int args[5];
  int (*funcs[5])(int);
};

int add1(int x) {
  return x + 1;
}

int sub1(int x) {
  return x - 1;
}

int fact(int n) {
  if (n == 0) {
    return 1;
  }
  return n * fact(n - 1);
}

int fib(int n) {
  if (n == 0) {
    return 0;
  }
  if (n == 1) {
    return 1;
  }
  return fib(n - 1) + fib(n - 2);
}

int zerohuh(int n) {
  return !n;
}

int *mul2(int *x) {
  *x *= 2;
  return x;
}
"""

definitions2 = """
struct general {
  int data;
  struct general *next;
};

struct warr {
  int data1[5];
  char *name;
};

struct fptrarr {
  int *values;
  char *name;
  int (*mapper)(int);
};

struct fptr {
  int *value;
  int (*func)(int);
};

struct arrfptr {
  int args[5];
  int (*funcs[5])(int);
};

static int add1(int x) {
  return x + 1;
}

static int sub1(int x) {
  return x - 1;
}

static int fact(int n) {
  if (n == 0) {
    return 1;
  }
  return n * fact(n - 1);
}

static int fib(int n) {
  if (n == 0) {
    return 0;
  }
  if (n == 1) {
    return 1;
  }
  return fib(n - 1) + fib(n - 2);
}

static int zerohuh(int n) {
  return !n;
}

static int *mul2(int *x) {
  *x *= 2;
  return x;
}
"""


def dedent_to_2(text):
    return textwrap.indent(textwrap.dedent(text), "  ")


# this function will generate a C file that contains
# the core of the example (before the addition of checked annotations)
def method_gen(prefix, proto, suffix):
    return_type = arg_type = susbody = foobody = barbody = ""
    foo = bar = sus = susproto = ""

    # main processing to distinguish between the different types of test we wish
    # to create
    if prefix == "arr":
        return_type = "int *"
        arg_type = "int *"
        susbody = dedent_to_2("""
        int *z = calloc(5, sizeof(int));
        int i, fac;
        int *p;
        for (i = 0, p = z, fac = 1; i < 5; ++i, p++, fac *= i) {
          *p = fac;
        }""")
    elif prefix == "arrstruct":
        return_type = "int *"
        arg_type = "struct general *"
        barbody = foobody = dedent_to_2("""
        struct general *curr = y;
        int i;
        for (i = 1; i < 5; i++, curr = curr->next) {
          curr->data = i;
          curr->next = malloc(sizeof(struct general));
          curr->next->data = i + 1;
        }
        """)
        susbody = dedent_to_2("""
        int *z = calloc(5, sizeof(int));
        struct general *p = y;
        int i;
        for (i = 0; i < 5; p = p->next, i++) {
          z[i] = p->data;
        }
        """)
    elif prefix == "arrinstruct":
        return_type = "struct warr *"
        arg_type = "struct warr *"
        susbody = dedent_to_2("""
        char name[20];
        struct warr *z = y;
        int i;
        for (i = 0; i < 5; i++) {
          z->data1[i] = i;
        }
        """)
    elif prefix == "arrofstruct":
        return_type = "struct general **"
        arg_type = "struct general *"
        susbody = dedent_to_2("""
        struct general **z = calloc(5, sizeof(struct general *));
        struct general *curr = y;
        int i;
        for (i = 0; i < 5; i++) {
          z[i] = curr;
          curr = curr->next;
        }
        """)
        barbody = foobody = dedent_to_2("""
        struct general *curr = y;
        int i;
        for (i = 1; i < 5; i++, curr = curr->next) {
          curr->data = i;
          curr->next = malloc(sizeof(struct general));
          curr->next->data = i + 1;
        }
        """)
    elif prefix == "safefptrarg":
        sus = "\nint *sus(int (*x)(int), int (*y)(int)) {\n"
        susproto = "\nint *sus(int (*)(int), int (*)(int));\n"
        foo = "\nint *foo() {\n"
        bar = "\nint *bar() {\n"
        susbody = dedent_to_2("""
        x = (int (*)(int))5;
        int *z = calloc(5, sizeof(int));
        int i;
        for (i = 0; i < 5; i++) {
          z[i] = y(i);
        }
        """)
        foobody = barbody = dedent_to_2("""
        int (*x)(int) = add1;
        int (*y)(int) = sub1;
        int *z = sus(x, y);
        """)
    elif prefix == "unsafefptrarg":
        sus = "\nint *sus(int (*x)(int), int (*y)(int)) {\n"
        susproto = "\nint *sus(int (*)(int), int (*)(int));\n"
        foo = "\nint *foo() {\n"
        bar = "\nint *bar() {\n"
        susbody = dedent_to_2("""
        x = (int (*)(int))5;
        int *z = calloc(5, sizeof(int));
        int i;
        for (i = 0; i < 5; i++) {
          z[i] = y(i);
        }
        """)
        foobody = barbody = dedent_to_2("""
        int (*x)(int) = add1;
        int (*y)(int) = mul2;
        int *z = sus(x, y);
        """)
    elif prefix == "safefptrs":
        susproto = "\nint * (*sus(int (*)(int), int (*)(int)))(int *);\n"
        sus = "\nint *(*sus(int (*x)(int), int (*y)(int)))(int *) {\n"
        foo = "\nint *(*foo(void))(int *) {\n"
        bar = "\nint *(*bar(void))(int *) {\n"
        susbody = dedent_to_2("""
        x = (int (*)(int))5;
        int *(*z)(int *) = mul2;
        """)
        foobody = barbody = dedent_to_2("""
        int (*x)(int) = add1;
        int (*y)(int) = sub1;
        int *(*z)(int *) = sus(x, y);
        """)
    elif prefix == "unsafefptrs":
        susproto = "\nchar *(*sus(int (*)(int), int (*)(int)))(int *);\n"
        sus = "\nchar *(*sus(int (*x)(int), int (*y)(int)))(int *) {\n"
        foo = "\nchar *(*foo(void))(int *) {\n"
        bar = "\nchar *(*bar(void))(int *) {\n"
        susbody = dedent_to_2("""
        x = (int (*)(int))5;
        char *(*z)(int *) = fib;
        """)
        foobody = barbody = dedent_to_2("""
        int (*x)(int) = add1;
        int (*y)(int) = sub1;
        int *(*z)(int *) = sus(x, y);
        """)
    elif prefix == "fptrsafe":
        sus = "\nint *sus(struct general *x, struct general *y) {\n"
        susproto = "\nint *sus(struct general *, struct general *);\n"
        foo = "\nint *foo() {\n"
        bar = "\nint *bar() {\n"
        barbody = foobody = dedent_to_2("""
        struct general *x = malloc(sizeof(struct general));
        struct general *y = malloc(sizeof(struct general));
        struct general *curr = y;
        int i;
        for (i = 1; i < 5; i++, curr = curr->next) {
          curr->data = i;
          curr->next = malloc(sizeof(struct general));
          curr->next->data = i + 1;
        }
        int *(*sus_ptr)(struct general *, struct general *) = sus;
        int *z = sus_ptr(x, y);
        """)
        susbody = dedent_to_2("""
        x = (struct general *)5;
        int *z = calloc(5, sizeof(int));
        struct general *p = y;
        int i;
        for (i = 0; i < 5; p = p->next, i++) {
          z[i] = p->data;
        }
        """)
    elif prefix == "fptrunsafe":
        sus = "\nint *sus(struct general *x, struct general *y) {\n"
        susproto = "\nint *sus(struct general *, struct general *);\n"
        foo = "\nint *foo() {\n"
        bar = "\nint *bar() {\n"
        barbody = foobody = dedent_to_2("""
        struct general *x = malloc(sizeof(struct general));
        struct general *y = malloc(sizeof(struct general));
        struct general *curr = y;
        int i;
        for (i = 1; i < 5; i++, curr = curr->next) {
          curr->data = i;
          curr->next = malloc(sizeof(struct general));
          curr->next->data = i + 1;
        }
        int (*sus_ptr)(struct fptr *, struct fptr *) = sus;
        int *z = (int *)sus_ptr(x, y);
        """)
        susbody = dedent_to_2("""
        x = (struct general *)5;
        int *z = calloc(5, sizeof(int));
        struct general *p = y;
        int i;
        for (i = 0; i < 5; p = p->next, i++) {
          z[i] = p->data;
        }
        """)
    elif prefix == "fptrarr":
        sus = "\nint **sus(int *x, int *y) {\n"
        susproto = "\nint **sus(int *, int *);\n"
        foo = "\nint **foo() {\n"
        bar = "\nint **bar() {\n"
        susbody = dedent_to_2("""
        x = (int *)5;
        int **z = calloc(5, sizeof(int *));
        int *(*mul2ptr)(int *) = mul2;
        int i;
        for (i = 0; i < 5; i++) {
          z[i] = mul2ptr(&y[i]);
        }
        """)
        foobody = barbody = dedent_to_2("""
        int *x = malloc(sizeof(int));
        int *y = calloc(5, sizeof(int));
        int i;
        for (i = 0; i < 5; i++) {
          y[i] = i + 1;
        }
        int **z = sus(x, y);
        """)
    elif prefix == "arrOFfptr":
        sus = "\nint (**sus(int *x, int *y))(int) { \n"
        susproto = "\nint (**sus(int *x, int *y))(int);\n"
        foo = "\nint (**foo(void)) (int) {"
        bar = "\nint (**bar(void)) (int) {"

        foobody = barbody = dedent_to_2("""
        int *x = malloc(sizeof(int));
        int *y = calloc(5, sizeof(int));
        int i;
        for (i = 0; i < 5; i++) {
          y[i] = i + 1;
        }
        int (**z)(int) = sus(x, y);
        """)

        susbody = dedent_to_2("""
        x = (int *)5;
        int (**z)(int) = calloc(5, sizeof(int (*)(int)));
        z[0] = add1;
        z[1] = sub1;
        z[2] = zerohuh;
        z[3] = fib;
        z[4] = fact;
        int i;
        for (i = 0; i < 5; i++) {
          y[i] = z[i](y[i]);
        }
        """)
    elif prefix == "fptrinstruct":
        sus = "\nstruct fptr *sus(struct fptr *x, struct fptr *y) {\n"
        susproto = "\nstruct fptr *sus(struct fptr *, struct fptr *);\n"
        foo = "\nstruct fptr *foo() {\n"
        bar = "\nstruct fptr *bar() {\n"
        susbody = dedent_to_2("""
        x = (struct fptr *)5;
        struct fptr *z = malloc(sizeof(struct fptr));
        z->value = y->value;
        z->func = fact;
        """)
        foobody = barbody = dedent_to_2("""
        struct fptr *x = malloc(sizeof(struct fptr));
        struct fptr *y = malloc(sizeof(struct fptr));
        struct fptr *z = sus(x, y);
        """)
    elif prefix == "fptrarrstruct":
        sus = "\nstruct fptrarr *sus(struct fptrarr *x, struct fptrarr *y) {\n"
        susproto = (
            "\nstruct fptrarr *sus(struct fptrarr *, struct fptrarr *);\n")
        foo = "\nstruct fptrarr *foo() {\n"
        bar = "\nstruct fptrarr *bar() {\n"
        susbody = dedent_to_2("""
        x = (struct fptrarr *)5;
        char name[30];
        struct fptrarr *z = malloc(sizeof(struct fptrarr));
        z->values = y->values;
        z->name = strcpy(name, "Hello World");
        z->mapper = fact;
        int i;
        for (i = 0; i < 5; i++) {
          z->values[i] = z->mapper(z->values[i]);
        }
        """)
        foobody = barbody = dedent_to_2("""
        char name[20];
        struct fptrarr *x = malloc(sizeof(struct fptrarr));
        struct fptrarr *y = malloc(sizeof(struct fptrarr));
        int *yvals = calloc(5, sizeof(int));
        int i;
        for (i = 0; i < 5; i++) {
          yvals[i] = i + 1;
        }
        y->values = yvals;
        y->name = name;
        y->mapper = NULL;
        strcpy(y->name, "Example");
        struct fptrarr *z = sus(x, y);
        """)
    elif prefix == "fptrarrinstruct":
        sus = "\nstruct arrfptr *sus(struct arrfptr *x, struct arrfptr *y) {\n"
        susproto = (
            "\nstruct arrfptr *sus(struct arrfptr *, struct arrfptr *);\n")
        foo = "\nstruct arrfptr *foo() {\n"
        bar = "\nstruct arrfptr *bar() {\n"
        susbody = dedent_to_2("""
        x = (struct arrfptr *)5;
        struct arrfptr *z = malloc(sizeof(struct arrfptr));
        int i;
        for (i = 0; i < 5; i++) {
          z->args[i] = i + 1;
        }
        z->funcs[0] = add1;
        z->funcs[1] = sub1;
        z->funcs[2] = zerohuh;
        z->funcs[3] = fib;
        z->funcs[4] = fact;
        """)
        foobody = barbody = dedent_to_2("""
        struct arrfptr *x = malloc(sizeof(struct arrfptr));
        struct arrfptr *y = malloc(sizeof(struct arrfptr));

        struct arrfptr *z = sus(x, y);
        int i;
        for (i = 0; i < 5; i++) {
          z->args[i] = z->funcs[i](z->args[i]);
        }
        """)
    elif prefix == "ptrTOptr":
        return_type = "char ***"
        arg_type = "char ***"
        susbody = dedent_to_2("""
        char *ch = malloc(sizeof(char));
        *ch = 'A'; /*Capital A*/
        char ***z = malloc(5 * sizeof(char **));
        for (int i = 0; i < 5; i++) {
          z[i] = malloc(5 * sizeof(char *));
          for (int j = 0; j < 5; j++) {
            z[i][j] = malloc(2 * sizeof(char));
            strcpy(z[i][j], ch);
            *ch = *ch + 1;
          }
        }
        """)

    # generate standard enders and duplications that occur in all generated
    # tests

    if not "fptr" in prefix:
        barbody += f"  {return_type}z = sus(x, y);"
        foobody += f"  {return_type}z = sus(x, y);"
        susproto = f"\n{return_type}sus({arg_type}, {arg_type});\n"
        sus = textwrap.dedent(f"""
        {return_type}sus({arg_type}x, {arg_type}y) {{
          x = ({arg_type})5;""")
        arg_np = arg_type[:-1]
        if arg_np.endswith(" "):
            arg_np = arg_np[:-1]
        foo = textwrap.dedent(f"""
        {return_type}foo() {{
          {arg_type}x = malloc(sizeof({arg_np}));
          {arg_type}y = malloc(sizeof({arg_np}));
        """)
        bar = textwrap.dedent(f"""
        {return_type}bar() {{
          {arg_type}x = malloc(sizeof({arg_np}));
          {arg_type}y = malloc(sizeof({arg_np}));
        """)

    # create unsafe use cases based on the suffix (by default, the generated
    # code is safe)

    if suffix == "both":
        susbody += "\n  z += 2;"
        barbody += "\n  z += 2;"
    elif suffix == "callee":
        susbody += "\n  z += 2;"
    elif suffix == "caller":
        barbody += "\n  z += 2;"

    susbody += "\n  return z;\n}\n"
    foobody += "\n  return z;\n}\n"
    barbody += "\n  return z;\n}\n"

    return [susproto, sus + susbody, foo + foobody, bar + barbody]


def process_file_smart(prefix, proto, suffix, name, cnameNOALL, cnameALL, name2,
                       cname2NOALL, cname2ALL):

    # generate a descriptive comment that describes what the test will do:
    comm_general = ("/*This file tests three functions: "
                    "two callers bar and foo, and a callee sus*/\n")

    comm_prefix = "/*In particular, this file tests: "
    if prefix == "arr":
        comm_prefix += ("arrays through a for loop and pointer\n"
                        "  arithmetic to assign into it*/")
    if prefix == "arrstruct":
        comm_prefix += ("arrays and structs, specifically by using an\n"
                        "  array to traverse through the values of a struct*/")
    if prefix == "arrinstruct":
        comm_prefix += ("how the tool behaves when there is an array\n"
                        "  field within a struct*/")
    if prefix == "arrofstruct":
        comm_prefix += ("how the tool behaves when there is an array\n"
                        "  of structs*/")
    if prefix == "safefptrarg":
        comm_prefix += ("passing a function pointer as an argument to a\n"
                        "  function safely (without unsafe casting)*/")
    if prefix == "unsafefptrarg":
        comm_prefix += ("passing a function pointer as an argument to a\n"
                        "  function unsafely (by casting it unsafely)*/")
    if prefix == "safefptrs":
        comm_prefix += ("passing function pointers in as arguments and\n"
                        "  returning a function pointer safely*/")
    if prefix == "arrOFfptr":
        comm_prefix += ("how the tool behaves when returning an array\n"
                        "  of function pointers*/")
    if prefix == "unsafefptrs":
        comm_prefix += ("passing fptrs in as arguments and returning a\n"
                        "  fptr unsafely (through unsafe casting*/")
    if prefix == "fptrsafe":
        comm_prefix += ("converting the callee into a function pointer\n"
                        "  and then using that pointer for computations*/")
    if prefix == "fptrunsafe":
        comm_prefix += (
            "converting the callee into a function pointer\n"
            "  unsafely via cast and using that pointer for computations*/")
    if prefix == "fptrarr":
        comm_prefix += ("using a function pointer and an array in\n"
                        "  tandem to do computations*/")
    if prefix == "fptrarrstruct":
        comm_prefix += ("using a function pointer and an array as\n"
                        "  fields of a struct that interact with each other*/")
    if prefix == "fptrinstruct":
        comm_prefix += ("how the tool behaves when a function pointer\n"
                        "  is a field of a struct*/")
    if prefix == "fptrarrinstruct":
        comm_prefix += ("how the tool behaves when there is an array\n"
                        "  of function pointers in a struct*/")
    if prefix == "ptrTOptr":
        comm_prefix += "having a pointer to a pointer*/"

    comm_proto = ""
    if proto == "multi":
        comm_proto = f"""
/*For robustness, this test is identical to
{prefix}proto{suffix}.c and {prefix}{suffix}.c except in that
the callee and callers are split amongst two files to see how
the tool performs conversions*/"""
    elif proto == "proto":
        comm_proto = f"""
/*For robustness, this test is identical to
{prefix}{suffix}.c except in that
a prototype for sus is available, and is called by foo and bar,
while the definition for sus appears below them*/"""
    comm_suffix = ""
    if suffix == "safe":
        comm_suffix = """
/*In this test, foo, bar, and sus will all treat their return values safely*/"""
    elif suffix == "callee":
        comm_suffix = """
/*In this test, foo and bar will treat their return values safely, but sus will
  not, through invalid pointer arithmetic, an unsafe cast, etc*/"""
    elif suffix == "caller":
        comm_suffix = """
/*In this test, foo and sus will treat their return values safely, but bar will
  not, through invalid pointer arithmetic, an unsafe cast, etc.*/"""
    elif suffix == "both":
        comm_suffix = """
/*In this test, foo will treat its return value safely, but sus and bar will
  not, through invalid pointer arithmetic, an unsafe cast, etc.*/"""
    comm_dec = """

/******************************************************************************/

"""

    comment = ''.join([
        comm_dec, comm_general, comm_prefix, comm_proto, comm_suffix, comm_dec
    ])

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
    os.system("rm {} {}".format(cnameNOALL, cnameALL))

    # Prior to the introduction of lower bound inference an automatic lower
    # bounds generation, the initial and converted code should have had the
    # same number of lines. Generated lower bounds introduce new lines,
    # so this is nolonger the case.
    # assert len(lines) == len(noall) == len(yeall), "fix file " + name

    if proto == "multi":
        file2 = open(name2, "r")
        noallfile2 = open(cname2NOALL, "r")
        allfile2 = open(cname2ALL, "r")

        # gather all the lines
        lines2 = str(file2.read()).split("\n")
        noall2 = str(noallfile2.read()).split("\n")
        yeall2 = str(allfile2.read()).split("\n")
        file2.close()
        noallfile2.close()
        allfile2.close()
        os.system("rm {} {}".format(cname2NOALL, cname2ALL))

        # See earlier comment on why this is disable.
        # assert len(lines2) == len(noall2) == len(yeall2), "fix file " + name

    def runtime_cname(s):
        assert s.startswith("tmp.")
        return "%t." + s[len("tmp."):]

    cnameNOALL = runtime_cname(cnameNOALL)
    cnameALL = runtime_cname(cnameALL)
    cname2NOALL = runtime_cname(cname2NOALL)
    cname2ALL = runtime_cname(cname2ALL)

    # our keywords that indicate we should add an annotation
    keywords = "int char struct double float".split(" ")
    ckeywords = "_Ptr _Array_ptr _Nt_array_ptr _Checked _Unchecked".split(" ")
    keywords_re = re.compile("\\b(" + "|".join(keywords) + ")\\b")
    ckeywords_re = re.compile("\\b(" + "|".join(ckeywords) + ")\\b")

    in_extern = False
    ye_offset = 0
    for i in range(0, len(lines)):
        line = lines[i]
        noline = noall[i]
        yeline = yeall[i + ye_offset]
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
                lines[i] += ("\n" + indentation + "//CHECK_NOALL: " + noline.lstrip())
                lines[i] += ("\n" + indentation + "//CHECK_ALL: " + yeline.lstrip())

            # This is a hack needed to properly updated tests where an array
            # variable declaration has been duplicated to allow for generating
            # fresh lower bound.
            if i + ye_offset + 1 < len(yeall):
                yeline_next = yeall[i + ye_offset + 1]
                if "= __3c_" in yeline_next and "> __3c_" in yeline:
                    lines[i] += ("\n" + indentation + "//CHECK_ALL: " + yeline_next.lstrip())
                    ye_offset += 1

        if ";" in line:
            in_extern = False

    if proto == "multi":
        in_extern = False
        ye_offset = 0
        for i in range(0, len(lines2)):
            line = lines2[i]
            noline = noall2[i]
            yeline = yeall2[i + ye_offset]
            if "extern" in line:
                in_extern = True
            if (not in_extern and
                ((keywords_re.search(line) and line.find("*") != -1) or
                 ckeywords_re.search(noline) or ckeywords_re.search(yeline))):
                indentation = re.search("^\s*", noline).group(0)
                if noline.endswith("{"):
                    indentation += "  "
                if noline == yeline:
                    lines2[i] += ("\n" + indentation + "//CHECK: " +
                                  noline.lstrip())
                else:
                    lines2[i] += ("\n" + indentation + "//CHECK_NOALL: " +
                                  noline.lstrip())
                    lines2[i] += ("\n" + indentation + "//CHECK_ALL: " +
                                  yeline.lstrip())

            # See above comment for why this hack is necessary.
            if i + ye_offset + 1 < len(yeall2):
                yeline_next = yeall2[i + ye_offset + 1]
                if "= __3c_" in yeline_next and "> __3c_" in yeline:
                    lines2[i] += ("\n" + indentation + "//CHECK_ALL: " + yeline_next.lstrip())
                    ye_offset += 1

            if ";" in line:
                in_extern = False

    run = f"""\
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/{name} -- | diff %t.checked/{name} -\
"""
    if proto == "multi":
        cname = "%t.checked/" + name
        cname2 = "%t.checked/" + name2
        cnameALLtwice1 = "%t.convert_again/" + name
        cnameALLtwice2 = "%t.convert_again/" + name2
        run = f"""\
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes -output-dir=%t.checkedALL %s %S/{name2} --
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checkedNOALL %s %S/{name2} --
// RUN: %clang -working-directory=%t.checkedNOALL -c {name} {name2}
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" --input-file {cnameNOALL} %s
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" --input-file {cnameALL} %s
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %S/{name2} %s --
// RUN: 3c -base-dir=%t.checked -alltypes -output-dir=%t.convert_again {cname} {cname2} --
// RUN: test ! -f {cnameALLtwice1}
// RUN: test ! -f {cnameALLtwice2}\
"""
        cnameNOALL2 = "%t.checkedNOALL2/" + name
        cnameALL2 = "%t.checkedALL2/" + name
        cname2NOALL2 = "%t.checkedNOALL2/" + name2
        cname2ALL2 = "%t.checkedALL2/" + name2
        # uncomment the following lines if we ever decide we want to generate
        # buggy tests that don't compile
        # if bug_generated:
        #     cname21 = prefix + suffix + proto + "1_BUG.checked2.c"
        #     cname22 = prefix + suffix + proto + "2_BUG.checked2.c"
        run2 = f"""\
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes -output-dir=%t.checkedALL2 %S/{name} %s --
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checkedNOALL2 %S/{name} %s --
// RUN: %clang -working-directory=%t.checkedNOALL2 -c {name} {name2}
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" --input-file {cname2NOALL2} %s
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" --input-file {cname2ALL2} %s
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %S/{name} %s --
// RUN: 3c -base-dir=%t.checked -alltypes -output-dir=%t.convert_again {cname} {cname2} --
// RUN: test ! -f {cnameALLtwice1}
// RUN: test ! -f {cnameALLtwice2}\
"""

    file = open(name, "w+")
    file.write(run + comment + "\n".join(lines))
    file.close()

    if proto == "multi":
        file = open(name2, "w+")
        file.write(run2 + comment + "\n".join(lines2))
        file.close()
    return


def annot_gen_smart(prefix, proto, suffix):

    # generate the body of the file
    [susproto, sus, foo, bar] = method_gen(prefix, proto, suffix)

    name = prefix + proto + suffix + ".c"
    cnameNOALL = "tmp.checkedNOALL/" + name
    cnameALL = "tmp.checkedALL/" + name
    name2 = name
    cname2NOALL = cnameNOALL
    cname2ALL = cnameALL

    if proto == "multi":
        name = prefix + suffix + proto + "1.c"
        name2 = prefix + suffix + proto + "2.c"
        cnameNOALL = "tmp.checkedNOALL/" + name
        cnameALL = "tmp.checkedALL/" + name
        cname2NOALL = "tmp.checkedNOALL/" + name2
        cname2ALL = "tmp.checkedALL/" + name2

    if proto == "proto":
        test = header + definitions + susproto + foo + bar + sus
    elif proto == "multi":
        test = header + definitions2 + susproto + foo + bar
    else:
        test = header + definitions + sus + foo + bar

    # write the main file
    file = open(name, "w+")
    file.write(test)
    file.close()

    # generate the second file if a multi example
    if proto == "multi":
        test2 = header + definitions2 + sus
        file = open(name2, "w+")
        file.write(test2)
        file.close()

    # run the porting tool on the file(s)
    if proto == "multi":
        os.system(
            "{}3c -alltypes -addcr -output-dir=tmp.checkedALL {} {} --".format(
                bin_path, name, name2))
        os.system("{}3c -addcr -output-dir=tmp.checkedNOALL {} {} --".format(
            bin_path, name, name2))
    else:
        os.system(
            "{}3c -alltypes -addcr -output-dir=tmp.checkedALL {} --".format(
                bin_path, name))
        os.system("{}3c -addcr -output-dir=tmp.checkedNOALL {} --".format(
            bin_path, name))

    # compile the files and if it doesn't compile, then let's indicate that a
    # bug was generated for this file
    bug_generated = False
    if proto != "multi":
        # Avoid leaving an object file in the working directory.
        out = subprocess.Popen(
            ['{}clang'.format(bin_path), '-c', '-o', '/dev/null', cnameNOALL],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT)
        stdout, stderr = out.communicate()
        stdout = str(stdout)
        if "error:" in stdout:
            bug_generated = True
            # name = prefix + proto + suffix + "_BUG.c"
    else:
        # In this case, since there are two source files, clang will give an
        # error if we try to specify a single output file with -o.
        # -working-directory seems to be the easiest solution, but we need to
        # make the path absolute (https://bugs.llvm.org/show_bug.cgi?id=24586).
        cwd = os.getcwd()
        out = subprocess.Popen([
            '{}clang'.format(bin_path),
            '-working-directory={}/tmp.checkedNOALL'.format(cwd), '-c', name,
            name2
        ],
                               stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT)
        stdout, stderr = out.communicate()
        stdout = str(stdout)
        if "error:" in stdout:
            bug_generated = True
            # name = prefix + suffix + proto + "1_BUG.c"
            # name2 = prefix + suffix + proto + "2_BUG.c"

    if bug_generated:
        # uncomment the following lines if we ever want to generate buggy tests
        # that do not compile
        # cname = prefix + suffix + proto + "1_BUG.checked.c"
        # cname2 = prefix + suffix + proto + "2_BUG.checked.c"
        os.system("rm {}".format(name))
        if proto == "multi":
            os.system("rm {}".format(name2))
        return

    process_file_smart(prefix, proto, suffix, name, cnameNOALL, cnameALL, name2,
                       cname2NOALL, cname2ALL)

    os.system("rm -r tmp.checkedALL tmp.checkedNOALL")
    return


#arr, arrinstruct, arrofstruct
if __name__ == "__main__":
    for skeleton in testnames:
        annot_gen_smart(skeleton[0], skeleton[1], skeleton[2])
