# Process for Releasing Checked C Clang

This document describes the steps to be followed while making a release of Checked C Clang. All the steps refer to an example release of Checked C Clang based on LLVM/Clang version 11.1.0 .

1. Make sure that all builds and tests are successful on the release branch. For our example, this branch is `release_11.x`.

2. The artifacts that comprise the release are, at present:
  1. The Checked C Clang compiler executables for Windows 32 and 64.
  2. The version of the Checked C specification that accompanies the released compiler.

3. The release tag and name has the format `CheckedC-Clang-<llvm-version>-rel<checkedc-release-number>`.  For our example, the release tag and name is `CheckedC-Clang-11.1.0-rel1`. Note that `<llvm-version>` represents the version of LLVM/Clang on which the current Checked C compiler release is based, and `<checkedc-release-number>` is to identify different releases of the Checked C compiler based on the same LLVM/Clang version.

4. Download (or collect) the signed release binaries. 

5. Build the Checked C language specification document after updating its version, if necessary.

6. Go to the [release page](https://github.com/Microsoft/checkedc-clang/releases) of the `checkedc-clang` repository and click on `draft a new release`. The release tag for the current release has to be specified as determined in step 3. NOTE: When specifying the release tag, make sure that the release tag gets applied to the correct release branch (in the current running example, `release_11.x`) by selecting the correct branch name in the drop-down list of branches.

7. Update the release notes for the current release in this draft, guided by previous release notes. Also upload the release artifacts. Some points to note:

  1. The release note should mention the version of the Checked C language specification that the released compiler is based on as this version number if not available in the name or tag associated with the release.
  2. The release note should mention the release date and tag.
  3. The bug fixes and the new features mentioned in the release note must be relative to the previous release. Moreover only bug fixes and PRs relevant to users should be part of the release notes. Specifically, bugs and PRs created for internal tracking should not be part of the release notes.

8. Once the release note is published, request another team member to download the release artifacts and sanity-check them.

9. Similar to the release made in the `checkedc-clang` repository, a release has to be made in the `checkedc` repository also. The only artifact in this release is the Checked C specification. It should be the same as the one released through the `checkedc-clang` release (i.e. both the releases must be in sync). The same tag as in step 3 must be applied to the `master` branch of the `checkedc` repository.

10. Apply an annotation tag (same tag as in step 3) on the `master` branches of both `checkedc-automation` and `checkedc-llvm-test-suite` repositories using the command below (assumes that the `master` branch of both these repositories is checked out and it is at exactly the same revision that was used to test the release):
  
```
    * git tag -a CheckedC-Clang-11.1.0-rel1

    * git push --tags

    * git tag -n (To see previous tags and their comments)
```
