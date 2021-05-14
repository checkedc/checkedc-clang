# 3C: Semi-automated conversion of C code to Checked C

The 3C (**C**hecked-**C**-**C**onvert) software facilitates conversion
of existing C code to Checked C by automatically inferring as many
Checked C annotations as possible and guiding the developer through
the process of assigning the remaining ones. 3C aims to provide the
first feasible way for organizations with large legacy C codebases
(that they don't want to drop everything to rewrite in a better
language) to comprehensively verify their code's spatial memory
safety.

This document describes 3C in general. Here are the [build
instructions](INSTALL.md). The inference logic is implemented in the
[`clang/lib/3C` directory](../../../lib/3C) in the form of a library
that can potentially be used in multiple contexts. As of November
2020, the only way to use 3C is via the `3c` command line tool in the
[`clang/tools/3c` directory](../../../tools/3c); its usage is
documented in [the readme there](../../../tools/3c/README.md).

## What 3C users should know about the development process

Checked C development as a whole is led by Microsoft in the
https://github.com/microsoft/checkedc-clang repository. 3C is included
in the Checked C codebase, but 3C development is led by [Correct
Computation, Inc.](https://correctcomputation.com/) (CCI) in the
https://github.com/correctcomputation/checkedc-clang repository, which
is a fork of Microsoft's repository; changes are periodically merged
back and forth. (That is, CCI plays roughly the role of a ["subsystem
maintainer"](https://www.kernel.org/doc/html/latest/process/2.Process.html#how-patches-get-into-the-kernel)
for 3C, for those familiar with that term from Linux kernel
development.) Both 3C and the rest of the Checked C tools (including
the compiler) are available in both repositories, but the repositories
will generally have different versions of different parts of the code
at any given time, so [you'll have to decide which repository is more
suitable for your work](#which-checkedc-clang-repository-to-use).

Issues and pull requests related to 3C should be submitted to CCI's
repository; see [CONTRIBUTING.md](CONTRIBUTING.md) for more
information.

As of March 2021, 3C is pre-alpha quality and we are just starting to
establish its public presence and processes. CCI is also working on a
proprietary extension of 3C called 5C ("**C**orrect **C**omputation's
**C**hecked-**C**-**C**onvert"). Our current plan is that 3C will
contain the core inference logic, while 5C will add features to
enhance developer productivity. If you'd like more information about
5C, please contact us at info@correctcomputation.com.

### Which `checkedc-clang` repository to use?

We typically recommend that serious 3C users use CCI's repository to
get 3C fixes and enhancements sooner, but in some scenarios, you may
be better off with Microsoft's repository. Here, in detail, are the
factors we can think of that might affect your decision (as of March
2021):

- CCI strives to merge changes reasonably quickly from Microsoft's
  repository to CCI's, but most 3C-specific changes are made first in
  CCI's repository and merged to Microsoft's in batches every few
  months. Thus, CCI's repository typically gives you a significantly
  newer version of 3C, while Microsoft's repository typically gives
  you a somewhat newer version of the rest of the Checked C codebase,
  including some shared code used by 3C. The implication of that last
  point is that a fix to the shared code in Microsoft's repository
  would benefit Microsoft's copy of 3C immediately but would not take
  effect on CCI's copy until the next merge from Microsoft to CCI, so
  CCI's copy of 3C is not always newer in _all_ respects.

- While the [3C regression tests](CONTRIBUTING.md#testing) are run on
  every change to either repository, CCI's copy of 3C undergoes
  additional testing: a set of "benchmark" tests that run nightly,
  plus manual use by CCI engineers. If a change to CCI's repository
  passes the regression tests but breaks the additional tests, we will
  generally fix the problem quickly. But if the same happens in
  Microsoft's repository, functionality of Microsoft's copy of 3C that
  is not covered by the regression tests could remain broken for some
  time. The problem would likely be detected the next time CCI tries
  to merge from Microsoft, in which case we would try to get a fix
  into Microsoft's repository reasonably quickly.

- The 3C regression tests are run regularly on Windows on Microsoft's
  repository but not on CCI's. A change that breaks the regression
  tests on Windows generally won't be made to Microsoft's repository,
  but such a change could be made to CCI's and the problem may not be
  detected until the next time CCI tries to merge to Microsoft (or a
  user reports the problem to us). So you're less likely to encounter
  Windows-specific problems with Microsoft's repository.

- On the other hand, some CCI engineers work on Mac OS X and
  frequently run the regression tests and other manual tests on CCI's
  copy of 3C on Mac OS X, while we are unaware of any testing of
  Microsoft's copy of 3C on Mac OS X. So you may be less likely to
  encounter Mac-specific problems with CCI's repository. But so far,
  when we've seen Mac-specific problems, we've usually gotten a fix
  into Microsoft's repository reasonably quickly.

As noted in the [setup instructions](INSTALL.md#basics), both 3C and
the Checked C compiler depend on the Checked C system headers, which
Microsoft maintains in [a subdirectory of a separate repository named
`checkedc`](https://github.com/microsoft/checkedc/tree/master/include).
CCI has [a fork of this
repository](https://github.com/correctcomputation/checkedc), but
currently it is used only for submitting changes to Microsoft. All
users should use Microsoft's `checkedc` repository regardless of which
`checkedc-clang` repository they use.
