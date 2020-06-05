import os

os.system("rm *.checked*")
os.system("../../../build/bin/llvm-lit *.c") 
os.system("rm *.checked*")