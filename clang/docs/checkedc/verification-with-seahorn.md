# Verification of Unchecked codes with Seahorn

Our goal is to verify whether unchecked functions violate their bounds-safe interface or not. We use [Seahorn](https://seahorn.github.io) tool as a backend verifier. Seahorn is a software verification framework. It can verify user-supplied assumptions and assertions, as well as a number of built-in safety properties. We replace Seahorn's front-end with Checked C clang to be able to process the bounds information and automatically insert necessary assertions to generate proper verification conditions.

To verify whether the unchecked functions violate their bounds-safe interface we follow these two steps:
1. Add bounds to unchecked pointers to have dynamic checks at pointer dereferences
2. Inject verifier sink functions (bad states) at dynamic check points

We generate the LLVM bit code of the function that we are analyzing with the injected sink functions (`__VERIFIER_error`). We then pass the bit code to Seahorn. These are the main assumptions that we consider:
* For each argument:
    1. The function is not called with a non-NULL value for the pointers
    2. The bounds lengths on argument pointers are non-negative
* Function calls within the unchecked body:
    1. Functions that are being called have bounds-safe interface as well

For example, for this bounds information: `int *a : count(n)`, we will have: `assume(a != NULL); assume(n >= 0);`. The assumption on function calls having bounds-safe interface is needed to be able to have enough bounds information to propagate over the pointer assignments.

## Installing Seahorn

The following steps are quick listing for installing Seahorn. For more detailed instructions please refer to Seahorn [github page](https://github.com/seahorn/seahorn).

1. `git clone https://github.com/seahorn/seahorn.git`
2. `cd seahorn; mkdir build; cd build`
3. `cmake -DCMAKE_INSTALL_PREFIX=run -DCMAKE_BUILD_TYPE=Release ..`
4. `cmake --build .`
5. `cmake --build . --target extra && cmake ..`
6. `cmake --build . --target crab && cmake ..`
7. `cmake --build . --target install`

The executables and scripts will be in `seahorn/build/run/bin`.

## Running Seahorn

The main script to launch Seahorn and configuring its pipelines is `sea`. You can see the available command by `sea -h`. The main verification pipeline can be run with `sea pf file.c`. This is a shortcut for running the _Front End_ and _Solving Horn constraints_ (`fe | horn --solve`). The output will be either `sat` or `unsat`. `unsat` means that starting from the safe assumptions, there is no way for the program to get to bad states, defined by assertions (assertions will not fail). In other words, the program is safe. For example, for the code below, that is annotated with the assumptions and assertions, running `sea pf file.c` will return `unsat`.
```c
#include "seahorn/seahorn.h"
extern int non_deterministic_value();

int main() {
    int x = 1, y = 1;
    int x_old = x, y_old = y;
    int n = non_deterministic_value();
    while (n--) {
        int t1 = x;
        int t2 = y;
        x = t1 + 1;
        y = t2 - 1;
    }
    sassert(x_old + y_old == x + y);
    return 0;
}
```


## The Checked C clang compiler and Seahorn

As mentioned above, we replace the front-end of Seahorn with Checked C, to use the bounds information and inject necessary assertions for the verification. The `sea fe` (Front End) is a shortcut for _clang_, _Pre processing_, _Mixed Semantics_ and _Compiler Optimization_ (`clang | pp | ms | opt`).

Checked C will inject calls to verifier functions when `-finject-verifier-calls` is given. Note that this flag will generate a bitcode that does not link, because the calls do not have definitions. To enable adding bounds and consequently dynamic checks for unchecked pointers, `-funchecked-pointers-dynamic-check` flag should be given.

We simply use Checked C clang instead of clang in the front-end pipeline to have the desired verification conditions. The following script shows an example pipeline. Each of the steps can be further configured with command line arguments.
```bash
INC= # /path/to/seahorn/build/run/include
clang -c -emit-llvm -finject-verifier-calls -funchecked-pointers-dynamic-check -O0 -m64 -I $INC file.c -o file.bc
sea pp file.bc -o file.pp
sea ms file.pp -o file.ms
sea opt file.ms -o file.opt
sea horn --solve file.opt
```

To simplify the process, one can point `clang {args}` to be an alias that runs `clang -finject-verifier-calls -funchecked-pointers-dynamic-check {args}`. Then the pipeline above simply becomes: `sea pf file.c`. Note that this alias should only be used for verification runs, as the given flags should not be used in normal compilation of a Checked C code.

The following code is a simple example of an unchecked function with bounds-safe interface that we can verify with Checked C+Seahorn.

```c
#include <stdio.h>
#include "seahorn/seahorn.h"


int foo(int* a : count(n), int n);
int foo(int* a, int n)
{
    assume(a != NULL);
    assume(n > 0);

    int i = 0, s = 0;

    for( i=0; i<n; i++ )
        s += a[i];

    return s;
}
```

## Note on Array Bounds Instrumentation

Seahorn has a builtin method for instrumenting checks for array bounds and propagating those information. In short, it introduces two shadow variables `size` and `offset` for each pointer, and adds assertions of `p.offset >= 0` and `p.offset < p.size` for a dereference of `*p`. This mode can be activated by `sea abc`. However, for our use case, this mode is not enough. Because the bounds introduced there can propagate information starting from some concrete information, rather in our case, we might be interested in verifying a function that has variable bounds (like the example above: `[a, a+n)`). In that case we need bounds information that is available in Checked C, and is not read by array instrumentation.

