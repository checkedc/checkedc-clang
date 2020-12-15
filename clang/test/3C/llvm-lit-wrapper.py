import os

os.system("rm *.checked*")
os.system("llvm-lit *.c") 
os.system("rm *.checked*")
