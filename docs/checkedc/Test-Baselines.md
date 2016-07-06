# Test base lines

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

## Differences in test results between branches

The  master branch in the checkedc-clang repo has additional tests for
Checked C.  It is expected that the master branch will have more
`Expected Passes` than the baseline branch.

## In-progress baseline updates

This section records the test results for an in-progress update to latest
sources in the baseline branch.  It is currently empty because no update
is in-progress.
