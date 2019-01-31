# Instructions for updating to the latest LLVM/clang sources

We are staying in sync with the LLVM/clang mainline sources.   The baseline branch is a pristine copy of
LLVM/clang sources.  We periodically update the baseline branch and then push the changes to other branches

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

First create remotes to the mirrored GitHub repos that contain the updated sources
for LLVM and clang. Go to your LLVM repo and do:

	git remote add mirror https://github.com/llvm-mirror/llvm

Then branch your baseline branch and merge changes into it:

    git checkout baseline
    git checkout -b updated-baseline
    git pull mirror master

Repeat the process for your clang repo:

	git remote add mirror https://github.com/llvm-mirror/clang
    git checkout baseline
    git checkout -b updated-baseline
    git pull mirror master

## Ensure the clang and LLVM sources are synchronized

LLVM has unified its projects into one repo.  However, we have not migrated our
repos into one repo yet.   This means that our sources are being
pulled from partial mirrors of a unifed repo.   You need to make sure that the
changes are in sync. The pace of commits is fast enough that a change that
causes a build break could be introduced between syncing.


While the LLVM ecosystem is migrating, you can look at the SVN IDs in the
upstream git repo commits to see if you've gotten out-of-sync.  If the 
changes are not very close, for each of your branched baselines
)updated-baseline in the above example), use

	git reset --hard commit-number,  where commit-number is the Git commit.

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

You may also need to pick up changes from LLVM/clang for fixes to any unexpected
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

