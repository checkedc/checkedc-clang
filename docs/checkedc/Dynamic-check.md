# Dynamic Checks

Checked C's `_Dynamic_check` operator is implemented within the compiler,
because we need to do static analysis on its argument to check it is a
non-modifying expression (NME).

To avoid having to extend the lexer/parser, we have implemented it as a
Clang Builtin, which still gives us all the later hooks we need.

## Semantic Analysis - NME

The check for non-modifying expressions is yet to be implemented.

## Code Generation

For each dynamic check, we need to check a condition, and then,
if the check fails, stop the program. If the check succeeds,
the program will continue.

We insert a new basic block for each check. This block contains
only a `llvm.trap` intrinsic call, which LLVM's code generation will
turn into either `abort()`, or an instruction that does the same.
We try to insert these basic blocks at the end of the function so that
the generated code is easier to understand. There is no sharing of these
blocks so that dynamic check failures are easier to debug.

We also have to start a new basic block for when the check passes,
which contains all the code after the check is finished. This is emitted
directly after the conditional branch in the check.

An example function with two dynamic checks is below.
In LLVM IR, the things to look for are `br` for branch, `icmp` for
integer comparison, and `call` for function calls.

```
define void @f1(i32* %i) #0 {
entry:
  %i.addr = alloca i32*, align 4
  store i32* %i, i32** %i.addr, align 4
  %0 = load i32*, i32** %i.addr, align 4
  %cmp = icmp ne i32* %0, null
  %conv = zext i1 %cmp to i32
  %_Dynamic_check_result = icmp ne i32 %conv, 0
  br i1 %_Dynamic_check_result, label %_Dynamic_check_succeeded, label %_Dynamic_check_failed

_Dynamic_check_succeeded:                         ; preds = %entry
  %1 = load i32*, i32** %i.addr, align 4
  %cmp1 = icmp eq i32* %1, null
  %conv2 = zext i1 %cmp1 to i32
  %_Dynamic_check_result4 = icmp ne i32 %conv2, 0
  br i1 %_Dynamic_check_result4, label %_Dynamic_check_succeeded5, label %_Dynamic_check_failed3

_Dynamic_check_succeeded5:                        ; preds = %_Dynamic_check_succeeded
  ret void

_Dynamic_check_failed:                            ; preds = %entry
  call void @llvm.trap() #1
  unreachable

_Dynamic_check_failed3:                           ; preds = %_Dynamic_check_succeeded
  call void @llvm.trap() #1
  unreachable
}
```