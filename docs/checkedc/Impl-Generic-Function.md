# Generic Type/Function

Generic Types and Functions are implemented with the idea to keep type
information to bind restrictions to void* usage.

## Type Variable Declaration Implementation

Type variables are declared inside `_For_any()` function specifier. As such,
type variables are parsed during function specifier parsing phase.

Type variables are constructed as a typedef, for its similiarity to
typedef. It also gives an advantage in implementation perspective as clang
already completely implemented typedef, including making sure that a newly
created typedef name is not present in the scope. 

## `_For_any` Scope

There are two issues that we must be careful with creating a scope for
`_For_any` specifier. First, `_For_any` scope must begin from the function
declaration (including return type) and end at semicolon (for simple
declaration) or end at the end of function body (for declaration with function
body). Second, the function name itself must be registered outside of `_For_any`
scope. Otherwise, when we go out of `_For_any` scope, the function no longer
exists. The function name must be registered after parsing function qualifiers/
specifiers in order to collect enough information about the function. Also,
the function name must be registered before the function body in case the
function is used as a recursive function. This means that the function name
must be registered to the parent "declaration" scope of `_For_any` scope, while
the `_For_any` scope is the current scope.

For the first issue, there are two exit points in Parser::ParseDeclGroup in 
ParseDecl.cpp where the code decides that they are done parsing function
declarations (without, and with function body). In both cases, we make sure that
the current scope we are in is created from `_For_any` specifier, and exit scope.

For the second issue, there are also two places where clang registers function
names. Clang registers function declaration without function body in
ParseDeclarationAfterDeclaratorAndAttributes in ParseDecl.cpp. Clang registers
function declaration with function body in ParseFunctionDefinition in Parser.cpp
In both cases, if the function has `_For_any` specifier, we register the 
function on the parent scope, since the current scope is `_For_any` scope.
