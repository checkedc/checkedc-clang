# Instructions for updating to the latest LLVM/Clang sources

We are staying in sync with the upstream LLVM/Clang repository. The `baseline`
branch of the `checkedc-clang` repository is a pristine copy of LLVM/Clang
sources, and the `master` branch contains LLVM/Clang sources plus the support
for Checked C language extensions. We periodically upgrade the `baseline` branch
and the `master` branch to sources of the most recent releases from the upstream
LLVM/Clang repository.

The branching and merging policies of the `checkedc-clang` repository are
aligned with the versioning and branching of upstream LLVM/Clang repository in
order to closely follow the LLVM/Clang releases. LLVM/Clang releases are
versioned as &lt;major-version&gt;.&lt;minor-version&gt;.&lt;patch-version&gt;.
The LLVM/Clang repository has a branch called `main`, and several release
branches (created off `main`) called `release/<major-version>.x`, each
corresponding to the major version number of a release. New features go on the
`main` branch. At an appropriate commit on the `main` branch, a release branch
is created. Bug fixes go on the release branch and only some of these bug fixes
get merged back into the `main` branch. Typically, LLVM/Clang makes two final
releases for each major version.

Now we describe our approach for upgrading the `checkedc-clang` repository to
LLVM/Clang sources corresponding to the two releases for each major version,
for example, 12.0.0 and 12.0.1. The upgrade is performed in two phases as
described below. This description is followed by detailed instructions for each
phase.

**Phase 1**: We upgrade the `master` branch of the `checkedc-clang` repository
up to the commit hash on the `main` branch of the LLVM/Clang repository at which
the branch `release/12.x` is created - we shall refer to this commit hash as
&lt;branch-point-of-12-on-main&gt;. Note that &lt;branch-point-of-12-on-main&gt;
needs to be the full commit hash and not just the 7-digit abbreviation. This
phase constitutes the major portion of the developer effort towards the merge.

**Phase 2**: We create a branch called `release_12.x` off the `master` branch in
the `checkedc-clang` repository. Then there are two sub-phases:
   - When we wish to make a Checked C compiler release based on the 12.0.0
   release of LLVM/Clang:
      - We first upgrade the `release_12.x` branch (of the `checkedc-clang`
      repository) to the sources corresponding to the 12.0.0 release on the
      `release/12.x` branch of the LLVM/Clang repository.
      - Next, we merge Checked C features and bug fixes from the `master` branch
      to the `release_12.x` branch.
      - The release of the Checked C compiler happens from the `release_12.x`
      branch.
   - When we wish to make a Checked C compiler release based on the 12.0.1
   release of LLVM/Clang:
      - We upgrade the same `release_12.x` branch (of the `checkedc-clang`
      repository) to the sources corresponding to the 12.0.1 release on the
      `release/12.x` branch of the LLVM/Clang repository.
      - Next, we merge Checked C features and bug fixes from the `master` branch
      to the `release_12.x` branch.
      - The release of the Checked C compiler happens from the `release_12.x`
      branch.
      

We do not expect substantial developer effort to be expended in Phase 2 as it is
highly unlikely that the bug fixes on the release branches of the LLVM/Clang
repository will break Checked C build and tests. 

Note that Phase 1 and the sub-phases of Phase 2 may not be executed one after
another in one continuous stretch. They may be spread over several weeks and may
be interspersed with Checked C development on the `master` branch.

## Detailed Instructions - Phase 1

1. Create a new branch off the `baseline` branch in the `checkedc-clang`
repository. Let us call this new branch `updated_baseline`.
```        
    git checkout -b updated_baseline baseline
```     
2. Upgrade the branch `updated_baseline` to &lt;branch-point-of-12-on-main&gt;
commit hash on the branch `main` of the LLVM/Clang repository. Record the commit
hash &lt;branch-point-of-12-on-main&gt; in the document LLVM-Upgrade-Notes.md.
```
    git remote add upstream https://github.com/llvm/llvm-project.git 
    git pull upstream <branch-point-of-12-on-main>
```
3. Execute  LLVM/Clang tests on the branch `updated_baseline` on your local
machine to make sure that the upgrade is stable.

