# Instructions for updating to the latest LLVM/Clang sources

We are staying in sync with the LLVM/Clang mainline sources. The `baseline`
branch of the checkedc-clang repository is a pristine copy of LLVM/Clang sources,
and the `master` branch is LLVM/Clang sources plus the support for Checked C
language extensions. We periodically update the `baseline` branch and then push
the changes to the `master` branch.

In order to closely follow the LLVM/clang releases, the branching and merging
policies of the checkedc-clang repository are aligned with the versioning and
branching of LLVM/Clang mainline. LLVM/Clang releases are versioned as
`<major-version>.<minor-version>.<patch-version>`. LLVM/Clang source repository
has a branch called `main`, and several release branches (created off `main`)
called `release/<major-version>.x`, each corresponding to the major version
number of a release. New features go on the `main` branch. At an appropriate
commit on the `main` branch, a release branch is created. Bug fixes go on the
release branch and only some of these bug fixes get merged back into the `main`
branch. Typically, LLVM/Clang makes two final releases for each major version.

Next, we describe our approach for updating the checkedc-clang repository to
LLVM/Clang sources corresponding to the two releases for each major version,
for example, 12.0.0 and 12.0.1. The update is performed in two phases as
described below. This description is followed by detailed instructions for each
phase.

**Phase 1**: We update the `master` branch of the checkedc-clang repository up
to the commit hash on LLVM/Clang `main` branch at which the branch `release/12.x`
was created - we shall refer to this commit hash as
`<branch-point-of-12-on-main>`. Note that `<branch-point-of-12-on-main>` needs
to be the full commit hash and not just the 7-digit abbreviation. This phase
constitutes the major portion of the developer effort towards the merge.

**Phase 2**: We create a branch called `release_12.x` off the `master`
branch in the checkedc-clang repository. Then there are two sub-phases:
   - When we wish to make a Checked C compiler release based on the 12.0.0
   release of LLVM/Clang:
      - We first update the `release_12.x` branch (of the checkedc-clang
      repository) to the sources corresponding to the 12.0.0 release on the
      `release/12.x` branch of the LLVM/Clang repository.
      - Next, we merge Checked C features and bug fixes from the `master` branch
      to the `release_12.x` branch.
      - The release of the Checked C compiler happens from the `release_12.x`
      branch.
   - When we wish to make a Checked C compiler release based on the 12.0.1
   release of LLVM/Clang:
      - We update the same `release_12.x` branch (of the checkedc-clang
      repository) to the sources corresponding to the 12.0.1 release on the
      `release/12.x` branch of the LLVM/Clang repository.
      - Next, we merge Checked C features and bug fixes from the `master` branch
      to the `release_12.x` branch.
      - The release of the Checked C compiler happens from the `release_12.x`
      branch.
      

We do not expect substantial developer effort to be expended in Phase 2 as it is
highly unlikely that the LLVM/Clang bug fixes that get checked in on the release
branches will break Checked C build and tests. 

Note that Phase 1 and the sub-phases of Phase 2 may not be executed one after
another in one continuous stretch. They may be spread over several weeks and may
be interspersed with Checked C development on the `master` branch.

## Detailed Instructions - Phase 1

1. Create a new branch off the `baseline` branch in the checkedc-clang
repository (we suggest creating a new branch that you can push to github so that
you can run automated testing). Let us call this new branch `updated_baseline`.
```        
    git checkout -b updated_baseline baseline
```     
2. Update the branch `updated_baseline` to `<branch-point-of-12-on-main>` commit
hash on the `main` branch of the LLVM/Clang repository.
```
    git remote add upstream https://github.com/llvm/llvm-project.git 
    git pull upstream <branch-point-of-12-on-main>
```
3. Push the branch `updated_baseline` to the remote repository and run ADO tests
on it to make sure things are stable.
```
    git push origin updated_baseline
```
4. Merge the `updated_baseline` branch back to the `baseline` branch.
```
    git checkout baseline
    git merge updated_baseline
```
5. Create a new branch, say, `updated_baseline_checkedc` from the `baseline`
branch.
```
    git checkout -b updated_baseline_checkedc baseline
```
6. Merge the `master` branch of the checkedc-clang repository into the
`updated_baseline_checkedc` branch. This merge may cause several merge
conflicts and test case failures. The last section in this document gives
guidelines on fixing the merge conflicts and test failures.
```
    git checkout updated_baseline_checkedc
    git merge master
``` 
6. After all the merge conflicts and test failures are fixed, merge the
`updated_baseline_checkedc` branch into `master`.
TODO: In this step, we need to overwrite the `master` branch with the contents
of `updated_baseline_checkedc` branch. Need to check if we first need to merge
`master` into `updated_baseline_checkedc` with the `ours` strategy for
recursive merge, before executing the commands specified below.
```
    git checkout master
    git merge updated_baseline_checkedc
```

## Detailed Instructions - Phase 2
We give the instructions for the first sub-phase of Phase 2. Similar steps
have to be followed for the other sub-phase.

1. Create a new branch off the `master` branch in the checkedc-clang
repository, called `release_12.x`. If the branch already exists, then merge the
`master` branch into it. Resolve merge conflicts if any.
```        
    git checkout -b release_12.x master
    OR
    git checkout release_12.x
    git merge master
```     
2. Update the branch `release_12.x` to the LLVM/Clang sources corresponding to
the 12.0.0 release. For this, we need to get the commit hash associated with the
12.0.0 release from https://github.com/llvm/llvm-project/releases. Let this 
commit hash be <12.0.0-commit-hash>. Note that it should be the full commit hash.
Resolve any merge conflicts if any.
```
    git remote add upstream https://github.com/llvm/llvm-project.git
    git pull upstream <12.0.0-commit-hash>
```
3. Push the branch `release_12.x` to the remote repository, run ADO tests
on it and fix test failures to make sure things are stable.
```
    git push origin release_12.x
```
## Practical Tips

If you use automated testing, make sure to clean the build directory first.
Enough changes will likely have accumulated that things may go wrong without
doing that first.

The test failures usually stem from incorrect merges or Checked C-specific data
not being initialized by new or changed constructor methods.

You'll want to run automated tests on Linux and Windows x86/x64, as well as
Linux LNT tests.  You may find in some cases that tests need to be updated
based on new warnings or slightly different compiler behavior.

The changes will be extensive enough that you don't want to do a pull request
on GitHub.  Just push the branches up to GitHub.


## Handling Merge Conflicts and Test Failures

TBD
