# Author: Shilpa Roy 
# Last updated: June 16, 2020

import itertools as it
import os
import subprocess

#### USERS PUT YOUR INFO HERE ##### 

# Please remember to add a '/' at the very end!
path_to_monorepo ="" #"/Users/shilpa-roy/checkedc-clang/build/bin/"



prefixes = ["arr", "arrstruct", "arrinstruct", "arrofstruct", "safefptrarg", "unsafefptrarg", "fptrsafe", "fptrunsafe", "fptrarr", "fptrarrstruct", "fptrinstruct", "fptrarrinstruct", "ptrTOptr"] #, "safefptrs", "unsafefptrs", "arrOFfptr"] 
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
header = """
#define size_t int
#define NULL 0
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern int printf(const char * restrict format : itype(restrict _Nt_array_ptr<const char>), ...);
extern _Unchecked char *strcpy(char * restrict dest, const char * restrict src : itype(restrict _Nt_array_ptr<const char>));\n""" 

# miscallaneous struct definitions that may or may not be used by the files above
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
    int (*funcs[5]) (int);
};

int add1(int x) { 
    return x+1;
} 

int sub1(int x) { 
    return x-1; 
} 

int fact(int n) { 
    if(n==0) { 
        return 1;
    } 
    return n*fact(n-1);
} 

int fib(int n) { 
    if(n==0) { return 0; } 
    if(n==1) { return 1; } 
    return fib(n-1) + fib(n-2);
} 

int zerohuh(int n) { 
    return !n;
}

