# Instructions for updating to the latest LLVM/Clang sources

We are staying in sync with the LLVM/Clang mainline sources.   The baseline branch is a pristine copy of
LLVM/Clang sources.  We periodically update the baseline branch and then push the changes to other branches.

The first step is to create updated baseline branches:
1. Create new branches of your local baseline branches (we suggest creating new
branches so that you can run automated testing. You'll need new branches
that you can push to GitHub).
2. Update those branches to the latest sources.
3. Run testing on those branches to make sure things are stable.

The second step is to create updated master branches:
  1. Create branches of your updated baseline branches.
  2. Merge changes from the Checked C master branches into those branches.
  3. Fix merge conflicts and run testing.  You will likely need to fix some issues
     and re-run testing until all issues are fixed.

The third step is to merge your changes back into your baseline and master branches.

## Create updated branches of your baseline branches

First create remote to the mirrored GitHub repo that contains the updated sources
for LLVM/Clang. Go to your LLVM/Clang repo and do:

    git remote add mirror https://github.com/llvm-mirror/llvm-project

Then branch your baseline branch and merge changes into it:

    git checkout baseline
    git checkout -b updated-baseline
    git pull mirror master

## Run testing on your branched baseline branches.

You can run testing locally or push your branched baseline branches to GitHub
and use automated testing.  This will show you whether there are any unexpected
failures in your baseline branch.

If you use automated testing, make sure to clean the build directory first.
Enough changes will likely have accumulated that things may go wrong without doing
that first.

## Branch your new baseline branches and merge master changes

You can now branch your baseline branches to create a new master branch:

    git checkout -b updated-master
    git merge master

You will very likely have merge conflicts and some test failures.  The test
failures usually stem from incorrect merges or Checked C-specific data not being
initialized by new or changed constructor methods.

You may also need to pick up changes from LLVM/Clang for fixes to any unexpected
baseline failures.

You can push your updated master branches up to GitHub for automated
testing.  If you haven't cleaned the build directory as described earlier,
make sure you do that.

You'll want to run automated tests on Linux and Windows x86/x64, as well as
Linux LNT tests.  You may find in some cases that tests need to be updated
based on new warnings or slightly different compiler behavior.

## Merge your branched baseline and master branches

Once all testing is passing, you can merge your branches back into
your baseline and master branches.


    git checkout baseline
    git merge updated-baseline
    git checkout master
    git merge updated-master

## Push the updated branches to GitHub

The changes will be extensive enough that you don't want to do a pull request
on GitHub.  Just push the branches up to GitHub.

