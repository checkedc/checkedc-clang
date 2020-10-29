#include "gtest/gtest.h"
#include "clang/3C/Constraints.h"

bool AllTypes = true;
TEST(BasicConstraintTest, insert) {
  Constraints CS;
  VarAtom *q_0 = CS.getOrCreateVar(0, "q", VarAtom::V_Other);
  VarAtom *q_1 = CS.getOrCreateVar(1, "q", VarAtom::V_Other);
  VarAtom *q_2 = CS.getOrCreateVar(2, "q", VarAtom::V_Other);

  Constraints::ConstraintSet csInsert;
  csInsert.insert(CS.createGeq(q_0, CS.getWild()));
  csInsert.insert(CS.createGeq(q_1, CS.getArr()));
  csInsert.insert(CS.createGeq(q_2, CS.getArr()));

  for (const auto &C : csInsert) {
    EXPECT_TRUE(CS.addConstraint(C));
  }

  Constraints::ConstraintSet csInserted = CS.getConstraints();

  EXPECT_EQ(csInsert, csInserted);

  EXPECT_FALSE(CS.addConstraint(CS.createGeq(q_0, CS.getWild())));

  csInserted = CS.getConstraints();

  EXPECT_EQ(csInsert, csInserted);
}

TEST(BasicConstraintTest, ordering) {
  Constraints CS;

  PtrAtom *P = CS.getPtr();
  ArrAtom *A = CS.getArr();
  WildAtom *W = CS.getWild();
  VarAtom *q_0 = CS.getOrCreateVar(0, "q", VarAtom::V_Other);
  VarAtom *q_1 = CS.getOrCreateVar(1, "q", VarAtom::V_Other);
  VarAtom *q_2 = CS.getOrCreateVar(2, "q", VarAtom::V_Other);
  Constraint *C1 = CS.createGeq(q_0, CS.getWild());
  Constraint *C2 = CS.createGeq(q_1, CS.getWild());
  Constraint *C4 = CS.createGeq(q_1, CS.getArr());
  Constraint *C3 = CS.createGeq(q_2, CS.getArr());

  EXPECT_TRUE(*P < *A);
  EXPECT_TRUE(*A < *W);
  EXPECT_TRUE(*P < *W);
  EXPECT_TRUE(!(*A < *P));
  EXPECT_TRUE(!(*W < *A));
  EXPECT_TRUE(!(*W < *P));
  EXPECT_TRUE(!(*P < *P));
  EXPECT_TRUE(!(*A < *A));
  EXPECT_TRUE(!(*W < *W));

  EXPECT_TRUE(*P < *q_0);
  EXPECT_TRUE(*A < *q_0);
  EXPECT_TRUE(*W < *q_0);
  EXPECT_TRUE(*q_0 < *q_1);
  EXPECT_TRUE(*q_1 < *q_2);
  EXPECT_TRUE(!(*q_1 < *q_0));
  EXPECT_TRUE(!(*q_1 < *q_1));
  EXPECT_TRUE(*C1 < *C2);
  EXPECT_TRUE(*C4 < *C2);
  EXPECT_TRUE(*C2 < *C3);
}

TEST(BasicConstraintTestGeq, solve) {
    Constraints CS;

    EXPECT_TRUE(CS.addConstraint(CS.createGeq(
        CS.getOrCreateVar(0, "q", VarAtom::V_Other), CS.getWild())));
    EXPECT_TRUE(CS.addConstraint(CS.createGeq(
        CS.getOrCreateVar(1, "q", VarAtom::V_Other), CS.getArr())));
    EXPECT_TRUE(CS.addConstraint(CS.createGeq(
        CS.getOrCreateVar(2, "q", VarAtom::V_Other), CS.getNTArr())));

    EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getOrCreateVar(2, "q", VarAtom::V_Other),
                     CS.getOrCreateVar(1, "q", VarAtom::V_Other))));
    EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getOrCreateVar(3, "q", VarAtom::V_Other),
                     CS.getOrCreateVar(2, "q", VarAtom::V_Other))));
    EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getOrCreateVar(1, "q", VarAtom::V_Other),
                     CS.getOrCreateVar(2, "q", VarAtom::V_Other))));
    EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getOrCreateVar(2, "q", VarAtom::V_Other),
                     CS.getOrCreateVar(3, "q", VarAtom::V_Other))));

    EXPECT_TRUE(CS.addConstraint(CS.createImplies(CS.createGeq(CS.getOrCreateVar(2, "q", VarAtom::V_Other), CS.getWild()),
                                                  CS.createGeq(CS.getOrCreateVar(1, "q", VarAtom::V_Other), CS.getWild()))));
    EXPECT_TRUE(CS.addConstraint(CS.createImplies(CS.createGeq(CS.getOrCreateVar(2, "q", VarAtom::V_Other), CS.getArr()),
                                                  CS.createGeq(CS.getOrCreateVar(3, "q", VarAtom::V_Other), CS.getWild()))));

    EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getWild(),CS.getVar(0))));
    EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getArr(),CS.getVar(1))));
    EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getNTArr(),CS.getVar(2))));
    EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getWild(),CS.getVar(3))));

    CS.solve();
    Constraints::EnvironmentMap env = CS.getVariables();

    EXPECT_TRUE(*env[CS.getVar(0)] == *CS.getWild());
    EXPECT_TRUE(*env[CS.getVar(1)] == *CS.getArr());
    EXPECT_TRUE(*env[CS.getVar(2)] == *CS.getNTArr());
    EXPECT_TRUE(*env[CS.getVar(3)] == *CS.getWild());
}

