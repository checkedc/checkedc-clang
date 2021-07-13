//=--3C.h---------------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Definitions supporting a code pattern for functions that take _named_
// optional parameters. For functions with multiple optional parameters, this
// pattern tends to be better than C++'s default arguments, which are positional
// and have the limitation that if a call site specifies a value for one
// argument, it must also specify values for all previous arguments.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_3C_OPTIONALPARAMS_H
#define LLVM_CLANG_3C_OPTIONALPARAMS_H

// The basic ideas of the code pattern, for a function `foo`:
//
// 1. Define a struct `FooOpts` to hold the optional "parameters", and use
//    default member initializers to specify the desired default values.
// 2. Declare `foo` to take a final parameter `const FooOpts &Opts = {}`. (The
//    default argument `{}` corresponds to a `FooOpts` with all the default
//    values.)
// 3. In the body of `foo`, use the `UNPACK_OPTS` macro to copy the fields of
//    `Opts` to local variables for convenient access.
// 4. At call sites, use a macro to generate an immediately invoked lambda that
//    generates a `FooOpts` instance with the desired values. We could
//    alternatively use designated initializers if they were supported in our
//    target C++ standard.
//
// In two places in the current pattern, optional parameters are simply copied
// by value, which gives rise to several limitations: we have to use pointers
// (`T *`) instead of references (`T &`), and there may be a performance cost to
// the copying. We may be able to overcome these limitations in the future with
// additional tricks such as using wrapper objects with an overloaded `=`
// operator for the members of `FooOpts`, but it doesn't seem worth it yet (as
// of 2021-07-09).
//
// Further design discussion:
// https://github.com/correctcomputation/checkedc-clang/issues/621
// https://github.com/correctcomputation/checkedc-clang/pull/638
//
// A simple example is included at the bottom of this file, after the
// implementation. That way, you can explore it in your IDE by changing the `#if
// 0` to `#if 1`, at the cost of having to scroll down to reach it. For an
// example of an actual use, see ConstraintVariable::mkString.

// IMPLEMENTATION:

// This currently supports up to 6 optional parameters for a given function. It
// should be clear how to extend the pattern of macros to increase the limit.

// Our internal macro names are prefixed with `_OP_` (for "optional parameters")
// in an attempt to avoid accidental collisions with anything else in the LLVM
// monorepo.

// Helpers:

// Workaround for an MSVC bug where if a macro body passes __VA_ARGS__ to
// another macro (called M in this discussion), M receives a single argument
// containing commas instead of a variable number of arguments. For reasons we
// haven't researched, placing this wrapper around the entire macro body seems
// to avoid the problem.
//
// _OP_MSVC_VA_ARGS_WORKAROUND is needed only when M declares individual
// parameters and we need the arguments to be matched up with those parameters.
// If M just declares `...` and passes it on to another macro with __VA_ARGS__,
// it's generally harmless to let M receive a single argument containing commas;
// the argument will be correctly split up by _OP_MSVC_VA_ARGS_WORKAROUND later.
//
// We should be able to remove this workaround when the 3C Windows build moves
// to the "new" MSVC preprocessor. See
// https://developercommunity.visualstudio.com/t/-va-args-seems-to-be-trated-as-a-single-parameter/460154.
#define _OP_MSVC_VA_ARGS_WORKAROUND(_x) _x

#define _OP_GET_ARG7(_1, _2, _3, _4, _5, _6, _7, ...) _7
// Precondition: The number of arguments is between 1 and 6 inclusive.
//
// The `_dummy` is because according to the C++ standard, at least one argument
// must be passed to a `...` parameter
// (https://en.cppreference.com/w/cpp/preprocessor/replace).
#define _OP_COUNT_ARGS(...)                                                    \
  _OP_MSVC_VA_ARGS_WORKAROUND(                                                 \
      _OP_GET_ARG7(__VA_ARGS__, 6, 5, 4, 3, 2, 1, _dummy))

// Each _i argument is expected to be of the form `field = expr`, so we generate
// a statement of the form `_s.field = expr;` to set the struct field.
#define _OP_ASSIGN_FIELDS_1(_i1) _s._i1;
#define _OP_ASSIGN_FIELDS_2(_i1, _i2)                                          \
  _s._i1;                                                                      \
  _s._i2;
#define _OP_ASSIGN_FIELDS_3(_i1, _i2, _i3)                                     \
  _s._i1;                                                                      \
  _s._i2;                                                                      \
  _s._i3;
#define _OP_ASSIGN_FIELDS_4(_i1, _i2, _i3, _i4)                                \
  _s._i1;                                                                      \
  _s._i2;                                                                      \
  _s._i3;                                                                      \
  _s._i4;
#define _OP_ASSIGN_FIELDS_5(_i1, _i2, _i3, _i4, _i5)                           \
  _s._i1;                                                                      \
  _s._i2;                                                                      \
  _s._i3;                                                                      \
  _s._i4;                                                                      \
  _s._i5;
#define _OP_ASSIGN_FIELDS_6(_i1, _i2, _i3, _i4, _i5, _i6)                      \
  _s._i1;                                                                      \
  _s._i2;                                                                      \
  _s._i3;                                                                      \
  _s._i4;                                                                      \
  _s._i5;                                                                      \
  _s._i6;
