// RUN: rm -rf %t*
// RUN: mkdir %t.alltypes && cd %t.alltypes
// RUN: 3c -base-dir=%S -alltypes -dump-stats -dump-intermediate -debug-solver %s
// RUN: python -c "import json, glob; [json.load(open(f)) for f in glob.glob('*.json')]"
// RUN: mkdir %t.noalltypes && cd %t.noalltypes
// RUN: 3c -base-dir=%S -dump-stats -dump-intermediate -debug-solver %s
// RUN: python -c "import json, glob; [json.load(open(f)) for f in glob.glob('*.json')]"

// Testing that json files output for statistics logging are well formed

int *a;
int *b(int *c);
static int *d() { return 0; }
void e(int *f, int len) { f[0]; }
