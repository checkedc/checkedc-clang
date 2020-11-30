This folder contains code that runs 3c on all the source files of a project.

  

## Prereqs:

Ensure that you have setup Checked C by following the instructions at
[https://github.com/microsoft/checkedc-clang/blob/master/clang/docs/checkedc/Setup-and-Build.md](https://github.com/microsoft/checkedc-clang/blob/master/clang/docs/checkedc/Setup-and-Build.md)
  
## Usage:

Compile your project to generate `compile_commands.json` by following the below
instructions:

### Generating `compile_commands.json`

#### Using `cmake`

Use the CMAKE_EXPORT_COMPILE_COMMANDS flag. You can run

```
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ...
```

or add the following line to your CMakeLists.txt script:

```
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

The `compile_commands.json` file will be put into the build directory.

#### Using `Bear` (Recommended)

For `make` and `cmake` based build systems, you can use `Bear`.

Install Bear from: https://github.com/rizsotto/Bear

Prepend `bear` to your make command i.e., if you were running `make -j4`

then run `bear make -j4`.

The above step ensures that the `compile_commands.json` will be present at the
root of the project folder.

Second, run `convert_project.py`:

```
python convert_project.py --help

usage: convert_project.py [-h] [--includeDir INCLUDEDIR] [-p PROG_NAME] -pr

PROJECT_PATH

  

Convert the provided project into Checked C.

  

optional arguments:

-h, --help show this help message and exit

--includeDir INCLUDEDIR

Path to the checkedC headers, run from a checkedCclang

repo

-p PROG_NAME, --prog_name PROG_NAME

Program name to run. i.e., path to 3c

-pr PROJECT_PATH, --project_path PROJECT_PATH

Path to the folder containing all project sources.

```

Example:

```
python convert_project.py -pr <project_folder> -p <checked_c_llvm_build_dir>/bin/3c --includeDir <checked_c_llvm_src_dir>/llvm/projects/checkedc-wrapper/checkedc/include
```
