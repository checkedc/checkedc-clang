// Import the CheckedC/dump-dataflow-facts.c and ensure the same behaviour of
// the testcases though the dump formats are difference
// RUN: %clang_cc1 -fdump-available-facts -verify \
// RUN: -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s \
// RUN: | FileCheck %s
// expected-no-diagnostics

int f(int a);

// Notes:
// (1) B4's Out has b < c and b >= c
// (2) B2's Out doesn't have b < c
// (3) B1's Out is empty
void fn_1(void) {
  int a, b, c;
  if (b < c)
    if (f((a=5)+3))
      b = c;
}
// CHECK-LABEL: fn_1
//
// CHECK: Block: B4; Pred: B5; Succ: B3, B1
// CHECK: Kill [B4, AllSucc]: a, b, c
// CHECK: Gen [B4 -> B3]: 
// CHECK:   b < c
// CHECK: Out [B4 -> B3]: 
// CHECK:   b < c
// CHECK: Gen [B4 -> B1]: 
// CHECK:   b >= c
// CHECK: Out [B4 -> B1]: 
// CHECK:   b >= c
//
// CHECK: Block: B2; Pred: B3; Succ: B1
// CHECK: Kill [B2, AllSucc]: b
// CHECK: Out [B2, AllSucc]:
// CHECK-NOT: b < c
//
// CHECK: Block: B1; Pred: B2, B3, B4; Succ: B0
// CHECK: In [B1]: {}
// CHECK: Out [B1, AllSucc]: {}

// Notes:
// (1) B7's In & Out has e1 < e2
// (2) B7's Kill has b
// (3) B3's In has b < c
// (4) B2's In has b >= c
// (3) B1's Out is empty
void fn_2(void) {
  int e1, e2, a, b, c, q, n;
  if (e1 < e2)
    b += a<e1;

  if (a)
    q = a;

  if (b < c)
    f((b = 2) + 1);
  else
    q = n;
}
// CHECK-LABEL: fn_2
//
// CHECK: Block: B7; Pred: B8; Succ: B6
// CHECK: Kill [B7, AllSucc]: b
// CHECK: In [B7]:
// CHECK:   e1 < e2
// CHECK: Out [B7, AllSucc]:
// CHECK:   e1 < e2
//
// CHECK: Block: B3; Pred: B4; Succ: B1
// CHECK: Kill [B3, AllSucc]: b
// CHECK: In [B3]:
// CHECK:   b < c
// CHECK: Out [B3, AllSucc]: {}
//
// CHECK: Block: B2; Pred: B4; Succ: B1
// CHECK: Kill [B2, AllSucc]: q
// CHECK: In [B2]:
// CHECK:   b >= c
// CHECK: Out [B2, AllSucc]:
// CHECK:   b >= c

// CFG (index is shift by one):
// Entry -> B6
// B6: int a,b,c
// B6: if a then B4 else B5
//    B4: [b], -> B3
//    B5: [c>=2], -> B3
// B3: if [..] then B2 else B1
//    B2: a = b, -> B1
//    B1
// Notes:
// (1) B3 is a block to unite the conditional value
void fn_3(void) {
  int a, b, c;
  if (a ? b : (c>=2))
    a = b;
}
// CHECK-LABEL: fn_3
//
// CHECK: Block: B6; Pred: B7; Succ: B4, B5
// CHECK: Gen [B6 -> B4]:
// CHECK:   a != 0
// CHECK: Gen [B6 -> B5]:
// CHECK:   a == 0
//
// CHECK: Block: B5; Pred: B6; Succ: B3
// CHECK: In [B5]:
// CHECK:   a == 0
// CHECK: Out [B5, AllSucc]:
// CHECK:   a == 0
// CHECK: Gen [B5 -> B3]: {}
// CHECK: Out [B5 -> B3]:
// CHECK:   a == 0
//
// CHECK: Block: B4; Pred: B6; Succ: B3
// CHECK: In [B4]:
// CHECK:   a != 0
// CHECK: Out [B4, AllSucc]:
// CHECK:   a != 0
// CHECK: Gen [B4 -> B3]: {}
// CHECK: Out [B4 -> B3]:
// CHECK:   a != 0
//
// CHECK: B3; Pred: B4, B5; Succ: B2, B1
// CHECK: In [B3]: {}
// CHECK: Out [B3, AllSucc]: {}
// CHECK: Gen [B3 -> B2]:
// CHECK:   a ? b : (c >= 2) != 0
// CHECK: Gen [B3 -> B1]:
// CHECK:   a ? b : (c >= 2) == 0
//
// CHECK: B2; Pred: B3; Succ: B1
// CHECK: Kill [B2, AllSucc]: a
// CHECK: In [B2]: 
// CHECK:   a ? b : (c >= 2) != 0
// CHECK: Out [B2, AllSucc]: {}
//
// CHECK: B1; Pred: B2, B3; Succ: B0
// CHECK: Kill [B1, AllSucc]: {}
// CHECK: In [B1]:  {}
// CHECK: Out [B1, AllSucc]: {}

