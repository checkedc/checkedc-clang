// Tests for clang-specific tests of typechecking of Checked C
// extensions.  It includes clang-specific error messages as well
// tests of clang-specific extensions.
//
// The Checked C repo contains many tests of typechecking as part
// of its extension conformance test suite that also check clang error
// messages.  The extension conformance tests are designed to test overall
// compiler compliance with the Checked C specification.  This file is
// for more detailed tests of error messages, such as notes and correction 
// hints emitted as part of clang diagnostics.
//
// RUN: %clang_cc1 -verify -fcheckedc-extension %s

char fn41() : count(5); // expected-error {{expected 'fn41' to have a pointer return type}} // expected-error {{function with no prototype cannot have a return bounds}}

// Prototype of a function followed by an old-style K&R definition
// of the function.

// The Checked C specification does not allow no prototype functions to have
// return types that are checked types.  Technically, the K&R style function
// definition is a no prototype function, so we could say it is illegal.
// However, clang enforces the prototype declaration at the definition of
// f100, so this seems OK to accept.
_Ptr<int> f100(int a, int b);

_Ptr<int> f100(a, b)
     int a;
     int b;
{
}
