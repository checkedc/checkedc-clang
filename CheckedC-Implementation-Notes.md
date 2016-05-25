* Implementation Notes

This file describes high-level design decisions in the implementation of Checked C in LLVM.  
Checked C is controlled by a feature flag (-fcheckedc-extension).  The feature flag sets
a language extension flag.

** Processing Checked C

Lexing and parsing will recognize Checked C extensions only when the language extension 
flag  is enabled.   The IR and logic for other parts of clang are being generalized to handle 
the Checked C extension.  There is typically no need to guard the logic with the language
extension flag.  Checked C is backwards-compatible with C.  The IR has additional information
in it that represents either original C concepts or the Checked C extensions.  The processing 
uses this information.

** Lexing and parsing

Lexing only recognizes Checked C keywords when the language extension flag is enabled.  Parsing
of Checked C extensions currently depends on keywords being present.  These will not be seen
when the feature flag is disabled.  In the future, we expect to conditionalize a few
places in the parsing phase to recognize new syntax.

** Typechecking

Pointer types and array types are extended with information about whether the types are
checked.  For pointer types, an enum is used to represent the different kinds of pointers.
For array types, a boolean flag is used to represent whether the array type is checked
or not.  This information is used when determining type compatibility: two pointers types
are compatible only when their kinds are the same and two array types are compatible only
when they have the same checked property.
