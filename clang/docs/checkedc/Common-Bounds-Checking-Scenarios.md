# Common Scenarios for Bounds Checking

NOTE: The contents of this document are consistent with the behavior of the Checked C compiler as 
on September 16th, 2021. The compiler may have had improvements/changes/bug fixes added to it
since then.

This document lists some common code snippets to:
1) understand how bounds validation, bounds widening and where clauses must work
together to provide bounds safety, and
2) understand some of the issues that the Checked C compiler should address.

## Example 1: (motivated by network/inet_ntop.c in musl)
Has multiple warnings and errors.

The code in this example compresses strings that have contiguous sequences of blanks of length
greater than 2. For example, "Hello, &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; how are &nbsp; &nbsp; &nbsp; you?"
is compressed to "Hello, how are you?".

A note on the current status of the compiler support for Where clauses: 

As of now, the Checked C compiler only parses Where clauses but does not yet prove that they hold.

While the parsing of Where clauses is supported at null statements, assignment statements,
declarations and for function parameters, Where clauses that specify postconditions at function
declarations are not yet being parsed by the Checked C compiler. For example, a postcondition on the
function `strlen` can be specified as:
`size_t strlen(const char *s : itype(_Nt_array_ptr<const char>)) _Where s : count(_Return_value);`.
Here, the Where clause `_Where s : count(_Return_value)` is a programmer-specified postcondition that is
expected to hold true after every call to function `strlen`. The Checked C compiler should validate
the postcondition by checking that it holds at function exit in the definition of function `strlen`.
Further, the Checked C compiler should also validate a Where clause specified at a call to function
`strlen` (see the example below) using: a) the Where clause that is specified as the postcondition in the
declaration of function `strlen` and b) all other facts that hold true at the call to function `strlen`.

```
#include <string.h>

void compress (_Nt_array_ptr<char> buf : count(0)) {
    unsigned len = strlen(buf) _Where buf : count(len);
    unsigned i = 0;
    while (i < len) {
        // The fact i < len should be inferred.
        unsigned spn = strspn(buf + i, " ");
        unsigned end_spn = i + spn;
        if (spn > 2 && end_spn <= len) {
            // The facts spn > 2 and end_spn <= len should be inferred.
            unsigned long remaining = len - end_spn;
            _Nt_array_ptr<char> src : count(remaining) = buf + end_spn;
            memmove(buf + i + 1, src, remaining + 1);
            len = i + 1 + remaining _Where buf : count(len);
        }
        i++;
    }
}
```

## Example 2:

```
void foo (_Nt_array_ptr<char>p : count(0)) {
    unsigned len = 0;
    _Nt_array_ptr<char> t : count(len) = p;   // Cannot prove that bounds of t are valid.
                                              // declared bounds are 'bounds(t, t + len)'
                                              // inferred bounds are 'bounds(p, p + 0)'
    while (t[len]) {
        char x = t[len];
        len++ _Where t : count(len);
        if (t[len] && t[len] != x) 
            t[len] = x;
    }
}
```



## Example 3:

```
#include <stdlib_checked.h>

int extract_id (_Nt_array_ptr<const char> p : count(n), unsigned int n) {
     int id = -1; 
     if (*(p+n) == '_') {
           if (*(p+n+1) && *(p+n+2) && !*(p+n+3)) {
                 id = atoi(p+n+1); // cannot prove argument meets declared bounds for 1st parameter
                                   // expected argument bounds are 'bounds((const char *)p + n + 1, (const char *)p + n + 1 + 0)'
                                   // inferred bounds are 'bounds(p, p + n + 2 + 1)'
           }
     }
     return id;
}
```


## Example 4:

```
#include <stdlib_checked.h>

int extract_id (_Nt_array_ptr<const char> str : count(n), unsigned n) {
     int id = -1;
     if (*(str + n)) {
           if (*(str + n + 1) && *(str + n + 2)) {
                 _Nt_array_ptr<const char> p = str + n; // cannot prove declared bounds for 'p' are valid after initialization 
                                                        // declared bounds are 'bounds(p, p + 0)' 
                                                        // inferred bounds are 'bounds(str, str + n + 2 + 1)'
                 id = atoi(p);
           }
     }
     return id;
}
```

## Example 5: (Accessing elements of an inner array in a spatially safe way)
Has multiple warnings and errors.

```
#include <string.h>

// get the first name that starts with the string "Ar"
_Nt_array_ptr<char> get_name(_Array_ptr<_Nt_array_ptr<char>> names : count(n), unsigned int n)
_Checked {
  unsigned int m = 0;
  _Nt_array_ptr<char> name : count(m) = "";

  for (int i = 0; i < n; i++) {
    m = 0;
    name = names[i];
    m = strlen(name) _Where name : count(m);
    if (m >= 2 && name[0] == 'A' && name[1] == 'r')
      return name;
  }
  return "";
}
```