#define _OP_ASSIGN_FIELDS(_count, ...)                                         \
  _OP_MSVC_VA_ARGS_WORKAROUND(_OP_ASSIGN_FIELDS_##_count(__VA_ARGS__))
// In _OP_ASSIGN_FIELDS, ## takes precedence over the expansion of the _count
// argument, so if we let PACK_OPTS call `_OP_ASSIGN_FIELDS(_OP_COUNT_ARGS(foo),
// bar)` directly, we'd end up with `_OP_ASSIGN_FIELDS__COUNT_ARGS(foo)(bar)`.
// _OP_ASSIGN_FIELDS_WRAP expands its _count argument before calling
// _OP_ASSIGN_FIELDS, working around the problem. See the second example on
// https://gcc.gnu.org/onlinedocs/cpp/Argument-Prescan.html.
#define _OP_ASSIGN_FIELDS_WRAP(_count, ...)                                    \
  _OP_ASSIGN_FIELDS(_count, __VA_ARGS__)

// PACK_OPTS currently doesn't support passing an empty list of fields to the
// `...`, because in the current usage for optional parameters, we expect the
// whole struct to be omitted if no optional parameters need to be passed.
#define PACK_OPTS(_type, ...)                                                  \
  ([&] {                                                                       \
    _type _s;                                                                  \
    _OP_ASSIGN_FIELDS_WRAP(_OP_COUNT_ARGS(__VA_ARGS__), __VA_ARGS__)           \
    return _s;                                                                 \
  })()

// `(void)_f` suppresses "unused variable" warnings: see the comment on
// LLVM_ATTRIBUTE_UNUSED in llvm/include/llvm/Support/Compiler.h.
#define _OP_UNPACK_OPT(_f)                                                     \
  auto _f = Opts._f;                                                           \
  (void)_f;
#define _OP_UNPACK_OPTS_1(_f1) _OP_UNPACK_OPT(_f1)
#define _OP_UNPACK_OPTS_2(_f1, _f2) _OP_UNPACK_OPT(_f1) _OP_UNPACK_OPT(_f2)
#define _OP_UNPACK_OPTS_3(_f1, _f2, _f3)                                       \
  _OP_UNPACK_OPT(_f1) _OP_UNPACK_OPT(_f2) _OP_UNPACK_OPT(_f3)
#define _OP_UNPACK_OPTS_4(_f1, _f2, _f3, _f4)                                  \
  _OP_UNPACK_OPT(_f1)                                                          \
  _OP_UNPACK_OPT(_f2) _OP_UNPACK_OPT(_f3) _OP_UNPACK_OPT(_f4)
#define _OP_UNPACK_OPTS_5(_f1, _f2, _f3, _f4, _f5)                             \
  _OP_UNPACK_OPT(_f1)                                                          \
  _OP_UNPACK_OPT(_f2)                                                          \
  _OP_UNPACK_OPT(_f3) _OP_UNPACK_OPT(_f4) _OP_UNPACK_OPT(_f5)
#define _OP_UNPACK_OPTS_6(_f1, _f2, _f3, _f4, _f5, _f6)                        \
  _OP_UNPACK_OPT(_f1)                                                          \
  _OP_UNPACK_OPT(_f2)                                                          \
  _OP_UNPACK_OPT(_f3)                                                          \
  _OP_UNPACK_OPT(_f4) _OP_UNPACK_OPT(_f5) _OP_UNPACK_OPT(_f6)
#define _OP_UNPACK_OPTS_IMPL(_count, ...)                                      \
  _OP_MSVC_VA_ARGS_WORKAROUND(_OP_UNPACK_OPTS_##_count(__VA_ARGS__))
// Ditto the comment on _OP_ASSIGN_FIELDS_WRAP.
#define _OP_UNPACK_OPTS_WRAP(_count, ...)                                      \
  _OP_UNPACK_OPTS_IMPL(_count, __VA_ARGS__)
// UNPACK_OPTS doesn't support an empty list of parameters: the UNPACK_OPTS call
// should just be omitted in that case.
#define UNPACK_OPTS(...)                                                       \
  _OP_UNPACK_OPTS_WRAP(_OP_COUNT_ARGS(__VA_ARGS__), __VA_ARGS__)

// EXAMPLE (change `#if 0` to `#if 1` to explore it in your IDE):

#if 0

struct FooOpts {
  std::string B = "hello";
  bool C = false;
};

#define FOO_OPTS(...) PACK_OPTS(FooOpts, __VA_ARGS__)

void foo(int A, const FooOpts &Opts = {}) {
  UNPACK_OPTS(B, C);
#if 0 // Avoid "duplicate local variable" errors in this illustration.
  // Expansion:
  auto B = Opts.B;
  // Suppresses an "unused variable" warning if FooOpts is shared by several
  // functions that take the same optional parameters (e.g., virtual overrides)
  // and some of those functions don't actually read all the parameters.
  (void)B;
  auto C = Opts.C;
  (void)C;
#endif

  llvm::errs() << "A is " << A << ", B is " << B << ", C is " << C << "\n";
}

void test() {
  foo(1);
  foo(2, FOO_OPTS(B = "goodbye"));
  // Expansion (notice how we conveniently concatenated `_s.` and the argument
  // `B = "goodbye"`):
  foo(2, ([&] {
        FooOpts _s;
        _s.B = "goodbye";
        return _s;
      })());

  foo(3, FOO_OPTS(C = true));
  foo(4, FOO_OPTS(B = "goodbye", C = true));
}

#endif // example

#endif // LLVM_CLANG_3C_OPTIONALPARAMS_H
