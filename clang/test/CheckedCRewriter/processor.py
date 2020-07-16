import fileinput 
import sys
import os

path_to_monorepo = "/Users/shilpa-roy/checkedc-clang/build/bin/"

# Let's clear all the existing annotations to get a clean fresh file with only code
def strip_existing_annotations(filename): 
    for line in fileinput.input(filename, inplace=1):
        if "//" in line and ("CHECK" in line or "RUN" in line): 
            line = "" 
        sys.stdout.write(line)

# Now split the file into clear processing units
def split_into_blocks(filename): 
    susproto = sus = foo = bar = header = "" 
    file = open(filename, "r")
    prot_encountered = insus = infoo = inbar = False 
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
        elif not prot_encountered: 
            header += line 
        
        if insus: 
            sus += line 
        elif infoo: 
            foo += line 
        elif inbar: 
            bar += line

    return [header.strip(), susproto.strip(), sus.strip(), foo.strip(), bar.strip()] 

# Add the annotations to the files 
def process_file(file, alltypes, susprotoc, susc, fooc, barc): 
    check = "CHECK_NOALL"
    if alltypes: check = "CHECK_ALL"

    keywords = "int char struct double float".split(" ") 
    ckeywords = "_Ptr _Array_ptr _Nt_array_ptr _Checked _Unchecked".split(" ")

    # these boolean variables indicate which method definition we are in, so we know where to add
    # our checked annotations later
    insus = infoo = inbar = False 

    # generate the check annotations
    for line in file.readlines():   
        linepre = line.split("=")[0]
        
        # annotate the prototype for sus
        if line.find("sus") != -1 and line.find(";") != -1 and (not (infoo or inbar or insus)):
            insus = infoo = inbar = False
            susprotoc += "//" + check + ": " + line

        # annotate the definition for sus
        elif line.find("sus") != -1 and line.find("{") != -1: 
            insus = infoo = inbar = False
            insus = True
            susc += "//" + check + ": " + line

        # annotate the definition for foo
        elif line.find("foo") != -1:
            insus = infoo = inbar = False
            infoo = True 
            fooc += "//" + check + ": " + line  

        # annotate the definition for bar
        elif line.find("bar") != -1:  
            insus = infoo = inbar = False
            inbar = True 
            barc += "//" + check + ": " + line  

        elif insus: 
            if (any(substr in linepre for substr in keywords) and linepre.find("*") != -1) or any(substr in line for substr in ckeywords):
                susc += "//" + check + ": " + line
        elif infoo: 
            if (any(substr in linepre for substr in keywords) and linepre.find("*") != -1) or any(substr in line for substr in ckeywords):
                fooc += "//" + check + ": " + line 
        elif inbar: 
            if (any(substr in linepre for substr in keywords) and linepre.find("*") != -1) or any(substr in line for substr in ckeywords):
                barc += "//" + check + ": " + line
    
    return [susprotoc, susc, fooc, barc]

# main processing unit
def process(filename): 
    strip_existing_annotations(filename) 
    [header, susproto, sus, foo, bar] = split_into_blocks(filename) 
    
    cnameNOALL = filename + "heckedNOALL.c" 
    cnameALL = filename + "heckedALL.c"

    os.system("{}cconv-standalone -alltypes -output-postfix=checkedALL {}".format(path_to_monorepo, filename))
    os.system("{}cconv-standalone -output-postfix=checkedNOALL {}".format(path_to_monorepo, filename))
    os.system("rm {}".format(filename))

    susprotoc = susc = fooc = barc = ""
    file = open(cnameNOALL, "r") 
    [susprotoc, susc, fooc, barc] = process_file(file, False, susprotoc, susc, fooc, barc)
    file.close() 
    os.system("rm {}".format(cnameNOALL))

    file = open(cnameALL, "r") 
    [susprotoc, susc, fooc, barc] = process_file(file, True, susprotoc, susc, fooc, barc)
    file.close() 
    os.system("rm {}".format(cnameALL))

    #TODO: Once Aaron's PR is merged, add the addcr flag here
    run = "// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes=\"CHECK_ALL\" %s"
    run += "\n//RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes=\"CHECK_NOALL\" %s"
    run += "\n//RUN: cconv-standalone -output-postfix=checkedNOALL %s" 
    run += "\n//RUN: %clang -c %S/{}".format(cnameNOALL)
    run += "\n//RUN: rm %S/{}".format(cnameNOALL) 

    ctest = [run, header, sus + "\n" + susc, foo + "\n" + fooc, bar + "\n" + barc] 
    if susproto != "": ctest = [run, header, susproto + "\n" + susprotoc, foo + "\n" + fooc, bar + "\n" + barc, sus + "\n" + susc] 

    file = open(filename, "w+") 
    file.write('\n\n'.join(ctest)) 
    file.close() 

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
    process(i)