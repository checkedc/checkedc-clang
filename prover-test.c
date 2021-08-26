// More where clauses
// void f(int x
//   // , _Array_ptr<int> a
//   ) {
//   x = 1;
  // x = 1;
  // _Where z < 3;
  // _Where a : count(3);

  // y = 3;
  // _Where y != 2;
  
  // _Where (x > 4) _And (y >= 5) _And (z == 6);
  // _Where a : count(3 * x);
// }

//CheckBoundsDeclAtInitializer
void f(int x, _Array_ptr<int> a : count(3)) {
  // _Where x == 1;
  _Where x == 5;
  _Array_ptr<int> b : count(x) = a;
}

// CheckBoundsDeclAtIncrementDecrement?
// BUG: 
// EquivExprs: 
// [2] a, a + 1
// void f(int x, _Array_ptr<int> a : count(3)) {
//   a++;
// }

// CheckBoundsDeclAtCallArg
// void g(_Array_ptr<int> b : count(3));

// void f(int x, _Array_ptr<int> a : count(2)) {
//   x = 1;
//   g(a);
// }

// CheckBoundsDeclAtAssignment (in CheckBinaryOperator)
// void f(_Array_ptr<int> a : count(2), _Array_ptr<int> b : count(3)) {
//   b = a;
// }

// CheckBoundsDeclAtStaticPtrCast
// void f(int x, _Array_ptr<int> a : count(2)) {
//   (_Array_ptr<int> : count(x)) a;
// }
// void f1(int *p : count(0)) {
//   _Ptr<int> test = (_Ptr<int>)p;
//   test[0];
// }

// CheckObservedBounds (in ValidateBoundsContext)
// n/a

// Mixed short and int
// void f(_Array_ptr<int> a : count(2+1), int x, short y, short z) {
//   _Array_ptr<int> b : count((x+y)+z) = a;
//   _Array_ptr<int> c : count(x+(y+z)) = a;
// }
