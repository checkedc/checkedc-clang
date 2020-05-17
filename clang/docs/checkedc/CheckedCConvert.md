# Checked-C-Convert


This is a clang tool that tries to automatically convert `C` code to include `Checked
C` types.

The tool is available under: `tools/checked-c-convert`

## Redesigning and new features

Corresponding pull request: [Porting Tool Updates](https://github.com/microsoft/checkedc-clang/pull/642)

In this pull request, we tried to faithfully implement the highlevel idea as described in the paper (Section 5.3 of [https://www.microsoft.com/en-us/research/uploads/prod/2019/05/checkedc-post2019.pdf](https://www.microsoft.com/en-us/research/uploads/prod/2019/05/checkedc-post2019.pdf))

If you see the Section 5.3 of the paper, there depending on how a parameter to a function is **used** by the callers and **used** inside the callee, we use `itype`s or casting. To do this, we need to have two sets of constraint variables for each of the function parameters i.e., one for the calleers (i.e., as seen outside the function) and one for the function body. Once we solve the constraints we can assign `itype` or casts. Similarly for return types.

However, our old implementation had inconsistencies in inferring checked types of the function parameters and returns. Specifically, when a function do not have a corresponding declaration e.g., `static` methods, 

We changed this so that irrespective of whether a function has a explicit declaration or not we always maintain two sets of constraint variables for parameters and returns.

### Function Subtyping
We introduced the support for `_Nt_array_ptrs`, this resulted in function subtyping issues as discussed in detail in: [https://gist.github.com/Machiry/962bf8c24117bc5f56a31598e6782100](https://gist.github.com/Machiry/962bf8c24117bc5f56a31598e6782100)
We implemented support for this.

### Performance Improvements

We made performance imporvements to the tool. This is mainly because of a single line change (Thanks to @rchyena) in `ConstraintVariables.cpp`:
from:
```
214: auto env = CS.getVariables();
```
to
```
214: auto &env = CS.getVariables();
```
#### Numbers for Icecast :
**New**
```
real    3m51.869s  
user    3m50.132s  
sys    0m0.804s
```
**Old**
```
real    6m9.581s  
user    6m0.688s  
sys    0m8.004s
```
In general the current version of the tool is sufficiently fast, for `libarchive` (~170K lines) it took ~10min. 