// TODO: while loop
// void fn_4(void) {}

_Nt_array_ptr<int> g(int a) : byte_count(a);
_Nt_array_ptr<int> fn_5(int a) {
  int d;
  if (d > a)
    return 0;

  _Nt_array_ptr<int> p : byte_count(d) = g(a);
  return p;
}
// CHECK-LABEL: fn_5
//
// CHECK: Block: B3; Pred: B4; Succ: B2, B1
// CHECK: Gen [B3, AllSucc]: {}
// CHECK: Kill [B3, AllSucc]: d
// CHECK: In [B3]: {}
// CHECK: Out [B3, AllSucc]: {}
// CHECK: Gen [B3 -> B2]
// CHECK:   d > a
// CHECK: Gen [B3 -> B1]
// CHECK:   d <= a
//
// CHECK: Block: B2; Pred: B3; Succ: B0
// CHECK: Gen [B2, AllSucc]: {}
// CHECK: Kill [B2, AllSucc]: {}
// CHECK: In [B2]:
// CHECK:   d > a
// CHECK: Out [B2, AllSucc]:
// CHECK:   d > a
//
// CHECK: Block: B1; Pred: B3; Succ: B0
// CHECK: Gen [B1, AllSucc]:
// CHECK-DAG:   p: bounds((_Array_ptr<char>)p, (_Array_ptr<char>)p + d)
// CHECK: Kill [B1, AllSucc]: p
// CHECK: In [B1]:
// CHECK:   d <= a
// CHECK: Out [B1, AllSucc]:
// CHECK-DAG:   p: bounds((_Array_ptr<char>)p, (_Array_ptr<char>)p + d)
// CHECK-DAG:   d <= a

_Nt_array_ptr<int> fn_6(int a) {
  int d;
  if (d <= a) {
    _Nt_array_ptr<int> p : byte_count(d) = g(a);
    return p;
  }
  return 0;
}
// CHECK-LABEL: fn_6
//
// CHECK: Block: B3; Pred: B4; Succ: B2, B1
// CHECK: Gen [B3, AllSucc]: {}
// CHECK: Kill [B3, AllSucc]: d
// CHECK: In [B3]: {}
// CHECK: Out [B3, AllSucc]: {}
// CHECK: Gen [B3 -> B2]
// CHECK:   d <= a
// CHECK: Gen [B3 -> B1]
// CHECK:   d > a
//
// CHECK: Block: B2; Pred: B3; Succ: B0
// CHECK: Gen [B2, AllSucc]:
// CHECK-DAG:   p: bounds((_Array_ptr<char>)p, (_Array_ptr<char>)p + d)
// CHECK: Kill [B2, AllSucc]: p
// CHECK: In [B2]:
// CHECK:   d <= a
// CHECK: Out [B2, AllSucc]:
// CHECK-DAG:   p: bounds((_Array_ptr<char>)p, (_Array_ptr<char>)p + d)
// CHECK-DAG:   d <= a
//
// CHECK: Block: B1; Pred: B3; Succ: B0
// CHECK: Gen [B1, AllSucc]: {}
// CHECK: Kill [B1, AllSucc]: {}
// CHECK: In [B1]:
// CHECK:   d > a
// CHECK: Out [B1, AllSucc]:
// CHECK:   d > a

