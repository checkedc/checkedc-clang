# Contributing to 3C

Issues and pull requests related to 3C should be submitted to [CCI's
`checkedc-clang`
repository](https://github.com/correctcomputation/checkedc-clang), not
[Microsoft's](https://github.com/microsoft/checkedc-clang), except as
stated below.

## Issues

Feel free to file issues against 3C, even during this early stage in
its development; filed issues will help inform us of what users want.
Please try to provide enough information that we can reproduce the bug
you are reporting or understand the specific enhancement you are
proposing. We strive to promptly fix any issues that block basic usage
of 3C in our [supported
environments](INSTALL.md#supported-environments).

## Pull requests

We are open to outside contributions to 3C, though we do not yet have
a sense of how much work we'll be willing to spend on integrating
them. As with any open-source project, we advise you to check with us
(e.g., by filing an issue) before you put nontrivial work into a
contribution in the hope that we'll accept it. Of course, you can also
consider that even if we do not accept your contribution, you will be
able to use your work yourself (subject to your ability to keep up
with us) and we may even direct users who want the functionality to
your version.

If your contribution does not touch any 3C-specific code (or is a
codebase-wide cleanup of low risk to 3C) and you can reasonably submit
it to [Microsoft's
repository](https://github.com/microsoft/checkedc-clang) instead, we
generally prefer that you do so. If such a contribution has particular
benefit to 3C, feel free to let us know, and we may assist you in
getting your contribution accepted upstream and/or ensure it is merged
quickly to CCI's repository.

If the previous paragraph does not apply, just submit a pull request
to CCI's repository. You must grant the same license on your
contribution as the existing codebase. We do not have a formal
contributor license agreement (CLA) process at this time, but we may
set one up and require you to complete it before we accept your
contribution. Also be aware that we need to keep 5C ([our proprietary
extension of
3C](README.md#what-3c-users-should-know-about-the-development-process))
working, so you may have to wait for us to address 5C-specific
problems arising from your 3C pull request and/or we may ask you to
make specific changes to your pull request to accommodate 5C's code.

At the appropriate time during development of a pull request, please
run the [regression tests](development.md#regression-tests) and
correct any failures. (For example, it may not make sense to do this
on a draft pull request containing an unfinished demonstration of an
idea.) All regression tests must pass (or be disabled if appropriate)
before your pull request can be merged. If you're changing behavior
(as opposed to just cleaning up the code), we'll typically require you
to add or update enough tests to exercise the important behavior
changes (i.e., those tests fail before your code change and pass after
it). If there's a concern that your change might affect other cases
that are not adequately tested yet, we may ask you to add tests for
those cases as well.

See the [developer's guide](development.md) for additional information
that may be helpful as you work on 3C.