TEST(BasicConstraintTest, solve) {
    Constraints CS;

    EXPECT_TRUE(CS.addConstraint(CS.createGeq(
        CS.getOrCreateVar(0, "q", VarAtom::V_Other), CS.getWild())));
    EXPECT_TRUE(CS.addConstraint(CS.createGeq(
        CS.getOrCreateVar(2, "q", VarAtom::V_Other), CS.getArr())));
    EXPECT_TRUE(CS.addConstraint(CS.createImplies(CS.createGeq(CS.getOrCreateVar(0, "q", VarAtom::V_Other), CS.getWild()),
                                                  CS.createGeq(CS.getOrCreateVar(1, "q", VarAtom::V_Other), CS.getWild()))));
    EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getOrCreateVar(3, "q", VarAtom::V_Other),
                     CS.getOrCreateVar(2, "q", VarAtom::V_Other))));

    EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getOrCreateVar(4, "q", VarAtom::V_Other),
                     CS.getOrCreateVar(5, "q", VarAtom::V_Other))));
    EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getOrCreateVar(5, "q", VarAtom::V_Other),
                     CS.getOrCreateVar(4, "q", VarAtom::V_Other))));
    EXPECT_TRUE(CS.addConstraint(CS.createGeq(
        CS.getOrCreateVar(4, "q", VarAtom::V_Other), CS.getWild())));
    EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getOrCreateVar(5, "q", VarAtom::V_Other),
                     CS.getOrCreateVar(5, "q", VarAtom::V_Other))));
    EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getOrCreateVar(6, "q", VarAtom::V_Other),
                     CS.getOrCreateVar(6, "q", VarAtom::V_Other))));
    EXPECT_TRUE(CS.addConstraint(CS.createImplies(CS.createGeq(CS.getOrCreateVar(3, "q", VarAtom::V_Other), CS.getArr()),
                                                  CS.createGeq(CS.getOrCreateVar(7, "q", VarAtom::V_Other), CS.getArr()))));
    EXPECT_TRUE(CS.addConstraint(CS.createImplies(CS.createGeq(CS.getOrCreateVar(3, "q", VarAtom::V_Other), CS.getWild()),
                                                  CS.createGeq(CS.getOrCreateVar(8, "q", VarAtom::V_Other), CS.getWild()))));

//  EXPECT_TRUE(CS.solve(numI).second);
    CS.solve();
    Constraints::EnvironmentMap env = CS.getVariables();

    EXPECT_TRUE(*env[CS.getVar(0)] == *CS.getWild());
    EXPECT_TRUE(*env[CS.getVar(1)] == *CS.getWild());
    EXPECT_TRUE(*env[CS.getVar(2)] == *CS.getArr());
    EXPECT_TRUE(*env[CS.getVar(3)] == *CS.getArr());
    EXPECT_TRUE(*env[CS.getVar(4)] == *CS.getWild());
    EXPECT_TRUE(*env[CS.getVar(5)] == *CS.getWild());
    EXPECT_TRUE(*env[CS.getVar(6)] == *CS.getPtr());
    EXPECT_TRUE(*env[CS.getVar(7)] == *CS.getArr());
    EXPECT_TRUE(*env[CS.getVar(8)] == *CS.getPtr());
}
TEST(BasicConstraintTest, equality) {
  Constraints CS;

  // q_0 = WILD
  // q_1 = PTR
  // q_0 = q_1
  // 
  // should derive 
  // q_0 = WILD
  // q_1 = WILD

  EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getOrCreateVar(0, "q", VarAtom::V_Other), CS.getWild())));
  EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getOrCreateVar(1, "q", VarAtom::V_Other), CS.getPtr())));
  EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getOrCreateVar(0, "q", VarAtom::V_Other), CS.getOrCreateVar(1, "q", VarAtom::V_Other))));

  CS.solve();
  //EXPECT_TRUE(CS.solve(numI).second);
  Constraints::EnvironmentMap env = CS.getVariables();

  EXPECT_TRUE(*env[CS.getVar(0)] == *CS.getWild());
  EXPECT_TRUE(*env[CS.getVar(1)] == *CS.getWild());
}

