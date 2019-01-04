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

The repesentation for type annotation causes some awkwardness in the IR.
We will likely move the type annotation to a separate field.   Currently,
there is no
way to represent a bounds-safe interface type with a bounds annotation.
This is needed for bounds-safe interface for pointers to
pointers, such as `int **`.  This type may have a bounds-safe interface
type of `array_ptr<ptr<int>>` and need bounds for the outer
`array_ptr`.
In addition, for parameters, clang
alters the types of the parameters when the types are array types
to be pointer types. We do not make this alteration for bounds-safe
interfaces types for parameters  (`itype(checked int[5])`,
for example).  The size of the array is used to infer bounds
annd altering the bounds-safe itnerface type would lose this
information. The fact that we do not make this alteration leads to
to special case code for handling bounds interface types for parameters.

### Parameters in bounds

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
can ignore function types. An exception is dataflow
analyses that are checking the correctness of bounds declarations.

### Temporary variables

The semantics of Checked C requires inferring bounds expressions for
expressions at compile time.  In some cases, the inferred bounds
must use the value produced at runtime by the evaluation of 
an expression.  We support this by selectively introducing temporary
variables into the AST to hold the results of evaulating expressions.

Note that we cannot just introduce assignments to synthesized variables
in the IR because C does not not
guarantee a precise order of evaluation for assignments.  It is undefined
behavior to assign to a variable and to read it within the same expression,
unless there is a sequence point between the assignment and read.  To
address this, we introduce temporary variables that have initialization
times controlled by the compiler.

Checked C temporary variables differ from temporary variables
introduced for C++ in the clang AST. For Checked C, we need to both
bind and *use* the temporary variable in other
expressions (i.e. bounds expressions).  The temporaries introduced
for C++ are more concerned about lifetime. Uses are implicit when the
temporaries go out of scope. For example, C++ introduces temporary variables for
object destruction.  When the temporary variable goes out of scope, the object
is destructed.

We have two operators for temporary variables: _binding_ and _use_:
- The `CHCKBindTemporaryExpr` expression binds a temporary variable to the
result of evaluating an expression.  The value of the binding
expression is the result of the expression evaluation.  The temporary
variable lives until the end of the evaluation of the top-level
expression containing the binding.  A top-level expression is
an expression that is not nested within another expression.
A temporary variable is only bound at most once in an expression. 
The temporary variable is only guaranteed to have a valid
value after the binding expression has been evaluated.  The
binding expression is subject to the same evaluation rules
as other C expressions.   

- The `BoundsValueExpr` expression reads the value of a temporary variable.
Its constructor takes a `CHKCBindTemporaryExpr` as an
argument.  We do not create a variable name for the
temporary expression.  The name used in diagnostic messages
is `value of e`, where `e` is the expression whose result is being computed
and bound to the temporary.  The `CHKBBindTemporaryExpr`
is not a subexpression of the `BoundsValueExpr`.  Instead, it
is similar to a declaration reference in a `DeclRefExpr`.

Temporary variables are introduced for string and array
literals.  They are also introduced for call expressions
where the bounds for the result of a call are described
in terms of the result.  This occurs when the called
function has return bounds that are count or
byte_count bounds expressions, or the return bounds
uses the `_Return_value` expression.

For string and array literals, the temporaries are introduced at
the conversion operation that represents the decay of the
array type to the pointer type.    

For example, `"abcd"` has the following AST:
```
ImplicitCastExpr ArrayToPointerDecay
  CHCKBindTemporaryExpr
    StringLiteral "abcd"
```
Let `x` be the AST object for `CHCBindTemporaryExpr`.
The bounds for the literal will be:
```
Bounds(Array-To-Pointer-Decay(x), Array-To-Pointer-Decay(x) + 4)
```
We tried always introducing temporaries for literals when the Checked C
extension is enabled, but that caused existing compiler code unrelated
to Checked C to break.  There are at least 20 places in clang that
assume the IR has a form where a string literal occurs
exactly as a subexpression of another expression. 

Because the temporary variables appear in bounds, and the bounds themselves
are attached to other nodes in the AST, we make sure that TreeTransform
keeps tempoary bindings and uses in sync.  The bindings are treated
the same way as declarations.

