# Explicit and Implicit Inclusion of Checked Header Files
Author: Sulekha Kulkarni, Approver: David Tarditi, Date Approved:


# OARP

| Role          | Person |
|-------------------|---------------|
| Owner         | Sulekha Kulkarni  |
| Approver      | David Tarditi     |
| Reviewers     | Joseph Lloyd, Mandeep Grang, Katherine Kjeer |
| Reviewers     | Mike Hicks and other folks at CCI |


## Goal
The goal of this document is to record the design considerations for allowing
programs to include the checked counterparts of system header files either
explicitly or implicitly during the conversion of these programs to Checked C.

## Problem Summary
The current approach for including checked header files, which is to explicitly
replace an included header file with its checked counterpart, is inconvenient
for adoption to Checked C - all the system header files included in a project
need to be altered. So we need a way to implicitly include checked header
files.

## Conditions that a Solution must Satisfy
 - The solution must provide users an option to either opt into the implicit
 inclusion of checked header files, or opt out of it if checked header files
 are implicitly included by default. This flexibility is required to support
 custom build environments.
 - The solution must be easy to integrate into an existing build system.
 - The current approach of explicitly including checked header files must
 always remain a viable alternative.
 - The order in which the compiler driver searches the include paths must remain
 unaltered.
 - The solution must be able to accommodate any clang-specific declarations in
 system header files, if present (Ex.: inttypes.h).
 - The Checked-C-specific declarations must live in the checkedc repository.


## Details of the Current Approach 
 - The current approach supports only the explicit inclusion of checked header
 files.
 - An installed Checked C compiler places the checked counterparts of system
 header files like `stdio_checked.h` in the directory
 `$INSTALL_DIR/lib/clang/<VERSION>/include`, which clang searches before it
 looks in system include directories. Note that there is no alteration of the
 include search path to accommodate any include files related to Checked C.
 - If a checked header file like `stdio_checked.h` is included in a program,
 clang picks it up from the above location.