TEST(Conflicts, test1) {
  Constraints CS;

  // q_0 = PTR
  // q_0 = ARR
  // q_1 = WILD
  // q_0 = q_1

  EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getOrCreateVar(0, "q", VarAtom::V_Other), CS.getPtr())));
  EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getOrCreateVar(0, "q", VarAtom::V_Other), CS.getArr())));
  EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getOrCreateVar(1, "q", VarAtom::V_Other), CS.getWild())));
  EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getOrCreateVar(0, "q", VarAtom::V_Other), CS.getOrCreateVar(1, "q", VarAtom::V_Other))));

  //EXPECT_TRUE(CS.solve(numI).second);
  CS.solve();
  Constraints::EnvironmentMap env = CS.getVariables();
  EXPECT_TRUE(*env[CS.getVar(0)] == *CS.getWild());
}

TEST(BasicNTArrayTest, NTArrayTests) {
  Constraints CS;
  // q_0 = NTArr

  // should derive
  // q_0 = NTArr

  EXPECT_TRUE(CS.addConstraint(CS.createGeq(
      CS.getOrCreateVar(0, "q", VarAtom::V_Other), CS.getNTArr())));

  //EXPECT_TRUE(CS.solve(numI).second);
  CS.solve();
  Constraints::EnvironmentMap env = CS.getVariables();

  EXPECT_TRUE(*env[CS.getVar(0)] == *CS.getNTArr());
}

TEST(NTArrayAndArrayTest, NTArrayTests) {
  // tries to test the following case:
  /*
   * // this will derive first set: set 1
   * char *str = strstr(..,..);
   * ..
   * // this will derive the second constraint: set 2
   * str[j] = 'a';
   */
  Constraints CS;
  // set 1
  // q_0 == NTARR
  // set 2
  // q_0 = ARR

  // should derive
  // q_0 = NTARR

  EXPECT_TRUE(CS.addConstraint(CS.createGeq(
      CS.getOrCreateVar(0, "q", VarAtom::V_Other), CS.getNTArr())));
  EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getOrCreateVar(0, "q", VarAtom::V_Other), CS.getArr())));

  //EXPECT_TRUE(CS.solve(numI).second);
  CS.solve();
  Constraints::EnvironmentMap env = CS.getVariables();

  EXPECT_TRUE(*env[CS.getVar(0)] == *CS.getNTArr());
}

TEST(NTArrayAndArrayConflictTest, NTArrayTests) {
  // tries to test the following case:
  /*
   * // this will derive first set: set 1
   * char *data;
   * data = str..
   * ...
   * // this will add second set of constraints
   * data += 1;
   */
  Constraints CS;
  // set 1
  // q_0 == ARR
  // q_0 == NTArr
  // set 2
  // q_0 == PTR

  // should derive
  // q_0 = NTArr

  // set 1
  EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getOrCreateVar(0, "q", VarAtom::V_Other), CS.getArr())));
  EXPECT_TRUE(CS.addConstraint(CS.createGeq(
      CS.getOrCreateVar(0, "q", VarAtom::V_Other), CS.getNTArr())));
  // set 2
  EXPECT_TRUE(CS.addConstraint(CS.createGeq(CS.getOrCreateVar(0, "q", VarAtom::V_Other), CS.getPtr())));


    //EXPECT_TRUE(CS.solve(numI).second);
  CS.solve();
  Constraints::EnvironmentMap env = CS.getVariables();

  EXPECT_TRUE(*env[CS.getVar(0)] == *CS.getNTArr());
}