// TODO: while loop
// void fn_7(void)

// --- Facts should not contain volatiles or calls --- //
// TODO: 
// void fn_8(void) {

// CFG
//  B9: a < b, -> [B8,                   B2]
//     B8: b < c, -> [B7,                B2]
//        B7: a != 1, -> [B6,    B4]
//           B6: b != 2, -> [B5, B4]
//              B5: c != 3,  -> [B4, B3]
//  B4: d = 0, -> B1
//  B3: d = 1, -> B1
//  B2: c = 2, -> B1
//     B1: c = 3, -> B0
void fn_9(void) {
  int a, b, c, d;
  if (a < b && b < c) {
    if (a != 1 || b != 2 || c != 3)
      d = 0;
    else
      d = 1;
  } else
    // Note for short-circuited logic
    // (a >= b) || (a < b && b >= c)
    c = 2;
  c = 3;
}
// CHECK-LABEL: fn_9
//
// CHECK: Block: B9; Pred: B10; Succ: B8, B2
// CHECK: Gen [B9, AllSucc]: {}
// CHECK: Kill [B9, AllSucc]: a, b, c, d
// CHECK: In [B9]: {}
// CHECK: Out [B9, AllSucc]: {}
// CHECK: Gen [B9 -> B8]
// CHECK:   a < b
// CHECK: Gen [B9 -> B2]
// CHECK:   a >= b
//
// CHECK: Block: B8; Pred: B9; Succ: B7, B2
// CHECK: Gen [B8, AllSucc]: {}
// CHECK: Kill [B8, AllSucc]: {}
// CHECK: In [B8]:
// CHECK:   a < b
// CHECK: Out [B8, AllSucc]:
// CHECK:   a < b
// CHECK: Gen [B8 -> B7]
// CHECK:   b < c
// CHECK: Out [B8 -> B7]
// CHECK-DAG:   b < c
// CHECK-DAG:   a < b
// CHECK: Gen [B8 -> B2]
// CHECK-DAG:   b >= c
// CHECK-DAG:   a < b
//
// CHECK: Block: B7; Pred: B8; Succ: B4, B6
// CHECK: Gen [B7, AllSucc]: {}
// CHECK: Kill [B7, AllSucc]: {}
// CHECK: In [B7]:
// CHECK-DAG:   b < c
// CHECK-DAG:   a < b
// CHECK: Out [B7, AllSucc]:
// CHECK-DAG:   b < c
// CHECK-DAG:   a < b
// CHECK: Gen [B7 -> B4]
// CHECK:   a != 1
// CHECK: Out [B7 -> B4]
// CHECK-DAG:   a != 1
// CHECK-DAG:   b < c
// CHECK-DAG:   a < b
// CHECK: Gen [B7 -> B6]
// CHECK:   a == 1
// CHECK: Out [B7 -> B6]
// CHECK-DAG:   a == 1
// CHECK-DAG:   b < c
// CHECK-DAG:   a < b
//
// CHECK: Block: B6; Pred: B7; Succ: B4, B5
// CHECK: Gen [B6, AllSucc]: {}
// CHECK: Kill [B6, AllSucc]: {}
// CHECK: In [B6]:
// CHECK-DAG:   a == 1
// CHECK-DAG:   b < c
// CHECK-DAG:   a < b
// CHECK: Out [B6, AllSucc]:
// CHECK-DAG:   a == 1
// CHECK-DAG:   b < c
// CHECK-DAG:   a < b
// CHECK: Gen [B6 -> B4]
// CHECK:   b != 2
// CHECK: Out [B6 -> B4]
// CHECK-DAG:   b != 2
// CHECK-DAG:   a == 1
// CHECK-DAG:   b < c
// CHECK-DAG:   a < b
// CHECK: Gen [B6 -> B5]
// CHECK:   b == 2
// CHECK: Out [B6 -> B5]
// CHECK-DAG:   b == 2
// CHECK-DAG:   a == 1
// CHECK-DAG:   b < c
// CHECK-DAG:   a < b
//
// CHECK: Block: B5; Pred: B6; Succ: B4, B3
// CHECK: Gen [B5, AllSucc]: {}
// CHECK: Kill [B5, AllSucc]: {}
// CHECK: In [B5]:
// CHECK-DAG:   b == 2
// CHECK-DAG:   a == 1
// CHECK-DAG:   b < c
// CHECK-DAG:   a < b
// CHECK: Out [B5, AllSucc]:
// CHECK-DAG:   b == 2
// CHECK-DAG:   a == 1
// CHECK-DAG:   b < c
// CHECK-DAG:   a < b
// CHECK: Gen [B5 -> B4]
// CHECK:   c != 3
// CHECK: Out [B5 -> B4]
// CHECK-DAG:   c != 3
// CHECK-DAG:   b == 2
// CHECK-DAG:   a == 1
// CHECK-DAG:   b < c
// CHECK-DAG:   a < b
// CHECK: Gen [B5 -> B3]
// CHECK:   c == 3
// CHECK: Out [B5 -> B3]
// CHECK-DAG:   c == 3
// CHECK-DAG:   b == 2
// CHECK-DAG:   a == 1
// CHECK-DAG:   b < c
// CHECK-DAG:   a < b
//
// CHECK: Block: B4; Pred: B5, B6, B7; Succ: B1
// CHECK: Gen [B4, AllSucc]: {}
// CHECK: Kill [B4, AllSucc]: d
// CHECK: In [B4]:
// CHECK-DAG:   b < c
// CHECK-DAG:   a < b
// CHECK: Out [B4, AllSucc]:
// CHECK-DAG:   b < c
// CHECK-DAG:   a < b
// CHECK: Gen [B4 -> B1]: {}
// CHECK: Out [B4 -> B1]:
// CHECK-DAG:   b < c
// CHECK-DAG:   a < b
//
// CHECK: Block: B3; Pred: B5; Succ: B1
// CHECK: Gen [B3, AllSucc]: {}
// CHECK: Kill [B3, AllSucc]: d
// CHECK: In [B3]:
// CHECK-DAG:   c == 3
// CHECK-DAG:   b == 2
// CHECK-DAG:   a == 1
// CHECK-DAG:   b < c
// CHECK-DAG:   a < b
// CHECK: Out [B3, AllSucc]:
// CHECK-DAG:   c == 3
// CHECK-DAG:   b == 2
// CHECK-DAG:   a == 1
// CHECK-DAG:   b < c
// CHECK-DAG:   a < b
// CHECK: Gen [B3 -> B1]: {}
// CHECK: Out [B3 -> B1]:
// CHECK-DAG:   c == 3
// CHECK-DAG:   b == 2
// CHECK-DAG:   a == 1
// CHECK-DAG:   b < c
// CHECK-DAG:   a < b
//
// Note for short-circuited logic
// Out[9,2]: a >= b
// Out[9,*]: {}
// Out[8,2]: a < b, b >= c
// Out[8,*]: a < b
// In[2] = intersect ( (a >= b), (a < b, b >= c) ) = {}
// CHECK: Block: B2; Pred: B8, B9; Succ: B1
// CHECK: Gen [B2, AllSucc]: {}
// CHECK: Kill [B2, AllSucc]: c
// CHECK: In [B2]: {}
// CHECK: Out [B2, AllSucc]: {}
// CHECK: Gen [B2 -> B1]: {}
// CHECK: Out [B2 -> B1]: {}

