# Verification of Unchecked codes with Seahorn

Our goal is to verify whether unchecked functions violate their bounds-safe interface or not. We use [Seahorn](http://seahorn.github.io) tool as a backend verifier. Seahorn is a software verification framework. It can verify user-supplied assumption and assertions, as well as a number of built-in safety properties. We replace Seahorn's front-end with Checked-c clang to be able to process the bounds information and automatically insert necessary assertions to generate proper verification conditions.

To verify whether the unchecked functions violate their bounds-safe interface we follow these two steps:
1. Add bounds to unchecked pointers to have dynamic checks at pointer dereferences
2. Inject verifier sink functions (bad states) at dynamic check points

We generate the LLVM bit code of the function that we are analyzing with the injected sink functions (`__VERIFIER_error`). We then pass the bit code to Seahorn. These are the main assumptions that we consider:
* For each argument:
    1. The function is not called with a non-NULL value for the pointers
    2. The bounds lengths on argument pointers are non-negative
* Function calls within the unckecked body:
    1. Functions that are being called have bounds-safe interface as well

For example, for this bounds information: `int *a : count(n)`, we will have: `assume(a != NULL); assume(n >= 0);`. The assumption on function calls is needed to be able to have enough bounds information to propagate over the pointer assignments.



