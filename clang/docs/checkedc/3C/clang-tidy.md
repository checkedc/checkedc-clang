# Use of `clang-tidy` on 3C

We're starting to use `clang-tidy` to automatically enforce some of the LLVM
style guidelines in our code. The main configuration is in
`clang/lib/3C/.clang-tidy`. There are a few existing warnings that we haven't
yet decided what to do about; please try to avoid adding more though.

We're currently using version 11.0.0 due to bugs in older versions, so if you
want to verify that our code is compliant with `clang-tidy`, you'll need to get
a copy of that version. If it isn't available from your OS distribution, you may
need to build it from source. This repository is currently based on a
pre-release of LLVM 9.0.0, so you can't use its version of `clang-tidy`.
However, Microsoft plans to upgrade it to LLVM 11.0.0 soon, at which point
you'll have the option to use `clang-tidy` built from this repository. It's
possible that using a newer, unstable version of `clang-tidy` (e.g., LLVM
master) might avoid more bugs, saving us the trouble of researching them, but we
doubt it's worth relying on a version that is unstable _and_ newer than what
Microsoft is about to upgrade to. This calculus might change when LLVM 12 is
released, expected around March 2021.

This file maintains a list of issues we've run into that you should be aware of
when using `clang-tidy`. Many of these could in principle be researched and (if
appropriate) reported upstream if it becomes a priority for us. Some issues lead
to bogus warnings we need to suppress, and those suppressions refer to the
relevant sections in this file.

## `_3C` name prefix

We use `3C` in the names of some program elements. Since an identifier cannot
begin with a digit, for names that would begin with `3C`, we decided that the
least evil is to prepend an underscore so the name begins with `_3C`. (A leading
underscore sometimes suggests that a program element is "private" or "special",
which is not the case here; this may be misleading.) One alternative we
considered was to use `ThreeC` at the beginning of a name, but that would be
inconsistent and would make it more work to search for all occurrences of either
`3C` or `ThreeC`. And we thought the alternative of using `ThreeC` everywhere
was just too clunky.

