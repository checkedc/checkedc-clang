# Process for Merging External Pull Requests

This document describes the detailed steps to be followed while reviewing and merging external pull requests submitted by developers outside of Microsoft. 

## Terminology

**Remote repository**: Any of the repositories on `GitHub` that are actively used/maintained by us, the Checked C team. For example, common ones are the `checkedc-clang` repository (https://github.com/microsoft/checkedc-clang), `checkedc` repository (https://github.com/microsoft/checkedc) and `checkedc-llvm-test-suite` repository (https://github.com/microsoft/checkedc-llvm-test-suite).

**External PR**: Developers outside of Microsoft may create a fork of any of our remote repositories, and participate in the development of the Checked C language and/or the compiler. As a part of this process, they may create a branch, say `Branch_A` on **their forks** of our remote repositories, make commits on branch `Branch_A` and submit a pull request to merge these commits from branch `Branch_A` to a branch (say, `master` branch) on our repository.  We denote such pull requests are "External PRs". Note that when developers submit external PRs, the external PR is available on our remote repository for operations such as review/pull/fetch/merge, but the branch on which the commits of the external PR were made (such as `Branch_A`) is not accessible to us.

**origin**: When this word occurs in a `git` command, it represents (is a name for) a remote repository (example: in the steps below **origin** represents https://github.com/microsoft/checkedc-clang).

## Detailed Steps

To make the details clear, the steps will walk through the merge of an example external PR numbered 123 from CCI on the `checkedc-clang` repository. 

1. Checkout the up-to-date `master` branch of the `checkedc-clang` repository on a local machine.

2. Fetch the commits comprising the external PR 123, creating a temporary branch (`test-CCI-PR-123`) using the following commands. Note that the branch `test-CCI-PR-123` is a branch on the copy of our remote repository (in this case, https://github.com/microsoft/checkedc-clang) on the local machine on which the commands are executed, even though PR 123 is an external PR.

  ```
   git fetch origin pull/123/head:test-CCI-PR-123
   git checkout test-CCI-PR-123
  ```

3. Review the commits of PR 123 either on the local machine or on the GUI of our remote repository. Here are some commands that may be helpful to see the diffs on command-line or for redirecting into a file. It may also be useful to merge commits from the `master` branch to restrict the diffs only to the changes in the external PR.

  ```
   git diff master..test-CCI-PR-123
   git diff --name-only master..test-CCI-PR-123   // To see the set of changed files
   git merge master    // To merge changes in master to the test-CCI-PR-123 branch
  ```

4.  The review may result in review comments, and the external developers may add more commits to PR 123 to address the review comments. These commits may be pulled as follows (assuming that the checked out branch on the local machine is `test-CCI-PR-123`):

  ```
   git pull origin pull/123/head
  ```

5. Push the temporary branch from the local machine to our remote repository (https://github.com/microsoft/checkedc-clang) and run the build and tests on the temporary branch.

  ```
   git push origin test-CCI-PR-123
  ```

6. Steps 3 and 4 above may need to be repeated until all review comments are addressed and test failures are fixed.

7. The last step is to merge PR 123 into the `master` branch of our remote repository (https://github.com/microsoft/checkedc-clang). Two points should be kept in mind:

	- The merge should either be a "merge commit" or a "squash merge" depending on the context of the PR. For example, "omnibus" 3C PRs from CCI ([example](https://github.com/microsoft/checkedc-clang/pull/1065)) should always be "merge commit" because they contain intermediate commits from CCI's `main` branch that need to stay in sync between CCI's and Microsoft's repositories after the merge. But for most PRs, the intermediate commits just reflect steps of work and code review that don't need to stay in sync with anywhere else, so a "squash merge" leaves a simpler history on the `master` branch for the benefit of most readers, and readers who want more detail can still view the intermediate commits on the PR page.
	- We should manually ensure that the commit id of the PR that gets merged is exactly the same as the commit id on which the most recent build and test executions were successful. This is to make sure that we avoid merging any unreviewed/untested changes that may compromise security.

8. After a successful merge of the external PR, the temporary branch (`test-CCI-PR-123` in the steps above) may be deleted.
