Functionality Tester
---
This folder contains scripts to test the functionality of `checked-c-convert` tool.
## Usage:
```
python run_tests.py --help
usage: FuntionalityTester [-h] -p PROG_NAME

Script that checks functionality of checked-c-convert tool

optional arguments:
  -h, --help            show this help message and exit
  -p PROG_NAME, --prog_name PROG_NAME
                        Program name to run. i.e., path to checked-c-convert
```
### Example:
```
python run_tests.py -p /Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/cmake-build-debug/bin/checked-c-convert 
[*] Got:11 tests.
[*] Running Tests.
[*] Testing:/Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/arr/basic_inter.c
[+] Test /Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/arr/basic_inter.c Passed.
[*] Testing:/Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/arr/basic_inter_field.c
[+] Test /Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/arr/basic_inter_field.c Passed.
[*] Testing:/Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/arr/basic_local.c
[+] Test /Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/arr/basic_local.c Passed.
[*] Testing:/Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/ntarr/basic_inter.c
[+] Test /Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/ntarr/basic_inter.c Passed.
[*] Testing:/Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/ntarr/basic_field_local.c
[+] Test /Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/ntarr/basic_field_local.c Passed.
[*] Testing:/Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/ntarr/basic_inter_field.c
[+] Test /Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/ntarr/basic_inter_field.c Passed.
[*] Testing:/Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/ntarr/basic_local.c
[+] Test /Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/ntarr/basic_local.c Passed.
[*] Testing:/Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/ptr/basic_inter.c
[+] Test /Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/ptr/basic_inter.c Passed.
[*] Testing:/Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/ptr/basic_field_local.c
[+] Test /Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/ptr/basic_field_local.c Passed.
[*] Testing:/Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/ptr/basic_inter_field.c
[+] Test /Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/ptr/basic_inter_field.c Passed.
[*] Testing:/Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/ptr/basic_local.c
[+] Test /Users/machiry/Projects/checkedc/llvm-stuff/checkedc-llvm/tools/clang/tools/checked-c-convert/functests/ptr/basic_local.c Passed.
[+] ALL TESTS PASSED.
```