While the [LLVM naming
guideline](https://llvm.org/docs/CodingStandards.html#name-types-functions-variables-and-enumerators-properly)
does not mention leading underscores, its current implementation in the
`readability-identifier-naming` check disallows them, and this is clearly
intentional. There's no indication that the guideline has contemplated our
scenario, and we don't know whether the community would allow our scenario if
they knew about it. We raised the question in [a related bug
report](https://bugs.llvm.org/show_bug.cgi?id=48230) but would probably need to
raise it again in a more appropriate forum to get an answer. For now, we're
keeping the `_3C` prefix and suppressing the warnings, aware that we might have
to change our naming if our code is ever upstreamed to LLVM.

## Names referenced by templates

The `readability-identifier-naming` check may flag a name `N` that we can't
change because it is referenced by a template we don't control. Clearly, there's
nothing we can do but suppress the warning.

There are various ways the check could in principle be enhanced to avoid this
problem. For example, the check could expand all templates to find any
references, though then it's unclear where the name should be flagged if both
the definition and the template are under the user's control. Ideally, the
template would require a [C++20
"concept"](https://en.cppreference.com/w/cpp/language/constraints) and the
definition of `N` would state that it implements the concept (there's not yet a
standard way to do this, though there are some [popular
hacks](https://www.cppfiddler.com/2019/06/09/concept-based-interfaces/)), so all
Clang-based tools (not just the `readability-identifier-naming` check) would
treat the `N` in both the definition and the template as references to the `N`
in the concept. However, all of these potential solutions seem far off from
implementation.

The `readability-identifier-naming` check does have an exemption when `N` is a
method and a superclass also has a method named `N`, even if our `N` is not a
virtual override. This exemption is designed for scenarios where the superclass
provides a default implementation of the method, as `RecursiveASTVisitor` does.
However, it does not apply to other templates such as `GraphWriter`.

## Runs of uppercase letters in names

Some of our names have "runs" (our term for an undocumented concept in the
`readability-identifier-naming` check) of two or more uppercase letters, which
typically represent initialisms; an example is `TRV` in `DeclWriter.cpp`. (An
uppercase letter followed by a lowercase letter or a digit is considered part of
the next word, not the run; for example, in `getRCVars` in
`3CInteractiveData.cpp`, the run is `RC`.) The `readability-identifier-naming`
check allows such runs, but if a name fails the check due to any other problem,
then the proposed fix "normalizes" the run by lowercasing all letters after the
first (in addition to fixing the other problem, whatever it was). For example,
`getRCVars` passes the check for a function name, but `GetRCVars` fails because
the first letter is uppercase and gets changed to `getRcVars`.

Normalizing well-known initialisms can improve readability (e.g., `ID` -> `Id`),
while for certain other runs, there are good arguments against normalization
(for example, `getRCVars` should remain consistent with `getSrcCVars`).
Unfortunately, between some names with runs never getting flagged and other runs
getting normalized without us paying too much attention, our code is currently a
mishmash. We're making a mild effort not to make it any worse. If a run gets
normalized and you think it shouldn't be, you're welcome to change it back. It
won't by itself cause the name to be flagged, so you won't need to add a
suppression.

## Things to know about `clang-tidy --fix`

- Since many of the edits made by `clang-tidy --fix` can affect formatting in
  one way or another (e.g., changing the length of anything can affect line
  wrapping or the column at which other syntax should be aligned), it's
  generally a good idea to run `clang-format` after `clang-tidy --fix`.

- If two warnings have textually overlapping fixes, `clang-tidy --fix` will skip
  applying one of them with the message, "note: this fix will not be applied
  because it overlaps with another fix". `llvm-else-after-return` is one of the
  worst offenders because it is implemented naively by deleting the entire
  `else` block and reinserting the content of the block, rather than just
  deleting the `else`, `{`, and `}` tokens (as applicable). Thus, for example,
  if an element flagged by `readability-identifier-naming` has even a single
  reference inside an `else` block being unwrapped by `llvm-else-after-return`,
  the element may not get renamed at all. Therefore, it may make sense to run
  `clang-tidy --fix --checks='-*,llvm-else-after-return'` as many times as
  needed (see the next point) before running the full set of checks.

- `llvm-else-after-return` often needs multiple passes to fix everything. We've
  seen several ways this can happen:

  - Since `llvm-else-after-return` turns an if-else statement into two
    statements (the `if` branch followed by the unwrapped `else` branch), it
    doesn't trigger in a context that accepts only one statement, such as the
    `else` branch of an outer if-else statement without braces. If the outer
    `else` branch gets unwrapped by `llvm-else-after-return`, it may then become
    possible to unwrap the inner one. In a long if-else chain in which each
    block returns, it will take one pass to remove each `else`.

  - In a scenario like the previous one, if the outer `else` branch has braces,
    the inner `else` branch will be flagged as ready for unwrapping but still
    may not be unwrapped in the same pass due to overlap, as previously
    described.

  - Unwrapping an `else` branch may leave an unconditional return in an outer
    `if` branch that then makes it possible to unwrap the outer `else` branch.

- `readability-identifier-naming` may fail to update a reference to an element
  that it renamed. It turns out that the `--fix-errors` option not only makes
  `clang-tidy` proceed in the presence of compile errors but also makes it fix
  certain compile errors, including certain broken references, so a subsequent
  `clang-tidy --fix-errors` pass may be helpful.

- `readability-identifier-naming` may introduce a name collision. This may lead
  to an error on one of the colliding definitions, an error on a reference that
  now resolves to an element that doesn't make sense in context, or possibly
  just incorrect runtime behavior.

- `llvm-else-after-return` edits the text in a naive way that may leave
  undesired patterns of whitespace. `clang-format` fixes many of these cases but
  sometimes leaves formatting that is valid but not what we want. We've seen
  blank lines added and a comment from an `else` block moved to the same line as
  the closing brace of the `if`.
