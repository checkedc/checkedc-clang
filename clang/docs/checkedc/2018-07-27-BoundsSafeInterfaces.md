**Function Specifier for Bounds Safe Interfaces:**

*itype_for_any(<COMMA_SEPARATED_TYPE_VARIABLES>)*

* Creates a new scope
* Restrictions on parameters:
    * No type variable can be used as the type of function parameters
    * Type variables of parameters must be contained within itype clauses
        * e.g. 
            `itype_for_any(T)
                void* foo(void* arg1 : itype(_Ptr<T>));`
* There are no restrictions on local variables
    * i.e., Typevariables can be used as the type of local variables created within function body; even if the scope is unchecked. 
* itype_for_any and for_any scopes cannot be nested
* Design discussion with David MoM:
    * The bounds interface function will have a polymorphic type associated with it
    * If the caller doesn't apply a type, then the default type of the parameters will be applied as types
    * e.g
        * For the foo function shown above, the caller `foo<int>(x)` will force arg1 to be a pointer to int
        * But the caller `foo(y)` will treat arg1 as pointer to void type
        * This design is to enable foo to be called from unchecked scopes to allow incremental adoption
        

      