int h(int *p);
void fn_10(void) {
  int *p, *q;
  int a, b, c;

  if (*p < a)
    b = 1;
  q = &a;

  if (*p < q[1])
    b=1;
  *q = 5;
}
// CHECK-LABEL: fn_10
//
// CHECK: Block: B5; Pred: B6; Succ: B4, B3
// CHECK: Gen [B5, AllSucc]: {}
// CHECK: Kill [B5, AllSucc]: a, b, c, p, q
// CHECK: In [B5]: {}
// CHECK: Out [B5, AllSucc]: {}
// CHECK: Gen [B5 -> B4]:
// CHECK-DAG:   *p < a
// CHECK: Out [B5 -> B4]:
// CHECK-DAG:   *p < a
// CHECK: Gen [B5 -> B3]:
// CHECK-DAG:   *p >= a
// CHECK: Out [B5 -> B3]:
// CHECK-DAG:   *p >= a
//
// CHECK: Block: B4; Pred: B5; Succ: B3
// CHECK: Gen [B4, AllSucc]: {}
// CHECK: Kill [B4, AllSucc]: b
// CHECK: In [B4]:
// CHECK-DAG:   *p < a
// CHECK: Out [B4, AllSucc]:
// CHECK-DAG:   *p < a
// CHECK: Gen [B4 -> B3]: {}
// CHECK: Out [B4 -> B3]:
// CHECK-DAG:   *p < a
//
// CHECK: Block: B3; Pred: B4, B5; Succ: B2, B1
// CHECK: Gen [B3, AllSucc]: {}
// CHECK: Kill [B3, AllSucc]: q
// CHECK: In [B3]: {}
// CHECK: Out [B3, AllSucc]: {}
// CHECK: Gen [B3 -> B2]:
// CHECK-DAG:   *p < q[1]
// CHECK: Out [B3 -> B2]:
// CHECK-DAG:   *p < q[1]
// CHECK: Gen [B3 -> B1]:
// CHECK-DAG:   *p >= q[1]
// CHECK: Out [B3 -> B1]:
// CHECK-DAG:   *p >= q[1]