int *mul2(int *x) { 
    *x *= 2; 
    return x;
}
"""

# this function will generate a C file that contains 
# the core of the example (before the addition of checked annotations)
def method_gen(prefix, proto, suffix): 
    return_type = arg_type = susbody = foobody = barbody = foo = bar = sus = susproto = ""

    # main processing to distinguish between the different types of test we wish to create
    if prefix=="arr": 
        return_type = "int *" 
        arg_type = "int *" 
        susbody = """
        int *z = calloc(5, sizeof(int)); 
        int i, *p, fac;
        for(i = 0, p = z, fac = 1; i < 5; ++i, p++, fac *= i) 
        { *p = fac; }"""
    elif prefix=="arrstruct":
        return_type = "int *" 
        arg_type = "struct general *" 
        barbody = foobody = """
        struct general *curr = y;
        int i;
        for(i = 1; i < 5; i++, curr = curr->next) { 
            curr->data = i;
            curr->next = malloc(sizeof(struct general));
            curr->next->data = i+1;
        }
        """
        susbody = """
        int *z = calloc(5, sizeof(int)); 
        struct general *p = y;
        int i;
        for(i = 0; i < 5; p = p->next, i++) { 
            z[i] = p->data; 
        } 
        """
    elif prefix=="arrinstruct":
        return_type = "struct warr *" 
        arg_type = "struct warr *"
        susbody = """
        char name[20]; 
        struct warr *z = y;
        int i;
        for(i = 0; i < 5; i++) { 
            z->data1[i] = i; 
        }
        """
    elif prefix=="arrofstruct":
        return_type = "struct general **"
        arg_type = "struct general *" 
        susbody = """ 
        struct general **z = calloc(5, sizeof(struct general *));
        struct general *curr = y;
        int i;
        for(i = 0; i < 5; i++) { 
            z[i] = curr; 
            curr = curr->next; 
        } 
        """ 
        barbody = foobody = """
        struct general *curr = y;
        int i;
        for(i = 1; i < 5; i++, curr = curr->next) { 
            curr->data = i;
            curr->next = malloc(sizeof(struct general));
            curr->next->data = i+1;
        }
        """ 
    elif prefix=="safefptrarg": 
        sus = "\nint * sus(int (*x) (int), int (*y) (int)) {\n"
        susproto = "\nint * sus(int (*) (int), int (*) (int));\n"
        foo = "\nint * foo() {\n"
        bar = "\nint * bar() {\n"
        susbody = """ 
        x = (int (*) (int)) 5;
        int *z = calloc(5, sizeof(int));
        int i;
        for(i = 0; i < 5; i++) { 
            z[i] = y(i);
        }
        """
        foobody = barbody = """ 
        int (*x)(int) = add1; 
        int (*y)(int) = sub1; 
        int *z = sus(x, y);
        """
    elif prefix=="unsafefptrarg":
        sus = "\nint * sus(int (*x) (int), int (*y) (int)) {\n"
        susproto = "\nint * sus(int (*) (int), int (*) (int));\n"
        foo = "\nint * foo() {\n"
        bar = "\nint * bar() {\n"
        susbody = """ 
        x = (int (*) (int)) 5;
        int *z = calloc(5, sizeof(int));
        int i;
        for(i = 0; i < 5; i++) { 
            z[i] = y(i);
        }
        """
        foobody = barbody = """ 
        int (*x)(int) = add1; 
        int (*y)(int) = mul2; 
        int *z = sus(x, y);
        """
    elif prefix=="safefptrs": 
        susproto = "\nint * (*sus(int (*) (int), int (*) (int))) (int *);\n"
        sus = "\nint * (*sus(int (*x) (int), int (*y) (int))) (int *) {\n"
        foo = "\nint * (*foo(void)) (int *) {\n"
        bar = "\nint * (*bar(void)) (int *) {\n" 
        susbody = """ 
        x = (int (*) (int)) 5; 
        int * (*z)(int *) = mul2;
        """
        foobody = barbody = """
        int (*x)(int) = add1; 
        int (*y)(int) = sub1; 
        int *(*z)(int *) = sus(x, y);
        """
    elif prefix=="unsafefptrs": 
        susproto = "\nchar * (*sus(int (*) (int), int (*) (int))) (int *);\n"
        sus = "\nchar * (*sus(int (*x) (int), int (*y) (int))) (int *) {\n"
        foo = "\nchar * (*foo(void)) (int *) {\n"
        bar = "\nchar * (*bar(void)) (int *) {\n" 
        susbody = """ 
        x = (int (*) (int)) 5; 
        char * (*z)(int *) = fib;
        """
        foobody = barbody = """
        int (*x)(int) = add1; 
        int (*y)(int) = sub1; 
        int *(*z)(int *) = sus(x, y);
        """
    elif prefix=="fptrsafe":
        sus = "\nint * sus(struct general *x, struct general *y) {\n"
        susproto = "\nint * sus(struct general *, struct general *);\n"
        foo = "\nint * foo() {\n"
        bar = "\nint * bar() {\n"
        barbody = foobody = """
        struct general *x = malloc(sizeof(struct general)); 
        struct general *y = malloc(sizeof(struct general));
        struct general *curr = y;
        int i;
        for(i = 1; i < 5; i++, curr = curr->next) { 
            curr->data = i;
            curr->next = malloc(sizeof(struct general));
            curr->next->data = i+1;
        }
        int * (*sus_ptr)(struct general *, struct general *) = sus;   
        int *z = sus_ptr(x, y);
        """
        susbody = """
        x = (struct general *) 5;
        int *z = calloc(5, sizeof(int)); 
        struct general *p = y;
        int i;
        for(i = 0; i < 5; p = p->next, i++) { 
            z[i] = p->data; 
        } 
        """
    elif prefix=="fptrunsafe":
        sus = "\nint * sus(struct general *x, struct general *y) {\n"
        susproto = "\nint * sus(struct general *, struct general *);\n"
        foo = "\nint * foo() {\n"
        bar = "\nint * bar() {\n"
        barbody = foobody = """
        struct general *x = malloc(sizeof(struct general)); 
        struct general *y = malloc(sizeof(struct general));
        struct general *curr = y;
        int i;
        for(i = 1; i < 5; i++, curr = curr->next) { 
            curr->data = i;
            curr->next = malloc(sizeof(struct general));
            curr->next->data = i+1;
        }
        int (*sus_ptr)(struct fptr *, struct fptr *) = sus;   
        int *z = (int *) sus_ptr(x, y);
        """
        susbody = """
        x = (struct general *) 5;
        int *z = calloc(5, sizeof(int)); 
        struct general *p = y;
        int i;
        for(i = 0; i < 5; p = p->next, i++) { 
            z[i] = p->data; 
        } 
        """
    elif prefix=="fptrarr":
        sus = "\nint ** sus(int *x, int *y) {\n"
        susproto = "\nint ** sus(int *, int *);\n"
        foo = "\nint ** foo() {\n"
        bar = "\nint ** bar() {\n"
        susbody = """
        x = (int *) 5;
        int **z = calloc(5, sizeof(int *)); 
        int * (*mul2ptr) (int *) = mul2;
        int i;
        for(i = 0; i < 5; i++) { 
            z[i] = mul2ptr(&y[i]);
        } 
        """
        foobody = barbody = """
        int *x = malloc(sizeof(int)); 
        int *y = calloc(5, sizeof(int)); 
        int i;
        for(i = 0; i < 5; i++) { 
            y[i] = i+1;
        } 
        int *z = sus(x, y);
        """
    elif prefix=="arrOFfptr":
        sus = "\nint (**sus(int *x, int *y)) (int) { \n"
        susproto = "\nint (**sus(int *x, int *y)) (int);\n" 
        foo = "\nint (**foo(void)) (int) {"
        bar = "\nint (**bar(void)) (int) {"

        foobody = barbody = """
        int *x = malloc(sizeof(int));
        int *y = calloc(5, sizeof(int)); 
        int i;
        for(i = 0; i < 5; i++) {
            y[i] = i+1;
        } 
        int (**z)(int) = sus(x, y);
        """

        susbody= """ 
        x = (int *) 5;
        int (**z)(int) = calloc(5, sizeof(int (*) (int))); 
        z[0] = add1;
        z[1] = sub1; 
        z[2] = zerohuh;
        z[3] = fib;
        z[4] = fact;
        int i;
        for(i = 0; i < 5; i++) { 
            y[i] = z[i](y[i]);
        }
        """
    elif prefix=="fptrinstruct":
        sus = "\nstruct fptr * sus(struct fptr *x, struct fptr *y) {\n"
        susproto = "\nstruct fptr * sus(struct fptr *, struct fptr *);\n"
        foo = "\nstruct fptr * foo() {\n"
        bar = "\nstruct fptr * bar() {\n"
        susbody = """ 
        x = (struct fptr *) 5; 
        struct fptr *z = malloc(sizeof(struct fptr)); 
        z->value = y->value; 
        z->func = fact;
        """
        foobody = barbody = """ 
        struct fptr * x = malloc(sizeof(struct fptr)); 
        struct fptr *y =  malloc(sizeof(struct fptr));
        struct fptr *z = sus(x, y);
        """
    elif prefix=="fptrarrstruct":
        sus = "\nstruct fptrarr * sus(struct fptrarr *x, struct fptrarr *y) {\n"
        susproto = "\nstruct fptrarr * sus(struct fptrarr *, struct fptrarr *);\n"
        foo = "\nstruct fptrarr * foo() {\n"
        bar = "\nstruct fptrarr * bar() {\n"
        susbody = """ 
        x = (struct fptrarr *) 5; 
        char name[30]; 
        struct fptrarr *z = malloc(sizeof(struct fptrarr)); 
        z->values = y->values; 
        z->name = strcpy(name, "Hello World");
        z->mapper = fact; 
        int i;
        for(i = 0; i < 5; i++) { 
            z->values[i] = z->mapper(z->values[i]);
        }
        """ 
        foobody = barbody = """ 
        char name[20]; 
        struct fptrarr * x = malloc(sizeof(struct fptrarr));
        struct fptrarr *y =  malloc(sizeof(struct fptrarr));
        int *yvals = calloc(5, sizeof(int)); 
        int i;
        for(i = 0; i < 5; i++) {
            yvals[i] = i+1; 
            }  
        y->values = yvals; 
        y->name = name; 
        y->mapper = NULL;
        strcpy(y->name, "Example"); 
        struct fptrarr *z = sus(x, y);
        """ 
    elif prefix=="fptrarrinstruct":
        sus = "\nstruct arrfptr * sus(struct arrfptr *x, struct arrfptr *y) {\n"
        susproto = "\nstruct arrfptr * sus(struct arrfptr *, struct arrfptr *);\n"
        foo = "\nstruct arrfptr * foo() {\n"
        bar = "\nstruct arrfptr * bar() {\n"
        susbody = """ 
        x = (struct arrfptr *) 5; 
        struct arrfptr *z = malloc(sizeof(struct arrfptr)); 
        int i;
        for(i = 0; i < 5; i++) { 
            z->args[i] = i + 1; 
        } 
        z->funcs[0] = add1;
        z->funcs[1] = sub1; 
        z->funcs[2] = zerohuh;
        z->funcs[3] = fib;
        z->funcs[4] = fact;
        """ 
        foobody = barbody = """ 
        struct arrfptr * x = malloc(sizeof(struct arrfptr));
        struct arrfptr * y =  malloc(sizeof(struct arrfptr));
       
        struct arrfptr *z = sus(x, y); 
        int i;
        for(i = 0; i < 5; i++) { 
            z->args[i] = z->funcs[i](z->args[i]);
        }
        """
    elif prefix=="ptrTOptr":
        return_type = "char ***"
        arg_type = "char * * *"
        susbody = """
        char *ch = malloc(sizeof(char)); 
        *ch = 'A'; /*Capital A*/
        char *** z = malloc(5*sizeof(char**)); 
        for(int i = 0; i < 5; i++) { 
            z[i] = malloc(5*sizeof(char *)); 
            for(int j = 0; j < 5; j++) { 
                z[i][j] = malloc(2*sizeof(char)); 
                strcpy(z[i][j], ch);
                *ch = *ch + 1; 
            }
        }
        """

    # generate standard enders and duplications that occur in all generated tests

    if not "fptr" in prefix: 
        barbody += "{} z = sus(x, y);".format(return_type) 
        foobody += "{} z = sus(x, y);".format(return_type)
        data = [return_type, arg_type, arg_type]
        susproto = "\n{} sus({}, {});\n".format(*data)
        sus = "\n{} sus({} x, {} y) {}\nx = ({}) 5;".format(data[0], data[1], data[2], "{", arg_type)
        arg_np = " ".join(arg_type.split(" ")[:-1])
        foo = """\n{} foo() {}
        {} x = malloc(sizeof({}));
        {} y = malloc(sizeof({}));
        """.format(return_type, "{", arg_type, arg_np, arg_type, arg_np) 
        bar = """\n{} bar() {}
        {} x = malloc(sizeof({}));
        {} y = malloc(sizeof({}));
        """.format(return_type, "{", arg_type, arg_np, arg_type, arg_np)       
        
    # create unsafe use cases based on the suffix (by default, the generated code is safe)

    if suffix == "both": 
        susbody += "\nz += 2;"
        barbody += "\nz += 2;" 
    elif suffix == "callee": 
        susbody += "\nz += 2;" 
    elif suffix == "caller": 
        barbody += "\nz += 2;"
    
    susbody += "\nreturn z; }\n"
    foobody += "\nreturn z; }\n" 
    barbody += "\nreturn z; }\n"

    return [susproto, sus+susbody, foo+foobody, bar+barbody] 

def process_file(file, cname, cname2, proto, alltypes, new_defs, new_defs2, susprotoc, susc, fooc, barc): 
    check = "CHECK_NOALL"
    if alltypes: check = "CHECK_ALL"

    keywords = "int char struct double float".split(" ") 
    ckeywords = "_Ptr _Array_ptr _Nt_array_ptr".split(" ")

    # these boolean variables indicate which method definition we are in, so we know where to add
    # our checked annotations later
    indefs = insus = infoo = inbar = False 
    inwarr = infptrarr = infptr = inarrfptr = False

    # generate the check annotations
    for line in file.readlines():   
        linepre = line.split("=")[0]
        # this indicates that we've reached the definitions
        if line.find("struct general {") != -1: 
            indefs = insus = infoo = inbar = False 
            indefs = True

        # annotate the prototype for sus
        elif proto != "" and line.find("sus") != -1 and line.find("=") == -1 and line.find(";") != -1: 
           indefs = insus = infoo = inbar = False
           susprotoc += "//" + check + ": " + line

        # annotate the definition for sus
        elif proto != "multi" and line.find("sus") != -1 and line.find("{") != -1: 
            indefs = insus = infoo = inbar = False
            insus = True
            susc += "//" + check + ": " + line

        # annotate the definition for foo
        elif line.find("foo") != -1:
            indefs = insus = infoo = inbar = False
            infoo = True 
            fooc += "//" + check + ": " + line  

        # annotate the definition for bar
        elif line.find("bar") != -1:  
            indefs = insus = infoo = inbar = False
            inbar = True 
            barc += "//" + check + ": " + line  

        elif indefs: 
            # struct general
            if line.find("general") != -1 and line.find(";") != -1: 
                new_defs[0] += "\n//" + check + ": " + line

            # struct warr
            elif line.find("struct warr {") != -1: 
                inwarr = True
            elif line.find("data1") != -1 and inwarr: 
                new_defs[1] += "\n//" + check + ": " + line 
            elif line.find("name") != -1 and inwarr: 
                inwarr = False 
                new_defs[1] += "//" + check + ": " + line 

            # struct fptrarr
            elif line.find("struct fptrarr {") != -1: 
                infptrarr = True
            elif line.find("values") != -1 and infptrarr: 
                new_defs[2] += "\n//" + check + ": " + line 
            elif line.find("name") != -1 and infptrarr: 
                new_defs[2] += "//" + check + ": " + line
            elif line.find("mapper") != -1 and infptrarr: 
                infptrarr = False 
                new_defs[2] += "//" + check + ": " + line
            
            # struct fptr
            elif line.find("struct fptr {") != -1: 
                infptr = True
            elif line.find("value") != -1 and infptr: 
                new_defs[3] += "\n//" + check + ": " + line 
            elif line.find("func") != -1 and infptr: 
                infptr = False 
                new_defs[3] += "//" + check + ": " + line

            # struct arrfptr
            elif line.find("struct arrfptr {") != -1: 
                inarrfptr = True
            elif line.find("args") != -1 and inarrfptr: 
                new_defs[4] += "\n//" + check + ": " + line 
            elif line.find("funcs") != -1 and inarrfptr: 
                inarrfptr = False 
                new_defs[4] += "//" + check + ": " + line
                 
            # mul2
            elif line.find("mul2") != -1:
                indefs = False
                new_defs[-1] += "\n//" + check + ": " + line 
        
        elif insus: 
            if (any(substr in linepre for substr in keywords) and linepre.find("*") != -1) or any(substr in line for substr in ckeywords):
                susc += "//" + check + ": " + line
        elif infoo: 
            if (any(substr in linepre for substr in keywords) and linepre.find("*") != -1) or any(substr in line for substr in ckeywords):
                fooc += "//" + check + ": " + line 
        elif inbar: 
            if (any(substr in linepre for substr in keywords) and linepre.find("*") != -1) or any(substr in line for substr in ckeywords):
                barc += "//" + check + ": " + line
    
    if proto=="multi": 
        file2 = open(cname2, "r")

        for line in file2.readlines():  
            linepre = line.split("=")[0] 
            # this indicates that we've reached the definitions
            if line.find("struct general {") != -1: 
                indefs = insus = infoo = inbar = False 
                indefs = True

            # annotate the definition for sus
            elif line.find("sus") != -1 and line.find("{") != -1: 
                indefs = insus = infoo = inbar = False
                insus = True
                susc += "//" + check + ": " + line

            elif insus: 
                if (any(substr in linepre for substr in keywords) and linepre.find("*") != -1) or any(substr in line for substr in ckeywords):
                    susc += "//" + check + ": " + line

            elif indefs: 
                # struct general
                if line.find("general") != -1 and line.find(";") != -1: 
                    new_defs2[0] += "\n//" + check + ": " + line

                # struct warr
                elif line.find("struct warr {") != -1: 
                    inwarr = True
                elif line.find("data1") != -1 and inwarr: 
                    new_defs2[1] += "\n//" + check + ": " + line 
                elif line.find("name") != -1 and inwarr: 
                    inwarr = False 
                    new_defs2[1] += "//" + check + ": " + line 

                # struct fptrarr
                elif line.find("struct fptrarr {") != -1: 
                    infptrarr = True
                elif line.find("values") != -1 and infptrarr: 
                    new_defs2[2] += "\n//" + check + ": " + line 
                elif line.find("name") != -1 and infptrarr: 
                    new_defs2[2] += "//" + check + ": " + line
                elif line.find("mapper") != -1 and infptrarr: 
                    infptrarr = False 
                    new_defs2[2] += "//" + check + ": " + line
            
                # struct fptr
                elif line.find("struct fptr {") != -1: 
                    infptr = True
                elif line.find("value") != -1 and infptr: 
                    new_defs2[3] += "\n//" + check + ": " + line 
                elif line.find("func") != -1 and infptr: 
                    infptr = False 
                    new_defs2[3] += "//" + check + ": " + line

                # struct arrfptr
                elif line.find("struct arrfptr {") != -1: 
                    inarrfptr = True
                elif line.find("args") != -1 and inarrfptr: 
                    new_defs2[4] += "\n//" + check + ": " + line 
                elif line.find("funcs") != -1 and inarrfptr: 
                    inarrfptr = False 
                    new_defs2[4] += "//" + check + ": " + line
                 
                # mul2
                elif line.find("mul2") != -1:
                    indefs = False
                    new_defs2[-1] += "\n//" + check + ": " + line 
            
        file2.close() 
    return [new_defs, new_defs2, susprotoc, susc, fooc, barc]

# this function will generate a C file using method_gen(), 
# run the porting tool on that C file, and generate a new
# C file that contains the annotations for llvm-lit
def annot_gen(prefix, proto, suffix):  
 
    # generate a descriptive comment that describes what the test will do: 
    comm_general = "/*This file tests three functions: two callers bar and foo, and a callee sus*/\n" 
    comm_prefix = "/*In particular, this file tests: "
    if prefix=="arr": comm_prefix += "arrays through a for loop and pointer\narithmetic to assign into it*/" 
    if prefix=="arrstruct": comm_prefix += "arrays and structs, specifically by using an array to\ntraverse through the values of a struct*/" 
    if prefix=="arrinstruct": comm_prefix += "how the tool behaves when there is an array\nfield within a struct*/"
    if prefix=="arrofstruct": comm_prefix += "how the tool behaves when there is an array\nof structs*/"
    if prefix=="safefptrarg": comm_prefix += "passing a function pointer as an argument to a\nfunction safely (without unsafe casting)*/"
    if prefix=="unsafefptrarg": comm_prefix += "passing a function pointer as an argument to a\nfunction unsafely (by casting it unsafely)*/"
    if prefix=="safefptrs": comm_prefix += "passing function pointers in as arguments and\nreturning a function pointer safely*/" 
    if prefix=="arrOFfptr": comm_prefix += "how the tool behaves when returning an array\nof function pointers*/"
    if prefix=="unsafefptrs": comm_prefix += "passing fptrs in as arguments and returning a\nfptr unsafely (through unsafe casting*/"
    if prefix=="fptrsafe": comm_prefix += "converting the callee into a function pointer\nand then using that pointer for computations*/"
    if prefix=="fptrunsafe": comm_prefix += "converting the callee into a function pointer\nunsafely via cast and using that pointer for computations*/"
    if prefix=="fptrarr": comm_prefix += "using a function pointer and an array in\ntandem to do computations*/"
    if prefix=="fptrarrstruct": comm_prefix += "using a function pointer and an array as fields\nof a struct that interact with each other*/"
    if prefix=="fptrinstruct": comm_prefix += "how the tool behaves when a function pointer\nis a field of a struct*/"
    if prefix=="fptrarrinstruct": comm_prefix += "how the tool behaves when there is an array\nof function pointers in a struct*/"
    if prefix=="ptrTOptr": comm_prefix += "having a pointer to a pointer*/"
    comm_proto = "" 
    if proto=="multi": comm_proto = "\n/*For robustness, this test is identical to {}.c and {}.c except in that\nthe callee and callers are split amongst two files to see how\nthe tool performs conversions*/".format(prefix+"proto"+suffix, prefix+suffix) 
    elif proto=="proto": comm_proto = "\n/*For robustness, this test is identical to {}.c except in that\na prototype for sus is available, and is called by foo and bar,\nwhile the definition for sus appears below them*/".format(prefix+suffix)
    comm_suffix = ""
    if suffix == "safe": comm_suffix = "\n/*In this test, foo, bar, and sus will all treat their return values safely*/"
    elif suffix == "callee": comm_suffix = "\n/*In this test, foo and bar will treat their return values safely, but sus will\nnot, through invalid pointer arithmetic, an unsafe cast, etc*/"
    elif suffix == "caller": comm_suffix = "\n/*In this test, foo and sus will treat their return values safely, but bar will\nnot, through invalid pointer arithmetic, an unsafe cast, etc.*/"
    elif suffix == "both": comm_suffix = "\n/*In this test, foo will treat its return value safely, but sus and bar will not,\nthrough invalid pointer arithmetic, an unsafe cast, etc.*/"
    comm_dec = "\n\n/*********************************************************************************/\n\n" 

    comment = ''.join(["\n", comm_dec, comm_general, comm_prefix, comm_proto, comm_suffix, comm_dec])

    # generate the body of the file
    [susproto, sus, foo, bar] = method_gen(prefix, proto, suffix) 
    name = prefix + proto + suffix + ".c"
    cnameNOALL = prefix + proto + suffix + ".checkedNOALL.c"  
    cnameALL = prefix + proto + suffix + ".checkedALL.c"
    name2 = name 
    cname2NOALL = cnameNOALL 
    cname2ALL = cnameALL

    if proto=="multi": 
        name = prefix + suffix + proto + "1.c" 
        name2 = prefix + suffix + proto + "2.c"
        cnameNOALL = prefix + suffix + proto + "1.checkedNOALL.c"  
        cnameALL = prefix + suffix + proto + "1.checkedALL.c"
        cname2NOALL = prefix + suffix + proto + "2.checkedNOALL.c"  
        cname2ALL = prefix + suffix + proto + "2.checkedALL.c"
    
    if proto=="proto": test = header + definitions + susproto + foo + bar + sus
    elif proto=="multi": test = header + definitions + susproto + foo + bar
    else: test = header + definitions + sus + foo + bar 

    # write the main file 
    file = open(name, "w+") 
    file.write(test)
    file.close() 
    
    # generate the second file if a multi example
    if proto=="multi": 
        test2 = header + definitions + sus
        file = open(name2, "w+") 
        file.write(test2)
        file.close()
    
    # run the porting tool on the file(s)
    if proto=="multi": 
        os.system("{}cconv-standalone -alltypes -output-postfix=checkedALL {} {}".format(path_to_monorepo, name, name2))
        os.system("{}cconv-standalone -output-postfix=checkedNOALL {} {}".format(path_to_monorepo, name, name2))
        os.system("rm {} {}".format(name, name2))
    else: 
        os.system("{}cconv-standalone -alltypes -output-postfix=checkedALL {}".format(path_to_monorepo, name))
        os.system("{}cconv-standalone -output-postfix=checkedNOALL {}".format(path_to_monorepo, name))
        os.system("rm {}".format(name))
    
    bug_generated = False
    if proto != "multi":
        out = subprocess.Popen(['{}clang'.format(path_to_monorepo), '-c', cnameNOALL], stdout=subprocess.PIPE, stderr=subprocess.STDOUT) 
        stdout, stderr = out.communicate()
        stdout = str(stdout) 
        if "error:" in stdout: 
            bug_generated = True
            name = prefix + proto + suffix + "_BUG.c" 
    else: 
        out = subprocess.Popen(['{}clang'.format(path_to_monorepo), '-c', cnameNOALL, cname2NOALL], stdout=subprocess.PIPE, stderr=subprocess.STDOUT) 
        stdout, stderr = out.communicate()
        stdout = str(stdout) 
        if "error:" in stdout: 
            bug_generated = True
            name = prefix + suffix + proto + "1_BUG.c"
            name2 = prefix + suffix + proto + "2_BUG.c"
    
    # read the checked generated file for new types
    # isolate the definitions (at the top of the file) into more bitesize chunks  
    # this will make adding checked annotations to them much easier
    new_defs = definitions.split("\n\n")
    new_defs2 = definitions.split("\n\n") 
    susprotoc = susc = fooc = barc = ""
    file = open(cnameNOALL, "r") 
    [new_defs, new_defs2, susprotoc, susc, fooc, barc] = process_file(file, cnameNOALL, cname2NOALL, proto, False, new_defs, new_defs2, susprotoc, susc, fooc, barc)
    file.close() 
    os.system("rm {}".format(cnameNOALL))
    if proto=="multi": os.system("rm {}".format(cname2NOALL)) 

    file = open(cnameALL, "r") 
    [new_defs, new_defs2, susprotoc, susc, fooc, barc] = process_file(file, cnameALL, cname2ALL, proto, True, new_defs, new_defs2, susprotoc, susc, fooc, barc)
    file.close() 
    os.system("rm {}".format(cnameALL))
    if proto=="multi": os.system("rm {}".format(cname2ALL)) 

    if bug_generated: 
        cname = prefix + suffix + proto + "1_BUG.checked.c"
        cname2 = prefix + suffix + proto + "2_BUG.checked.c"
        return

    run = "// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes=\"CHECK_ALL\" %s"
    run += "\n//RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes=\"CHECK_NOALL\" %s"
    run += "\n//RUN: cconv-standalone -output-postfix=checkedNOALL %s" 
    run += "\n//RUN: %clang -c %S/{}".format(cnameNOALL)
    run += "\n//RUN: rm %S/{}".format(cnameNOALL)
    run2 = ""
    if proto=="multi": 
        run = "// RUN: cconv-standalone -base-dir=%S -alltypes -output-postfix=checkedALL %s %S/" + name2  
        run += "\n// RUN: cconv-standalone -base-dir=%S -output-postfix=checkedNOALL %s %S/" + name2 
        run += "\n//RUN: %clang -c %S/{} %S/{}".format(cnameNOALL, cname2NOALL)
        run += "\n//RUN: FileCheck -match-full-lines -check-prefixes=\"CHECK_NOALL\" --input-file %S/{} %s".format(cnameNOALL) 
        run += "\n//RUN: FileCheck -match-full-lines -check-prefixes=\"CHECK_ALL\" --input-file %S/{} %s".format(cnameALL)
        run += "\n//RUN: rm %S/{} %S/{}".format(cnameALL, cname2ALL)
        run += "\n//RUN: rm %S/{} %S/{}".format(cnameNOALL, cname2NOALL)
        cnameNOALL2 = prefix + suffix + proto + "1.checkedNOALL2.c"  
        cnameALL2 = prefix + suffix + proto + "1.checkedALL2.c"
        cname2NOALL2 = prefix + suffix + proto + "2.checkedNOALL2.c"  
        cname2ALL2 = prefix + suffix + proto + "2.checkedALL2.c"
        if bug_generated: 
            cname21 = prefix + suffix + proto + "1_BUG.checked2.c" 
            cname22 = prefix + suffix + proto + "2_BUG.checked2.c"
        run2 = "// RUN: cconv-standalone -base-dir=%S -alltypes -output-postfix=checkedALL2 %s %S/" + name  
        run2 += "\n// RUN: cconv-standalone -base-dir=%S -output-postfix=checkedNOALL2 %s %S/" + name 
        run2 += "\n//RUN: %clang -c %S/{} %S/{}".format(cnameNOALL2, cname2NOALL2)
        run2 += "\n//RUN: FileCheck -match-full-lines -check-prefixes=\"CHECK_NOALL\" --input-file %S/{} %s".format(cname2NOALL2) 
        run2 += "\n//RUN: FileCheck -match-full-lines -check-prefixes=\"CHECK_ALL\" --input-file %S/{} %s".format(cname2ALL2)
        run2 += "\n//RUN: rm %S/{} %S/{}".format(cnameALL2, cname2ALL2)
        run2 += "\n//RUN: rm %S/{} %S/{}".format(cnameNOALL2, cname2NOALL2)

    # generate the final file with all annotations
    ctest = run + comment + header + '\n\n'.join(new_defs) + sus + susc + foo + fooc + bar + barc
    if proto == "proto": 
        ctest = run + comment + header + '\n\n'.join(new_defs) + susproto + susprotoc + foo + fooc + bar + barc + sus + susc
    elif proto == "multi": 
        ctest = run + comment + header + '\n\n'.join(new_defs) + susproto + susprotoc + foo + fooc + bar + barc
        ctest2 = run2 + comment + header + '\n\n'.join(new_defs2) + sus + susc
    
    file = open(name, "w+") 
    file.write(ctest) 
    file.close() 
    if proto=="multi": 
        file = open(name2, "w+") 
        file.write(ctest2) 
        file.close()

    return

#arr, arrinstruct, arrofstruct
if __name__ == "__main__": 
    os.system("rm *.checked*")
    for skeleton in testnames: 
        annot_gen(skeleton[0], skeleton[1], skeleton[2])
    os.system("rm *.checked*")
