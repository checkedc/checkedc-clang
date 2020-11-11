# Use of `clang-tidy` on 3C

We're starting to use `clang-tidy` to automatically enforce some of the LLVM
style guidelines in our code. The main configuration is in
`clang/lib/3C/.clang-tidy`.

We're currently using version 11.0.0 due to bugs in older versions, so if you
want to verify that our code is compliant with `clang-tidy`, you'll need to get
a copy of that version. If it isn't available from your OS distribution, you may
need to build it from source. This repository is currently based on a
pre-release of LLVM 9.0.0, so you can't use its version of `clang-tidy`.
However, Microsoft plans to upgrade it to LLVM 11.0.0 soon, at which point
you'll have the option to use `clang-tidy` built from this repository.

This file maintains a list of `clang-tidy` problems we've encountered that led
to warnings we needed to suppress, so those suppressions can refer to the
corresponding entries in this file.

## `_3C` name prefix

We use `3C` in the names of some program elements. Since an identifier cannot
begin with a digit, for names that would begin with `3C`, we decided that the
least evil is to prepend an underscore so the name begins with `_3C`. (A leading
underscore sometimes suggests that a program element is "private" or "special",
which is not the case here; this may be misleading.) One alternative we
considered was to use `ThreeC` at the beginning of a name, but that would be
inconsistent and would make it more work to search for all occurrences of either
`3C` or `ThreeC`. And we thought the alternative of using `ThreeC` everywhere
was just too clunky.

Unfortunately, the current implementation of the [LLVM naming
guideline](https://llvm.org/docs/CodingStandards.html#name-types-functions-variables-and-enumerators-properly)
in the `readability-identifier-naming` check does not allow names to begin with
underscores; in fact, it proposes to remove the underscores, leaving invalid
identifiers that break compilation. There's no indication that the guideline has
contemplated our scenario, and we don't know whether its maintainers would allow
our scenario if they knew about it; we've filed [a
bug](https://bugs.llvm.org/show_bug.cgi?id=48230) to ask.
