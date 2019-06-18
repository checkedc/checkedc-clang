#include "gtest/gtest.h"
#include "../Constraints.h"

TEST(BasicConstraintTest, insert) {
  Constraints CS;
  VarAtom *q_0 = CS.getOrCreateVar(0);
  VarAtom *q_1 = CS.getOrCreateVar(1);
  VarAtom *q_2 = CS.getOrCreateVar(2);

  Constraints::ConstraintSet csInsert;
  csInsert.insert(CS.createEq(q_0, CS.getWild()));
  csInsert.insert(CS.createEq(q_1, CS.getArr()));
  csInsert.insert(CS.createNot(CS.createEq(q_2, CS.getPtr())));

  for (const auto &C : csInsert) {
    EXPECT_TRUE(CS.addConstraint(C));
  }

  Constraints::ConstraintSet csInserted = CS.getConstraints();

  EXPECT_EQ(csInsert, csInserted);

  EXPECT_FALSE(CS.addConstraint(CS.createEq(q_0, CS.getWild())));

  csInserted = CS.getConstraints();

  EXPECT_EQ(csInsert, csInserted);
}

TEST(BasicConstraintTest, ordering) {
  Constraints CS;

  PtrAtom *P = CS.getPtr();
  ArrAtom *A = CS.getArr();
  WildAtom *W = CS.getWild();
  VarAtom *q_0 = CS.getOrCreateVar(0);
  VarAtom *q_1 = CS.getOrCreateVar(1);
  VarAtom *q_2 = CS.getOrCreateVar(2);
  Constraint *C1 = CS.createEq(q_0, CS.getWild());
  Constraint *C2 = CS.createEq(q_1, CS.getWild());
  Constraint *C4 = CS.createEq(q_1, CS.getArr());
  Constraint *C3 = CS.createEq(q_2, CS.getArr());

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

TEST(BasicConstraintTest, solve) {
  Constraints CS;

  EXPECT_TRUE(CS.addConstraint(CS.createEq(CS.getOrCreateVar(0), CS.getWild())));
  EXPECT_TRUE(CS.addConstraint(CS.createNot(CS.createEq(CS.getOrCreateVar(2), CS.getPtr()))));
  EXPECT_TRUE(CS.addConstraint(CS.createImplies(CS.createEq(CS.getOrCreateVar(0), CS.getWild()),
                                CS.createEq(CS.getOrCreateVar(1), CS.getWild()))));
  EXPECT_TRUE(CS.addConstraint(CS.createEq(CS.getOrCreateVar(3), CS.getOrCreateVar(2))));

  EXPECT_TRUE(CS.addConstraint(CS.createEq(CS.getOrCreateVar(4), CS.getOrCreateVar(5))));
  EXPECT_TRUE(CS.addConstraint(CS.createEq(CS.getOrCreateVar(5), CS.getOrCreateVar(4))));
  EXPECT_TRUE(CS.addConstraint(CS.createEq(CS.getOrCreateVar(4), CS.getWild())));
  EXPECT_TRUE(CS.addConstraint(CS.createEq(CS.getOrCreateVar(5), CS.getOrCreateVar(5))));
  EXPECT_TRUE(CS.addConstraint(CS.createEq(CS.getOrCreateVar(6), CS.getOrCreateVar(6))));
  EXPECT_TRUE(CS.addConstraint(CS.createImplies(CS.createEq(CS.getOrCreateVar(3), CS.getArr()),
  CS.createEq(CS.getOrCreateVar(7), CS.getArr()))));
  EXPECT_TRUE(CS.addConstraint(CS.createImplies(CS.createEq(CS.getOrCreateVar(3), CS.getWild()),
  CS.createEq(CS.getOrCreateVar(8), CS.getWild()))));

  EXPECT_TRUE(CS.solve().second);
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

  EXPECT_TRUE(CS.addConstraint(CS.createEq(CS.getOrCreateVar(0), CS.getWild())));
  EXPECT_TRUE(CS.addConstraint(CS.createEq(CS.getOrCreateVar(1), CS.getPtr())));
  EXPECT_TRUE(CS.addConstraint(CS.createEq(CS.getOrCreateVar(0), CS.getOrCreateVar(1))));

  EXPECT_TRUE(CS.solve().second);
  Constraints::EnvironmentMap env = CS.getVariables();

  EXPECT_TRUE(*env[CS.getVar(0)] == *CS.getWild());
  EXPECT_TRUE(*env[CS.getVar(1)] == *CS.getWild());
}

TEST(Conflicts, test1) {
  Constraints CS;

  // q_0 = PTR
  // q_0 != ARR
  // q_0 != WILD
  // q_1 = WILD
  // q_0 = q_1

  EXPECT_TRUE(CS.addConstraint(CS.createEq(CS.getOrCreateVar(0), CS.getPtr())));
  EXPECT_TRUE(CS.addConstraint(CS.createNot(CS.createEq(CS.getOrCreateVar(0), CS.getArr()))));
  EXPECT_TRUE(CS.addConstraint(CS.createNot(CS.createEq(CS.getOrCreateVar(0), CS.getWild()))));
  EXPECT_TRUE(CS.addConstraint(CS.createEq(CS.getOrCreateVar(1), CS.getWild())));
  EXPECT_TRUE(CS.addConstraint(CS.createEq(CS.getOrCreateVar(0), CS.getOrCreateVar(1))));

  EXPECT_TRUE(CS.solve().second);
  Constraints::EnvironmentMap env = CS.getVariables();
  EXPECT_TRUE(*env[CS.getVar(0)] == *CS.getWild());
}
