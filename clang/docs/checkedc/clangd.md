# Checked C's `clangd`

[`clangd`](https://clangd.llvm.org/) is a language server that provides rich
support to IDEs for editing code in languages supported by Clang (jump to
definition/references, real-time display of errors and warnings, etc.). Since
this repository is a fork of the upstream Clang repository modified to support
Checked C, a copy of `clangd` built from this repository naturally provides some
support for editing Checked C code. Compared to a plain text editor, `clangd`
can dramatically improve the experience of working with Checked C code, and we
highly recommend trying it.

## Setting up `clangd`

1. Follow your usual build process for the Checked C compiler (for example,
   [this one](Setup-and-Build.md)), but in place of
   `-DLLVM_ENABLE_PROJECTS=clang`, use
   `-DLLVM_ENABLE_PROJECTS=clang;clang-tools-extra` (you may need to quote the
   semicolon from your shell). This is because `clangd` is in the
   `clang-tools-extra` "project".

2. Build the `clangd` target using your build tool.

3. Follow [the upstream `clangd` setup
   instructions](https://clangd.llvm.org/installation), but skip the "Installing
   clangd" section. Instead, configure your IDE extension to use the `clangd`
   executable in the `bin` subdirectory of your build directory. If you'll be
   working with non-Checked-C projects in the same IDE, we highly recommend
   making this setting specific to your Checked C project and using a normal
   `clangd` (which will be more stable) for the other projects. For example, if
   you're using [Visual Studio Code](https://code.visualstudio.com/), go to
   "Extensions" in the left toolbar -> "clangd" -> gear icon -> "Extension
   Settings" -> "Workspace" tab, and fill in the "Clangd: Path" field.

4. Enjoy the rich editing support! If `clangd` crashes repeatedly, try to narrow
   down the part of your code that is triggering the crash and [file a
   bug report](#contributing).

## Maintenance status

The Checked C team does not officially support `clangd` or do any development
specifically on `clangd` but is open to contributions and tries to keep the
`clangd` tests (the `check-clangd` target) passing. As of September 2021,
Correct Computation (CCI) (the sponsor of [3C](3C/README.md)) is providing some
maintenance for `clangd` (trying to keep at least the basics working in CCI's
preferred Linux environment) because of `clangd`'s value to users of 3C.

Experience so far is that:

- The Checked C extensions to the core C parsing and semantic analysis code
  sometimes have bugs in their use of internal Clang APIs that have no effect on
  the command-line compiler but cause problems for `clangd`, up to and including
  crashes. CCI's top priority for `clangd` is fixing common crashes of this
  nature.

- Syntax highlighting and real-time error/warning display mostly work.

- Jumping to definitions and references generally works in plain-C constructs
  but often does not work correctly in Checked-C-specific constructs because the
  necessary integration of the Checked-C-specific constructs with the APIs used
  by `clangd` has not been implemented.

### Contributing

High-priority `clangd` bugs may be reported to [Microsoft's
repository](https://github.com/microsoft/checkedc-clang) with a mention of
@mattmccutchen-cci (CCI's lead `clangd` maintainer as of September 2021). Pull
requests may also be submitted to Microsoft's repository. If you are making a
change to the core Checked C compiler code that affects `clangd`, it's fine to
add either a direct test of that code in its corresponding test directory or a
`clangd`-specific test under `clang-tools-extra/clangd/test` or
`clang-tools-extra/clangd/unittests`.