4. Merge the `updated_baseline` branch back to the `baseline` branch.
```
    git checkout baseline
    git merge updated_baseline
```
5. Create a new branch, say, `updated_baseline_master_12` from the `baseline`
branch.
```
    git checkout -b updated_baseline_master_12 baseline
```
6. Merge the `master` branch of the checkedc-clang repository into the
`updated_baseline_master_12` branch. This merge may cause several merge
conflicts and test case failures. After the merge command, git may suggest to
you to set merge.renamelimit to a specific value. It is a good idea to set it as
suggested and re-run the merge. The last section in this document gives
guidelines on fixing the merge conflicts and test failures.
```
    git checkout updated_baseline_master_12
    git merge master > merge_conflict_list.txt
``` 
7. The resolution of test failures may require you to cherry-pick commits from
the upstream LLVM/Clang repository. Record the cherry-picked commits in the
document LLVM-Upgrade-Notes.md. Let &lt;commit-to-cherry-pick&gt; be one
such commit hash on branch &lt;cherry-pick-branch&gt; of the LLVM/Clang
repository. 
```
    git fetch upstream <cherry-pick-branch>
    git cherry-pick <commit-to-cherry-pick>
```
8. While the merge conflicts and test failures are being fixed, it is possible
that the `master` branch has had several commits. Merge the `master` branch into
`updated_baseline_master_12` branch, and resolve the new conflicts and failures
if necessary.
```
    git merge master >> merge_conflicts_list.txt
```
9. After all the merge conflicts and test failures on the local machine are
fixed, push the `baseline` and `updated_baseline_master_12` branches to the
remote repository and excute the ADO tests on `updated_baseline_master_12`.
```
    git push origin baseline
    git push origin updated_baseline_master_12
```
10. After the ADO tests are successful, merge the `updated_baseline_master_12`
branch into `master` and push to the remote repository.
```
    git checkout master
    git merge updated_baseline_master_12
    git push origin master
```

## Detailed Instructions - Phase 2
We give the instructions for the first sub-phase of Phase 2. Similar steps
have to be followed for the other sub-phase.

1. Create a new branch, called `release_12.x`, from the `master` branch in the
`checkedc-clang` repository. If the branch already exists, then merge the
`master` branch into it. Resolve merge conflicts and test failures if any.
```        
    git checkout -b release_12.x master
    OR
    git checkout release_12.x
    git merge master
```     
2. Upgrade the branch `release_12.x` to the LLVM/Clang sources corresponding to
the 12.0.0 release. For this, we need to get the commit hash associated with the
12.0.0 release from https://github.com/llvm/llvm-project/releases. Let this 
commit hash be &lt;12.0.0-commit-hash&gt;. Note that it should be the full
commit hash. Resolve merge conflicts and test failures if any.
```
    git remote add upstream https://github.com/llvm/llvm-project.git
    git pull upstream <12.0.0-commit-hash>
```
3. Push the branch `release_12.x` to the remote repository, run ADO tests
on it and fix test failures to make sure that the branch is stable.
```
    git push origin release_12.x
```
## Practical Tips

1. If you use automated testing, make sure to clean the build directory first.
Enough changes will likely have accumulated that things may go wrong without
doing that first.

2. The test failures usually stem from incorrect merges or Checked C-specific data
not being initialized by new or changed constructor methods.

3. You'll want to run automated tests on Linux and Windows x86/x64, as well as
Linux LNT tests.  You may find in some cases that tests need to be updated
based on new warnings or slightly different compiler behavior.

4. The changes will be extensive enough that you don't want to do a pull request
on GitHub.  Just push the branches up to GitHub.


## Handling Merge Conflicts and Test Failures

1. When merge.renamelimit is set to a large value, the output of the `git merge`
command may have several lines like:
CONFLICT (rename/delete): ... in master renamed to ... in HEAD. Version HEAD ...
left in tree.
CONFLICT (rename/delete): ... in master renamed to ... in HEAD. Version master
... left in tree.

    If the version in HEAD is left in tree, it is very likely a valid change -
    the file may have been moved. If the version in `master` is left in tree,
    the files may be valid files that have been added as part of Checked C
    support:
    - `llvm/projects/checkedc-wrapper/lit.site.cfg.py`
    - `llvm/projects/checkedc-wrapper/lit.cfg.py`

    Others may need more investigation. For example, in the LLVM/Clang 11 to 12
    merge, two files which were reported by git as "Version master ... left in
    tree" were old versions lying around that needed to be deleted:
    - `clang/include/clang/StaticAnalyzer/Core/PathSensitive/SMTAPI.h`
    - `clang/include/clang/AST/TypeNodes.def`

2. While resolving merge conflicts, it is a good idea to have three reference
code snapshots to perform diffs of individual files to help distinguish between
checkedc-specific changes and llvm-11-to-12 changes:
    - The `master` branch of the `checkedc-clang` repository at the commit hash
  which was merged into `updated_baseline_master_12`.
    - The pristine LLVM/Clang source at &lt;branch-point-of-11-on-main&gt;
    - The pristine LLVM/Clang source at &lt;branch-point-of-12-on-main&gt;

3. When bitfields are modified (or need modification to accommodate changes
related to Checked C) in files like `clang/include/clang/AST/Stmt.h` (see
commit hash af12895f631), and `clang/include/clang/AST/DeclBase.h` (see the diff
between commits bc556e5 and e08e3f6 - this change was required for a previous
merge), it may need more manual investigation to check the correctness of the
changes.