The design of the existing TreeTransform phase does not side-effect
existing expressions and statements. It creates new expressions
and statements.   If a Checked C temporary variable binds some expression e1
and e1 is transformed to e2, we introduce a new Checked C temporary
variable.  In the fragment of the AST that is being transformed,
we update any uses of the original temporary variable to the new
temporary variable.

There is an important difference in TreeTransform between
the treatment of temporary variables introduced
for Checked C bounds declarations and those
introduced for C++.  For C++, these temporaries are ignored during
TreeTransform and regenerated by semantic actions.  There are no explicit uses
to worry about. For Checked C, these temporaries are tracked and
renamed in the fragment of the AST being transformed.  Therefore these
variables should not be regenerated by semantic actions invoked by
TreeTransform. The Checked C temporaries should be introduced during the initial
construction of the AST during parsing.

We want to minimize the temporaries that are introduced into the AST
to avoid extensively changing the ASTs presented to later phases.  Extensive changes 
could cause existing compiler code to break when compiling C code with the Checked C
extension turned, but no use of Checked C extensions.  It could also lead
to unexpected performance differences, if the shape of the IR affected
downstream optimizations or code transformations.  This leads to a design
where during construction of the AST, we calculate whether a temporary variable
will be needed and insert  he temporary only if it is needed.   There have only 
been a few places where this calculation is needed, so this has not been disruptive
to AST construction.

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

### Bounds checks and bounds information for expressions.

Bounds checks are represented in the AST by attaching bounds expressions to some
lvalue-producing expressions:
- Dereference expressions. In the clang IR, these are represented as `UnaryOperator`
expressions.
- Array subscript expressions.   In the clang IR, these are represented
as `ArraySubscriptExpr` expressions.
- Member access expressions via the arrow operator.  Here the bounds check
represents the bounds for the base expression of the lvalue.

Bounds checks are attached to dereference and subscript operations
when:
1. The pointer operand to these expressions has type `array_ptr`.
2. The result of the expression is used to access memory or
in a member base expression.  The result is used to
access memory when it is used by the left-hand side of
an assignment or by `LValueToRevalue` cast.
The bounds checks ensure that the lvalue produced by the
expression is within bounds.

Bounds checks are attached to member access expressions involving the
arrow operator when the base operand has type `array_ptr`.

Bounds expressions are also attached to various cast operators.
For casts to `ptr` type, the bounds expressions are used during
static checking of the cast. For dynamic bounds cast operators,
the bounds expressions are used during dynamic checking of the
cast.

### Bounds-safe interface transitions
Declarations that have unchecked pointer types may have bounds-safe interfaces.
At uses of those declarations, rules specific to bounds-safe interfaces
apply:

- A value with checked type can be assigned to an lvalue with unchecked
type, provided that that the types are otherwise structurally identical 
and that the bounds-safe interface is satisfied.
- In a checked scope, the use should be retyped as though it only has
a fully checked type.

We represent these typing changes using implicit cast expressions
in the IR, as is done for assignment conversion.
We add a bit to `CastExpr` that tells whether a cast was inserted because
of bounds-safe interface rules.  These casts are trusted during
checking: normally we would not allow an implicit conversion from a
checked type to an unchecked type, for example.

At assignments, we insert a `bitcast` from the checked type
to an unchecked type, if the left-hand side of the assignment has
an unchecked type with a bounds-safe interface.  This is provided that
the bounds-safe interface type for the left-hand side
is compatible with the right-hand side type.

In checked scopes, the cast that is inserted depends on
whether the use is an lvalue or rvalue and the C type
pf the use.   For arrays and function types,
we do the casts as part  of `array-to-pointer ` type decay and
`function-to-pointer` type decay respectively. We did not want
to insert new casts underneath those operations.
For other types, the cast depends on whether the use
is an lvalue or an value.  For lvalues, we insert
an `lvalue bitcast` and for rvalues we use a plain
`bitcast`. 

In checked scopes, we compute the checked type to use as the target
of the cast in two steps: first, we compute a type based on
the bounds-safe interface for the declaration.  Then, we
recursively rewrite the type to replace any function types
that have bounds-safe interfaces on parameters or return values
with checked function types.

While `lvalue bitcasts` are ugly,
the nice thing about retyping uses at such a low level
is that the machinery for type checking and
inserting bounds checks for checked types just works
for checked scopes.  It avoids special cases that
lead to more complex code.


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
