# Verification of Unchecked codes with Seahorn

[Seahorn](http://seahorn.github.io) is a software verification framework. This verifier can verify user-supplied assumption and assertions, as well as a number of built-in safety properties. Our goal is to verify whether unchecked functions violate their bounds-safe interface or not. We replace Seahorn's front-end with Checked-c clang to be able to process the bounds information and automatically insert necessary assertion to generate proper verification conditions.

To verify whether the unchecked functions violate their bounds-safe interface we perform two steps:
1. Add bounds to unchecked pointers to have dynamic checks at pointer dereferences
2. Inject verifier sink functions (bad states) at dynamic check points

We generate the LLVM bit code of the function that we are analyzing with the injected sink functions (`__VERIFIER_error`). We then pass the bit code to Seahorn. There are two main assumptions that we consider for each argument pointer:
1. The function is not called with a non-NULL value for the pointers
2. The bounds lengths are non-negative

For example, for this bounds information: `int *a : count(n)`, we insert: `assume(a != NULL); assume(n >= 0);`.

Another assumption that we make is that any function call within the function that we are analyzing is to a function that has bounds-safe interface. This assumption is needed to be able to have enough bounds information to propagate over the pointer assignments.