## Solution Space
 - Solution 1:
     - Let `foo.h` be a system header file with a checked counterpart called
     `foo_checked.h`. We add a new file, also called `foo.h`, in the same
     directory that contains `foo_checked.h`. This new `foo.h` should contain
     `#include_next <foo.h>` plus all the Checked-C-specific declarations
     present in `foo_checked.h`. In addition, the original contents of
     `foo_checked.h` should be deleted and it should now only contain
     `#include <foo.h>`.
     - The way this solution will work:
          - If a program includes `foo.h`, clang will first pick up the
          "checked" version of `foo.h`. As this `foo.h` has a
          `#include_next <foo.h>`, the system `foo.h` will be picked up
          (infinite recursion is avoided because of include_next), and all the
          Checked-C-specific declarations in the "checked" version of `foo.h`
          will also be picked up.
          - If the program includes `foo_checked.h`, the line `#include <foo.h>`
          in `foo_checked.h` will initiate the same process as above.
     - **Pros**: 1) The current approach of explicit inclusion is supported.
       2) No changes are required to the build system. 3) No changes are
       required to the order in which the include directories are searched.
       4) It is convenient for automation.
     - **Cons**: 1) The solution does not provide a way to opt out of including
       checked headers. 2) While it is easy to accommodate clang-specific
       declarations in system header files (Checked-C-specific declarations can
       be added after the clang-specific declarations), this solution will
       require some Checked-C-specific declarations to be part of checkedc-clang
       repository.

 - Solution 2:
     - Have a different compiler driver to provide the facility for implicit
       inclusion of header files.
     - **Pros**: 1) The solution provides the option to opt into implicitly
       including checked header files. 2) The existing approach of explicit
       inclusion is supported. 3) In most cases integration into a build system
       will be easy: the `CC` variable can be redefined appropriately. 4) It is
       convenient for automation.
     - **Cons**: 1) It is a very heavy-weight solution. 2) In some cases
       integrating with a build system will be more involved when both the
       drivers are needed to compile the sources. This may happen in a scenario
       where most source files want implicit inclusion but a few source files
       want explicit inclusion because they want to directly include some system
       header files in order to avoid Checked-C-specific declarations.

 - Solution 3:
     - Let `foo.h` be a system header file with its checked counterpart called
       `foo_checked.h`. We add a new file called `foo.h` in the same directory
       that contains `foo_checked.h`. In this new file, we add the following:

             #ifdef IMPLICIT_INCLUDE_CHECKED_HDRS
             #include <foo_checked.h>
             #else
             #include_next <foo.h>
             #endif
     - In `foo_checked.h`, we modify `#include <foo.h>` to
       `#include_next <foo.h>`.

     - The way this solution will work:
        - If a program includes `foo.h`, clang will pick up either
          `foo_checked.h` or the system `foo.h` depending on whether or not
          `-DIMPLICIT_INCLUDE_CHECKED_HDRS` is specified on the compilation
          commandline.  Therefore specifying this flag will cause the implicit
          inclusion of the checked counterpart of `foo.h`, which will make it
          convenient for automation. Not specifying the commandline flag will
          not perform implicit inclusion, providing a mechanism to opt out of 
          implicit inclusion. Infinite recursion is avoided because of
	  `#include_next`.
        - If a program includes `foo_checked.h` current behavior will prevail.
          The above flag has no effect on the explicit inclusion of checked
          header files.

     - **Pros**: 1) The solution provides a way to opt into implicit inclusion
       of checked header files. 2) The existing approach of explicit inclusion
       is supported. 3) In most cases integration with the build system is easy:
       the flag `-DIMPLICIT_INCLUDE_CHECKED_HDRS` is passed to the compilation
       of all source files through the `CFLAGS` variable. 4) No changes are
       required to the order in which include directories are searched. 5) It
       is convenient for automation.

     - **Cons**: 1) In some cases integration with the build system will be more
       involved if the above flag needs to be passed to the compilation of most
       source files and avoided for a few source files that may want to directly
       include system header files in order to avoid Checked-C-specific
       declarations. 2) It is difficult to accomodate clang-specific
       declarations.

## Proposed Solution:

The proposed solution is as follows:
  - We will implicitly include checked header files by default and provide the 
    compilation flag NO_IMPLICIT_INCLUDE_CHECKED_HDRS to opt out of the implicit
    inclusion.
  - For header files that do not have clang-specific declarations, but have
    Checked-C-specific declarations (there are 13 such header files at present),
    we will do the following:
      - For each system header file, say `foo.h`, that does not have
        clang-specific declarations but has Checked-C-specific declarations, we
        will add a new file also called `foo.h` that will contain the following:

            #if !defined __checkedc || defined NO_IMPLICIT_INCLUDE_CHECKED_HDRS
            #include_next <foo.h>
            #else
            #include <foo_checked.h>
            #endif

      - The file `foo_checked.h` will contain `#include_next <foo.h>` and the
        Checked-C-specific declarations.

  - For a system header file that contains clang-specific declarations and also
    Checked-C-specific declarations (there is one such header file at present),
    we will do the following:
      - Let `bar.h` be such a header file. Then there already 
        exists a file called `bar.h` in the `clang/lib/Headers` directory,
        which contains clang-specific declarations in the following format:

            #include_next <bar.h>
            <currently existing clang-specific declarations>

        At the end of this pre-existing `bar.h`, we will add the following:

            #if defined __checkedc && !defined NO_IMPLICIT_INCLUDE_CHECKED_HDRS
            #include <bar_checked_internal.h>
            #endif

      - All the Checked-C-specific declarations (that are currently present in
        `bar_checked.h`) will be moved to `bar_checked_internal.h`. The file
        `bar_checked.h` will be modified to contain just the following:

            #include <bar.h>
            #include <bar_checked_internal.h>