void o(int *p);
void fn_11(void) {
  int a, *p, b;
  if (*p < a) {
    b = 2;
    o(p);
  }
}
// CHECK-LABEL: fn_11
//
// CHECK: Block: B3; Pred: B4; Succ: B2, B1
// CHECK: Gen [B3, AllSucc]: {}
// CHECK: Kill [B3, AllSucc]: a, b, p
// CHECK: In [B3]: {}
// CHECK: Out [B3, AllSucc]: {}
// CHECK: Gen [B3 -> B2]:
// CHECK-DAG:   *p < a
// CHECK: Out [B3 -> B2]:
// CHECK-DAG:   *p < a
// CHECK: Gen [B3 -> B1]:
// CHECK-DAG:   *p >= a
// CHECK: Out [B3 -> B1]:
// CHECK-DAG:   *p >= a
//
// CHECK: Block: B2; Pred: B3; Succ: B1
// CHECK: Gen [B2, AllSucc]: {}
// CHECK: Kill [B2, AllSucc]: b
// CHECK: In [B2]:
// CHECK-DAG:   *p < a
// CHECK: Out [B2, AllSucc]:
// CHECK-DAG:   *p < a
// CHECK: Gen [B2 -> B1]: {}
// CHECK: Out [B2 -> B1]:
// CHECK-DAG:   *p < a

