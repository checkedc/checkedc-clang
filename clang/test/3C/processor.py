import fileinput 
import sys
import os

path_to_monorepo = "/Users/shilpa-roy/checkedc-clang/build/bin/"

structs = """
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
};
"""

# Let's clear all the existing annotations to get a clean fresh file with only code
def strip_existing_annotations(filename): 
    for line in fileinput.input(filename, inplace=1):
        if "//" in line: 
            line = "" 
        sys.stdout.write(line)

# Now split the file into clear processing units
def split_into_blocks(filename): 
    susproto = sus = foo = bar = header = "" 
    file = open(filename, "r")
    insus = infoo = inbar = strcpy = prot_encountered = False 
    for line in file.readlines(): 
        if line.find("sus") != -1 and line.find(";") != -1 and (not (infoo or inbar or insus)):
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
        elif not prot_encountered and "strcpy" in line: 
            header += line 
            strcpy = True
        
        elif not prot_encountered and not strcpy: 
            header += line
        
        if insus: 
            sus += line 
        elif infoo: 
            foo += line 
        elif inbar: 
            bar += line

    return [header.strip(), susproto.strip(), sus.strip(), foo.strip(), bar.strip()]  


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
    os.system("rm {} {}".format(cnameNOALL, cnameALL)) 
    
    # ensure all lines are the same length
    assert len(lines) == len(noall) == len(yeall), "fix file " + name
    # our keywords that indicate we should add an annotation
    keywords = "int char struct double float".split(" ") 
    ckeywords = "_Ptr _Array_ptr _Nt_array_ptr _Checked _Unchecked".split(" ") 

    for i in range(0, len(lines)): 
        line = lines[i] 
        noline = noall[i] 
        yeline = yeall[i]
        if line.find("extern") == -1 and ((any(substr in line for substr in keywords) and line.find("*") != -1) or any(substr in noline for substr in ckeywords) or any(substr in yeline for substr in ckeywords)): 
            if noline == yeline: 
                lines[i] = line + "\n\t//CHECK: " + noline.lstrip()
            else: 
                lines[i] = line + "\n\t//CHECK_NOALL: " + noline.lstrip() + "\n\t//CHECK_ALL: " + yeline
    
    run = "// RUN: 3c -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes=\"CHECK_ALL\",\"CHECK\" %s"
    run += "\n// RUN: 3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes=\"CHECK_NOALL\",\"CHECK\" %s"
    run += "\n// RUN: 3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -" 
    run += "\n// RUN: 3c -output-postfix=checked -alltypes %s"
    run += "\n// RUN: 3c -alltypes %S/{} -- | count 0".format(name + "hecked.c", name + "hecked.c") 
    run += "\n// RUN: rm %S/{}\n".format(name + "hecked.c")

    file = open(name, "w+")
    file.write(run + "\n".join(lines)) 
    file.close()
    return 

def process_smart(filename): 
    strip_existing_annotations(filename) 
    [header, susproto, sus, foo, bar] = split_into_blocks(filename) 

    struct_needed = False 
    if "struct" in susproto or "struct" in sus or "struct" in foo or "struct" in bar: 
        struct_needed = True
    
    cnameNOALL = filename + "heckedNOALL.c" 
    cnameALL = filename + "heckedALL.c"

    test = [header, sus, foo, bar] 
    if susproto != "" and struct_needed: test = [header, structs, susproto, foo, bar, sus] 
    elif struct_needed: test = [header, structs, sus, foo, bar] 
    elif susproto != "": test = [header, susproto, foo, bar, sus]

    file = open(filename, "w+") 
    file.write('\n\n'.join(test)) 
    file.close()

    os.system("{}3c -alltypes -addcr -output-postfix=checkedALL {}".format(path_to_monorepo, filename))
    os.system("{}3c -addcr -output-postfix=checkedNOALL {}".format(path_to_monorepo, filename)) 

    process_file_smart(filename, cnameNOALL, cnameALL) 
    return


b_tests = ['b10_allsafepointerstruct.c',
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
 'b9_allsafestructp.c'] 

for i in b_tests: 
    process_smart(i)