// Regression test for a bug in which 3C didn't escape backslashes in file paths
// in its JSON output
// (https://github.com/correctcomputation/checkedc-clang/issues/619). On
// Windows, file paths contain backslashes as the directory separator, so the
// main json_formatting.c test would catch the bug. This test catches the bug on
// Linux and Mac OS X by manually creating a file with a backslash in the name
// (which is an ordinary filename character on these OSes).

// UNSUPPORTED: system-windows

// We reuse json_formatting.c to ensure that we test all the same code
// constructs, but we `#include` it instead of running 3c on it directly because
// some code used by 3c seems to be messing with backslashes in file names on
// the command line. The `#include` seems to preserve the backslash. (This is an
// unusual case, so it wouldn't be surprising if 3c's behavior changes at some
// point and this test breaks and we have to redesign it.)

// RUN: rm -rf %t*

// RUN: mkdir %t.alltypes && cd %t.alltypes
// RUN: cp %s .
// RUN: cp %S/json_formatting.c 'json\_formatting.h'
// RUN: 3c -alltypes -dump-stats -dump-intermediate -debug-solver -output-dir=out.checked json_formatting_backslash.c --
// RUN: %python -c "import json, glob; [json.load(open(f)) for f in glob.glob('*.json')]"

// RUN: mkdir %t.noalltypes && cd %t.noalltypes
// RUN: cp %s .
// RUN: cp %S/json_formatting.c 'json\_formatting.h'
// RUN: 3c -dump-stats -dump-intermediate -debug-solver -output-dir=out.checked json_formatting_backslash.c --
// RUN: %python -c "import json, glob; [json.load(open(f)) for f in glob.glob('*.json')]"

// Even though this looks like a double-quoted string, the Checked C compiler
// seems to require the backslash to _not_ be escaped. We choose `\_` because it
// is not a valid escape sequence in a JSON string literal (see
// https://www.json.org/).
#include "json\_formatting.h"