struct st { int x; int y; };
void fn_12(void) {
  struct st *st_a;
  int b, a, *q;

  if (st_a->x < a)
    b = 1;
  if ((*st_a).x < b)
    a = 1;
  else
    if (*(q + 4) <= 8)
      a = 3;
  (*st_a).x = 7;
}
// CHECK-LABEL: fn_12
//
// CHECK: Block: B7; Pred: B8; Succ: B6, B5
// CHECK: Gen [B7, AllSucc]: {}
// CHECK: Kill [B7, AllSucc]: a, b, q, st_a
// CHECK: In [B7]: {}
// CHECK: Out [B7, AllSucc]: {}
// CHECK: Gen [B7 -> B6]:
// CHECK-DAG:   st_a->x < a
// CHECK: Out [B7 -> B6]:
// CHECK-DAG:   st_a->x < a
// CHECK: Gen [B7 -> B5]:
// CHECK-DAG:   st_a->x >= a
// CHECK: Out [B7 -> B5]:
// CHECK-DAG:   st_a->x >= a
//
// CHECK: Block: B6; Pred: B7; Succ: B5
// CHECK: Gen [B6, AllSucc]: {}
// CHECK: Kill [B6, AllSucc]: b
// CHECK: In [B6]:
// CHECK-DAG:   st_a->x < a
// CHECK: Out [B6, AllSucc]:
// CHECK-DAG:   st_a->x < a
// CHECK: Gen [B6 -> B5]: {}
// CHECK: Out [B6 -> B5]:
// CHECK-DAG:   st_a->x < a
//
// CHECK: Block: B5; Pred: B6, B7; Succ: B4, B3
// CHECK: Gen [B5, AllSucc]: {}
// CHECK: Kill [B5, AllSucc]: {}
// CHECK: In [B5]: {}
// CHECK: Out [B5, AllSucc]: {}
// CHECK: Gen [B5 -> B4]:
// CHECK-DAG:   (*st_a).x < b
// CHECK: Out [B5 -> B4]:
// CHECK-DAG:   (*st_a).x < b
// CHECK: Gen [B5 -> B3]:
// CHECK-DAG:   (*st_a).x >= b
// CHECK: Out [B5 -> B3]:
// CHECK-DAG:   (*st_a).x >= b
//
// CHECK: Block: B4; Pred: B5; Succ: B1
// CHECK: Gen [B4, AllSucc]: {}
// CHECK: Kill [B4, AllSucc]: a
// CHECK: In [B4]:
// CHECK-DAG:   (*st_a).x < b
// CHECK: Out [B4, AllSucc]:
// CHECK-DAG:   (*st_a).x < b
// CHECK: Gen [B4 -> B1]: {}
// CHECK: Out [B4 -> B1]:
// CHECK-DAG:   (*st_a).x < b
//
// CHECK: Block: B3; Pred: B5; Succ: B2, B1
// CHECK: Gen [B3, AllSucc]: {}
// CHECK: Kill [B3, AllSucc]: {}
// CHECK: In [B3]:
// CHECK-DAG:   (*st_a).x >= b
// CHECK: Out [B3, AllSucc]:
// CHECK-DAG:   (*st_a).x >= b
// CHECK: Gen [B3 -> B2]:
// CHECK-DAG:   *(q + 4) <= 8
// CHECK: Out [B3 -> B2]:
// CHECK-DAG:   *(q + 4) <= 8
// CHECK-DAG:   (*st_a).x >= b
// CHECK: Gen [B3 -> B1]:
// CHECK-DAG:   *(q + 4) > 8
// CHECK: Out [B3 -> B1]:
// CHECK-DAG:   *(q + 4) > 8
// CHECK-DAG:   (*st_a).x >= b
//
// CHECK: Block: B2; Pred: B3; Succ: B1
// CHECK: Gen [B2, AllSucc]: {}
// CHECK: Kill [B2, AllSucc]: a
// CHECK: In [B2]:
// CHECK-DAG:   *(q + 4) <= 8
// CHECK-DAG:   (*st_a).x >= b
// CHECK: Out [B2, AllSucc]:
// CHECK-DAG:   *(q + 4) <= 8
// CHECK-DAG:   (*st_a).x >= b
// CHECK: Gen [B2 -> B1]: {}
// CHECK: Out [B2 -> B1]:
// CHECK-DAG:   *(q + 4) <= 8
// CHECK-DAG:   (*st_a).x >= b
//

// TODO: fn_13
// unchecked and for-loop

// TODO: fn_14
// for-loop

// TODO: fn_15
// switch and for-loop

