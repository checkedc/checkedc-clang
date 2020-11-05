# Contributing to 3C

Issues and pull requests related to 3C should be submitted to [CCI's repository](https://github.com/correctcomputation/checkedc-clang), not [Microsoft's Checked C repository](https://github.com/microsoft/checkedc-clang), except as stated below.

## Issues

Feel free to file issues against 3C, even during this early stage in its development; filed issues will help inform us of what users want.  Please try to provide enough information that we can reproduce the bug you are reporting or understand the specific enhancement you are proposing.  We strive to promptly fix any issues that block basic usage of 3C in our [supported environments](INSTALL.md#supported-environments).

## Pull requests

We are open to outside contributions to 3C, though we do not yet have a sense of how much work we'll be willing to spend on integrating them.  As with any open-source project, we advise you to check with us (e.g., by filing an issue) before you put nontrivial work into a contribution in the hope that we'll accept it.  Of course, you can also consider that even if we do not accept your contribution, you will be able to use your work yourself (subject to your ability to keep up with us) and we may even direct users who want the functionality to your version.

If your contribution does not touch any 3C-specific code (or is a codebase-wide cleanup of low risk to 3C) and you can reasonably submit it to [Microsoft's Checked C repository](https://github.com/microsoft/checkedc-clang) instead, we generally prefer that you do so.  If such a contribution has particular benefit to 3C, feel free to let us know, and we may assist you in getting your contribution accepted upstream and/or ensure it is merged quickly to CCI's repository.

If the previous paragraph does not apply, just submit a pull request here.  You must grant the same license on your contribution as the existing codebase.  We do not have a formal contributor license agreement (CLA) process at this time, but we may set one up and require you to complete it before we accept your contribution.  Also be aware that we need to keep 5C working, so you may have to wait for us to address 5C-specific problems arising from your 3C pull request and/or we may ask you to make specific changes to your pull request to accommodate 5C’s code.

## Testing

3C has a regression test suite located in `clang/test/3C`.  At the appropriate time during development of a pull request, please run it and correct any failures.  (For example, it may not make sense to run it on a draft pull request containing an unfinished demonstration of an idea.)  The easiest way to run it is to run the following in your build directory:

```
ninja check-clang-3c
```

This command will build everything needed that hasn't already been built, run the test suite, report success or failure (exit 0 or 1, so you can use it in scripts), and display some information about any failures, which may or may not be enough for you to understand what went wrong.

## Coding guidelines

Please follow [LLVM coding standards](https://llvm.org/docs/CodingStandards.html#name-types-functions-variables-and-enumerators-properly) in your code. Specifically:

* The maximum length of a line: 80 chars
* All comments should start with a Capital letter.
* All Local variables, including fields and parameters, should start with a Capital letter (similar to PascalCase). Short names are preferred.
* A space between the conditional keyword and `(` i.e., `if (`, `while (`, ``for (` etc.
* Space after the type name, i.e., `Type *K` _not_ `Type* K`.
* Space before and after `:` in iterators, i.e., `for (auto &k : List)`
