* Implementation Notes

This file describes high-level design decisions in the implementation of 
Checked C in LLVM/clang.

** Implementation plan

Because Checked C is a backwards-compatible extension to C, the plan for
implementing Checked C in LLVM/clang is straightforward.  The IR for clang
will be extended to represent new features in Checked C.  The current 
phases in clang will be extended to process the extensions to the IR.
This includes the phases for lexing, parsing, semantic analysis and type 
checking, and  conversion to LLVM IR.  A new phase will be added to semantic
analysis after type checking to check bounds declarations.  It will follow 
the rules described in the Checked C specification.  Runtime checks for 
bounds will be inserted during conversion to LLVM IR.

There is no design for extending C++ with checking. Because there
is no design, C++ specific code in LLVM/clang is not being 
extended to handle checked extensions at this time.

** Processing extensions to the IR

Processing of Checked C is controlled by a feature flag (-fcheckedc-extension).
The feature  flag sets a language extension flag.  Lexing and parsing will 
recognize Checked C extensions only when the language extension flag  is 
enabled.  There is typically no need to guard logic in other phases with the
language extension flag.  Checked C is backwards-compatible with C, so the IR
has additional information in it that represents either original C concepts or
the Checked C extensions.  The processing uses this information.

** Lexing and parsing

Lexing only recognizes Checked C keywords when the language extension flag is
enabled.  Parsing of Checked C extensions currently depends on keywords being 
present.  These will not be seen when the feature flag is disabled. In the 
future, we expect to conditionalize a few places in the parsing phase to 
recognize new syntax.

** Typechecking

Pointer types and array types are extended with information about whether the
types are checked.  For pointer types, an enum is used to represent the 
different kinds of pointers.  For array types, a boolean flag is used to
represent whether the array type is checked or not.  This information is used
when determining type compatibility: two pointers types are compatible only 
when their kinds are the same and two array types are compatible only when 
they have the same checked property.
