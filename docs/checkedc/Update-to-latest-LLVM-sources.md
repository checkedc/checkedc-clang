# Instructions for updating to the latest LLVM/clang sources

We are staying in sync with the LLVM/clang mainline sources.   The baseline branch is a pristine copy of  clang/LLVM sources. 
We periodically update the baseline branch and then push the changes to other branches

## Update your local baseline branches to the latest sources
To update the baseline branch to the latest sourcess, make sure you have personal forks of the Checked C LLVM and clang repos.
Clone these forks to your local machine.   Then create remotes to the mirrored Github repos that contain the updates sources 
for LLVM and clang. Go to your LLVM repo and do:

	git remote add mirror https://github.com/llvm-mirror/llvm

Set the upstream branch to the master llvm branch:

	git branch --set-upstream baseline mirror/llvm

You can then pull changes from the main repo into your local repo:

	git pull mirror baseline

Repeat the process for your clang repo:

	git remote add mirror https://github.com/llvm-mirror/clang
	git branch --set-upstream baseline mirror/clang
	git pull mirror  baseline


## Ensure the clang and LLVM sources are synchronized
The sources are being pulled from multiple repos that are mirrors of SVN repositories.  
The sources need for LLVM and clang may be out of sync - for example, the mirrors may not be in sync or a change may 
be checked in after pulling from one of the repos.

You need to make sure that source are in sync.   The Git mirror commits have the SVN change number embedded in them and the 
SVN change number is consistent across SVN repos for clang and LLVM.   You can examine that recent change log for clang and LLVM and find changes that are in
sync according to the SVN number  (there may be gaps in the numbering because a change may only affect on repo).     For each Git repo, note the Git commit (the first 8 or so digits of the hash)

For each Git repo, make *sure* that you change to the baseline branch:

	git checkout baseline

Then do

	git reset --hard commit-number,  where commit-number is the Git commit.

## Update the baseline branch on Github

You will then need to build and run tests to establish test baselines.   Assuming that the tests  results are good, 
you can push them to your personal Github forks:

	git push origin baseline

You can then issue pull requests to pull the changes into the Microsift Github repos.

## Update the master branch.

After you have updated the baseline branch, you can update the master branch. Change to each repo and then do:

	git checkout master
	git merge baseline

Set up the build system and compile.  Fix any issues that you encounter.  

Then run tests.  We have added tests for Checked C to the clang master branch, so these additional tests need to be taken
into account during testing.  Make sure the code passes the following tests:

- The same tests as the baseline branch, _plus_ the Checked C specific tests for clang in the master branch.
- The Checked C languages tests for the Checked C project.

Once the tests are passing, push the changes up to a personal Github fork and issue a pull request.
