# Implementation Notes

This file describes high-level design decisions in the implementation of 
Checked C in LLVM/clang.

## Implementation plan

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
is no design, C++ specific code in LLVM/clang is not being extended
to handle checked extensions at this time. LLVM/clang will only allow
the Checked C extension to be used with C and will not allow it to
be used with C++, Objective C, or OpenCL.

## IR Extensions

### Types

Pointer types and array types are extended with information about whether the
types are checked.  For pointer types, an enum is used to represent the
different kinds of pointers.  For array types, a boolean flag is used to
represent whether the array type is checked or not.  This information is used
when determining type compatibility: two pointers types are compatible only
when their kinds are the same and two array types are compatible only when
they have the same checked property.

Function types are extended with optional bounds expressions for parameters
and the return value.  For parameters, the bounds expressions are recorded
in a variably-sized array allocated within the function type.  The array
is not allocated if there are no bounds expressions on parameters.

Bounds expressions are canonicalized for function types. References to
parameters are abstracted to the index of the parameter in the argument
list, so that names of arguments do not matter.

### Bounds expressions

Bounds expressions are represented using subclasses of the Expr
class.  This allows the usual machinery for expressions to be
reused.  There are three kinds of bounds expressions:

- Nullary bounds expressions.  These do not refer to any other
  expressions.  There are two kinds of nullary bounds expressions:
  None and Invalid.  Invalid represents bounds expressions that
  are invalid for some reason.  They are used to prevent errors
  from cascading during semantic processing.  Just ignoring an invalid
  bounds expressions could trigger downstream semantic checking errors.
- Count bounds expressions, which refer to a single expression
  that is the count.  There are two kinds of count bounds expressions:
  element count and byte counts.
- Range bounds expressions, which refer to lower and upper bound
  expressions.

There is an abtract base class `BoundsExpr` for bounds expressions.
A unified kind is stored on this base class.

Type annotations are also represented as a subclass of `BoundsExpr`.
They can appear syntactically where bounds expressions can apppear.
The type for the type annotation is stored as the type of the
type annotation expression.

### Other expressions

The `PositionalParameterExpr` class represents a reference to a parameter
that has been abstracted to the index of the parameter in an argument list.
This is used in the represention of function types with bounds expressions:
the bounds expressions are modified to use `PositionalParameterExpr`
instead of `DeclRefExpr`s that are pointing to `ParmVarDecl`.

For example, given

```
int f(_Array_ptr<int> arr : bounds(arr, arr + len), int len);
```

`arr` is given the positional index 0 and `len` is given the positional
index 1.

This makes type canonicalization of function types with bounds expressions
straightforward.  The comparion of parameters is easy: two parameters are the
same if their positional parameter expressions have the same index and type.
The current type canonicalization algorithm is used, and it is extended to
check that each bounds expression is identical.  Each bounds expression is
structurally compared, including any subexpressions of the bounds expression.

We could have modified the type canonicalization algorithm to handle
canonicalization of parameters specially for the case of function bounds
expressions. Parameter variables do have the positional index recorded.
However, this would make the IR confusing to understand. Programmers would
have to remember that parameters in bounds expressions behave specially, and
that the names need to be ignored.  It seemed better to encode this directly
into the IR and avoid a special case that is implicit and based on context.

The downside of extending the IR this way is that functions that do case-by-case
analysis of expressions might need to be extended, if they are applied to
bounds expressions in function types.  This seems likely to be rare.

Note that function types with bounds expressions are closed with respect to
local variables in scope at the definition of a function type.  They
cannot refer to them.  This includes parameters not declared by
the function type too.   The following code is illegal:

```
int f(_Array_ptr<int> arr : bounds(arr, arr + len), int len) {
 typedef int myfunc(_Array_ptr<int> arg : bounds(arr, arr + len));
 ...
}
```
The implication is that most dataflow analyses of variables
can ignore function types. An exception is dataflow analysis
analyses that are checking the correctness of bounds declarations.

### Declarations

A declaration may have an optional bounds expression.  The `DeclaratorDecl`
class is extended with an optional bounds expression.  The bounds expression
is meaningful for the following subclasses of `DeclaratorDecl`:

- Variable declarations (`VarDecl`), including parameter, local, and global variable
declarations.  The bounds declaration describes the bounds of values stored in a
variable.
- Function declarations (`FunctionDecl`). The bounds expression declares the return bounds of
of a function.
- Field member declarations (`FieldDecl`).  The bounds declaration describe the bounds
of values stored in a member.   The bounds expression may refer to other field members
in the structure or union type.

## Processing Checked C extensions

Processing of Checked C is controlled by a feature flag (`-fcheckedc-extension`).
The feature  flag sets a language extension flag.  Lexing and parsing will 
recognize Checked C extensions only when the language extension flag  is 
enabled.  There is typically no need to guard logic in other phases with the
language extension flag.  Checked C is backwards-compatible with C, so the IR
has additional information in it that represents either original C concepts or
the Checked C extensions.  The processing uses this information.

### Lexing and parsing

Lexing only recognizes Checked C keywords when the language extension flag is
enabled.  Parsing of Checked C extensions currently depends on keywords being 
present.  These will not be seen when the feature flag is disabled. In the 
future, we expect to conditionalize a few places in the parsing phase to 
recognize new syntax.
