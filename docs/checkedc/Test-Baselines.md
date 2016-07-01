# Test base lines

## Current baseline for the master branch

Here is the current baseline for testing just clang with the x86 target (using the check-clang project)

```
         Failing Tests (3):
             Clang :: Index/index-templates.cpp
             Clang :: Index/usrs.m
             Clang :: Lexer/eof-conflict-marker.c
```
```
           Expected Passes    : 8945
           Expected Failures  : 21
           Unsupported Tests  : 206
           Unexpected Failures: 3
```

Here is the current base line for testing LLVM + clang on x86 (check-all):
```
         Failing Tests (5):
             Clang :: Index/index-templates.cpp
             Clang :: Index/usrs.m
             Clang :: Lexer/eof-conflict-marker.c
             LLVM :: MC/AsmParser/macros-gas.s
             LLVM :: tools/llvm-objdump/malformed-archives.test
```
```
           Expected Passes    : 18787
           Expected Failures  : 97
           Unsupported Tests  : 6647
           Unexpected Failures: 5
```


The current base line for testing LLVM + clang on all targets (check-all) in the master
branch needs to be updated.

## Testing baseline for the base line branch


Here is the current baseline for testing just clang with the x86 target (using the check-clang project)

```
         Failing Tests (3):
             Clang :: Index/index-templates.cpp
             Clang :: Index/usrs.m
             Clang :: Lexer/eof-conflict-marker.c
```
```
           Expected Passes    : 8942
           Expected Failures  : 21
           Unsupported Tests  : 206
           Unexpected Failures: 3
```

Here is the current base line for testing LLVM + clang on x86 (check-all):
```
         Failing Tests (5):
             Clang :: Index/index-templates.cpp
             Clang :: Index/usrs.m
             Clang :: Lexer/eof-conflict-marker.c
             LLVM :: MC/AsmParser/macros-gas.s
             LLVM :: tools/llvm-objdump/malformed-archives.test
```
```
           Expected Passes    : 18779
           Expected Failures  : 97
           Unsupported Tests  : 6647
           Unexpected Failures: 5
```


Here is the current base line for testing LLVM + clang on all targets (check-all):
```
         Failing Tests (6):
             Clang :: Index/index-templates.cpp
             Clang :: Index/usrs.m
             Clang :: Lexer/eof-conflict-marker.c
             LLVM :: MC/ARM/directive-align.s
             LLVM :: MC/AsmParser/macros-gas.s
             LLVM :: tools/llvm-objdump/malformed-archives.test
```
```
           Expected Passes    : 24455
           Expected Failures  : 196
           Unsupported Tests  : 871
           Unexpected Failures: 6
```

## Delta between the master branch and baseline branch

We have added tests for Checked C to the master branch.  These tests are specific to clang, such as tests of
compiler internals or the driver.  We currently expect the master branch to pass the following additional tests
in these configurations.

- For just clang with the x86 target (using the check-clang project), 3 additional `Expected Passes` tests.
- For testing LLVM + clang on x86 (check-all), 8 additional `Expected Passes` tests


## In-progress baseline updates

This section records the test results for an in-progress update to latest sources in the baseline branch.  It is currently empty because
no update is in-progress.
