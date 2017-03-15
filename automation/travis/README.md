# Build System for Checked C

Because Building Clang/LLVM is actually really complicated, we have
this repository as a completely seperate space to build within.

Effectively, we clone all the repos we want to build into the correct
structure within this directory, and then build. This works because it means we can
have a build directory that isn't within the git tree for llvm/clang. This also means
we can run the LLVM external test suite (or our fork of it)

The final aim is that any changes to the repositories in the "Built Repos" list
will trigger a build in this repo, correctly checking out the branch that we want,
and choosing master for all other branches.

## Built Repos

- `checkedc-llvm`
- `checkedc-clang`
- `checkedc`
- `checkedc-llvm-test-suite`
- `lnt`

## Build Steps

All the following commands should be run within the current directory.

`build.sh` will run all these steps except `install-pkgs.sh`.

- Run `install-pkgs.sh`, it will need sudo, and can probably just be done once when setting up
  the build machine.
- Run `install.sh`, this installs things like the correct version of cmake
  and sets up a python virtualenv for LNT. This exports some environment variables for `checkout.sh`, =
  so will need to be run as `. ./install.sh`
- Run `checkout.sh`, which checks out all the repositories into the correct directory structure, and
  checks out the correct branches for the build. It then uses cmake to make a build directory, installs lnt
  into the virtualenv that `install.sh` created. It's probably sensible to run this as `. ./checkout.sh` too.
- Run `make -j${NPROC} --no-print-directory -C llvm.build`, which goes into the llvm.build directory
  and builds all the executables, including clang. This is a seperate step to the next one on travis so
  that the logs are kept seperate (this produces lots of logs, most of which are useless).
- Run `make -j${NPROC} --no-print-directory -C llvm.build --keep-going check-checkedc check-clang`,
  which goes into llvm.build and runs the checkedc tests and clang regression tests
- Run `./llvm.lnt.ve/bin/lnt runtest nt --sandbox ./llvm.lnt.sandbox --cc ./llvm.build/bin/clang --test-suite ./llvm-test-suite --cflags -fcheckedc-extension`,
  which runs the full llvm test suite, benchmarks and all.

If you want to control the degree of parallelism required, export `NPROC_LIMIT` set to
the maximum number of cores you want compilation to use. The default limit is 8. If `nproc`
returns a lower value than limit, that will be used instead of the limit.

## Requirements

The scripts in this directory assume Linux (Ubuntu 14.04).

Currently we assume that you have the following installed already:
- a C/C++ compiler (clang/gcc)
- Make
- python 2.7 and virtualenv
- `nproc` command/utility (seems to be in GNU coreutils)
- awk. Because bash doesn't have a `minimum` function
- Anything else you need installed to build Clang.

## Caching

On travis we use ccache (which is not required), as well as caching the following directories:

- `llvm` our clone of checkedc-llvm (with the checkedc-clang and checkedc repos as subdirectories)
- `llvm-test-suite` our clone of checkedc-llvm-test-suite
- `lnt` our clone of lnt
- `llvm.build` our llvm build dir
- `llvm.lnt.sandbox` our LNT sandbox dir
- `llvm.lnt.ve` the virtualenv directory for LNT
- `cmake` our local version of cmake, if it exists