struct st_80;
struct st_80_arr {
    struct st_80 **e : itype(_Array_ptr<_Ptr<struct st_80>>) count(c);
    int d;
    int c;
};
void fn_16(_Ptr<struct st_80_arr> arr, int b) {
  _Array_ptr<_Ptr<struct st_80>> a : count(b) = 0;
  if (arr->c <= b) {
    arr->c = b * b, arr->e = a;
  }
}
// CHECK-LABEL: fn_16
//
// CHECK: Block: B3; Pred: B4; Succ: B2, B1
// CHECK: Gen [B3, AllSucc]: 
// CHECK-DAG:   a: bounds(a, a + b)
// CHECK: Kill [B3, AllSucc]: a
// CHECK: In [B3]: {}
// CHECK: Out [B3, AllSucc]:
// CHECK-DAG:   a: bounds(a, a + b)
// CHECK: Gen [B3 -> B2]:
// CHECK-DAG:   arr->c <= b
// CHECK: Out [B3 -> B2]:
// CHECK-DAG:   arr->c <= b
// CHECK: Gen [B3 -> B1]:
// CHECK-DAG:   arr->c > b
// CHECK: Out [B3 -> B1]:
// CHECK-DAG:   arr->c > b
//
// CHECK: Block: B2; Pred: B3; Succ: B1
// CHECK: Gen [B2, AllSucc]: {}
// CHECK: Kill [B2, AllSucc]: arr
// CHECK: In [B2]:
// CHECK-DAG:   arr->c <= b
// CHECK-DAG:   a: bounds(a, a + b)
// CHECK: Out [B2, AllSucc]:
// CHECK-DAG:   a: bounds(a, a + b)
// CHECK: Gen [B2 -> B1]: {}
// CHECK: Out [B2 -> B1]:
// CHECK-DAG:   a: bounds(a, a + b)
//

// TODO: fn_17
// while-loop

// TODO: fn_18
// TODO: fn_19
// while-loop

typedef struct {
  int f1;
} S1;
void fn_20(void) {
  int a;
  _Array_ptr<S1> gp1 : count(3) = 0;
  _Array_ptr<S1> gp3 : count(3) = 0;
  if (gp3->f1 != 0)
    ++(a);
  (gp3 + 2)->f1 += 1;
}
// CHECK-LABEL: fn_20
//
// CHECK: Block: B3; Pred: B4; Succ: B2, B1
// CHECK: Gen [B3, AllSucc]:
// CHECK-DAG:   gp1: bounds(gp1, gp1 + 3)
// CHECK-DAG:   gp3: bounds(gp3, gp3 + 3)
// CHECK: Kill [B3, AllSucc]: a, gp1, gp3
// CHECK: In [B3]: {}
// CHECK: Out [B3, AllSucc]:
// CHECK-DAG:   gp1: bounds(gp1, gp1 + 3)
// CHECK-DAG:   gp3: bounds(gp3, gp3 + 3)
// CHECK: Gen [B3 -> B2]:
// CHECK-DAG:   gp3->f1 != 0
// CHECK: Out [B3 -> B2]:
// CHECK-DAG:   gp3->f1 != 0
// CHECK: Gen [B3 -> B1]:
// CHECK-DAG:   gp3->f1 == 0
// CHECK: Out [B3 -> B1]:
// CHECK-DAG:   gp3->f1 == 0
//
// CHECK: Block: B2; Pred: B3; Succ: B1
// CHECK: Gen [B2, AllSucc]: {}
// CHECK: Kill [B2, AllSucc]: a
// CHECK: In [B2]:
// CHECK-DAG:   gp3->f1 != 0
// CHECK-DAG:   gp1: bounds(gp1, gp1 + 3)
// CHECK-DAG:   gp3: bounds(gp3, gp3 + 3)
// CHECK: Out [B2, AllSucc]:
// CHECK-DAG:   gp3->f1 != 0
// CHECK-DAG:   gp1: bounds(gp1, gp1 + 3)
// CHECK-DAG:   gp3: bounds(gp3, gp3 + 3)
// CHECK: Gen [B2 -> B1]: {}
// CHECK: Out [B2 -> B1]:
// CHECK-DAG:   gp3->f1 != 0
