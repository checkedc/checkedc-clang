//===- unittests/Basic/CharInfoTest.cpp -- ASCII classification tests -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/VarEquiv.h"
#include "gtest/gtest.h"
#include <regex>

using namespace llvm;
using namespace clang;
using namespace PartitionRefinement;

// Test individual operations on a partition with one to three sets.
TEST(PartitionRefinementTest, individualElementOperations) {
  Partition *P1 = new Partition();
  P1->add(0, 1);
  EXPECT_EQ(P1->getRepresentative(0), P1->getRepresentative(1));
  EXPECT_NE(P1->getRepresentative(0), P1->getRepresentative(3));
  EXPECT_FALSE(P1->isSingleton(0));
  EXPECT_FALSE(P1->isSingleton(1));
  EXPECT_TRUE(P1->isSingleton(2));
  EXPECT_TRUE(P1->isSingleton(3));
  EXPECT_TRUE(P1->isSingleton(4));

  P1->makeSingleton(0);

  EXPECT_TRUE(P1->isSingleton(0));
  EXPECT_TRUE(P1->isSingleton(1));
  EXPECT_TRUE(P1->isSingleton(2));
  EXPECT_TRUE(P1->isSingleton(3));
  EXPECT_TRUE(P1->isSingleton(4));

  EXPECT_EQ(0, P1->getRepresentative(0));
  EXPECT_EQ(1, P1->getRepresentative(1));
  EXPECT_EQ(2, P1->getRepresentative(2));
  EXPECT_EQ(3, P1->getRepresentative(3));
  EXPECT_EQ(4, P1->getRepresentative(4));

  P1->add(0, 0);
  EXPECT_TRUE(P1->isSingleton(0));

  // place {0, 1, 2, 3} in the same equivalence set.
  P1->add(0, 1);
  P1->add(1, 2);
  P1->add(2, 3);
  EXPECT_EQ(P1->getRepresentative(0), P1->getRepresentative(0));
  EXPECT_EQ(P1->getRepresentative(0), P1->getRepresentative(1));
  EXPECT_EQ(P1->getRepresentative(0), P1->getRepresentative(2));
  EXPECT_EQ(P1->getRepresentative(0), P1->getRepresentative(3));
  EXPECT_NE(P1->getRepresentative(0), P1->getRepresentative(4));

  EXPECT_EQ(P1->getRepresentative(1), P1->getRepresentative(0));
  EXPECT_EQ(P1->getRepresentative(1), P1->getRepresentative(1));
  EXPECT_EQ(P1->getRepresentative(1), P1->getRepresentative(2));
  EXPECT_EQ(P1->getRepresentative(1), P1->getRepresentative(3));
  EXPECT_NE(P1->getRepresentative(1), P1->getRepresentative(4));

  EXPECT_EQ(P1->getRepresentative(2), P1->getRepresentative(0));
  EXPECT_EQ(P1->getRepresentative(2), P1->getRepresentative(1));
  EXPECT_EQ(P1->getRepresentative(2), P1->getRepresentative(2));
  EXPECT_EQ(P1->getRepresentative(2), P1->getRepresentative(3));
  EXPECT_NE(P1->getRepresentative(2), P1->getRepresentative(4));

  EXPECT_NE(P1->getRepresentative(4), P1->getRepresentative(0));
  EXPECT_NE(P1->getRepresentative(4), P1->getRepresentative(1));
  EXPECT_NE(P1->getRepresentative(4), P1->getRepresentative(2));
  EXPECT_NE(P1->getRepresentative(4), P1->getRepresentative(3));
  EXPECT_EQ(P1->getRepresentative(4), P1->getRepresentative(4));

  // Place {1} in its own individual equivalance set.
  P1->makeSingleton(1);
  EXPECT_TRUE(P1->isSingleton(1));

  EXPECT_NE(P1->getRepresentative(0), P1->getRepresentative(1));
  EXPECT_NE(P1->getRepresentative(2), P1->getRepresentative(1));
  EXPECT_NE(P1->getRepresentative(3), P1->getRepresentative(1));

  EXPECT_EQ(P1->getRepresentative(0), P1->getRepresentative(0));
  EXPECT_EQ(P1->getRepresentative(2), P1->getRepresentative(0));
  EXPECT_EQ(P1->getRepresentative(3), P1->getRepresentative(0));

  EXPECT_EQ(P1->getRepresentative(0), P1->getRepresentative(2));
  EXPECT_EQ(P1->getRepresentative(2), P1->getRepresentative(2));
  EXPECT_EQ(P1->getRepresentative(3), P1->getRepresentative(2));

  EXPECT_EQ(P1->getRepresentative(0), P1->getRepresentative(3));
  EXPECT_EQ(P1->getRepresentative(2), P1->getRepresentative(3));
  EXPECT_EQ(P1->getRepresentative(3), P1->getRepresentative(3));

  // Make equivalence sets be {1, 4} and {0, 2, 3}.
  P1->add(1, 4);
  EXPECT_EQ(P1->getRepresentative(0), P1->getRepresentative(0));
  EXPECT_NE(P1->getRepresentative(0), P1->getRepresentative(1));
  EXPECT_EQ(P1->getRepresentative(0), P1->getRepresentative(2));
  EXPECT_EQ(P1->getRepresentative(0), P1->getRepresentative(3));
  EXPECT_NE(P1->getRepresentative(0), P1->getRepresentative(4));

  EXPECT_NE(P1->getRepresentative(1), P1->getRepresentative(0));
  EXPECT_EQ(P1->getRepresentative(1), P1->getRepresentative(1));
  EXPECT_NE(P1->getRepresentative(1), P1->getRepresentative(2));
  EXPECT_NE(P1->getRepresentative(1), P1->getRepresentative(3));
  EXPECT_EQ(P1->getRepresentative(1), P1->getRepresentative(4));

  EXPECT_EQ(P1->getRepresentative(2), P1->getRepresentative(0));
  EXPECT_NE(P1->getRepresentative(2), P1->getRepresentative(1));
  EXPECT_EQ(P1->getRepresentative(2), P1->getRepresentative(2));
  EXPECT_EQ(P1->getRepresentative(2), P1->getRepresentative(3));
  EXPECT_NE(P1->getRepresentative(2), P1->getRepresentative(4));

  EXPECT_EQ(P1->getRepresentative(3), P1->getRepresentative(0));
  EXPECT_NE(P1->getRepresentative(3), P1->getRepresentative(1));
  EXPECT_EQ(P1->getRepresentative(3), P1->getRepresentative(2));
  EXPECT_EQ(P1->getRepresentative(3), P1->getRepresentative(3));
  EXPECT_NE(P1->getRepresentative(3), P1->getRepresentative(4));

  EXPECT_NE(P1->getRepresentative(4), P1->getRepresentative(0));
  EXPECT_EQ(P1->getRepresentative(4), P1->getRepresentative(1));
  EXPECT_NE(P1->getRepresentative(4), P1->getRepresentative(2));
  EXPECT_NE(P1->getRepresentative(4), P1->getRepresentative(3));
  EXPECT_EQ(P1->getRepresentative(4), P1->getRepresentative(4));

  // Make equivalence sets be {1}, {4}, {0, 2, 3}
  P1->makeSingleton(1);
  EXPECT_FALSE(P1->isSingleton(0));
  EXPECT_TRUE(P1->isSingleton(1));
  EXPECT_FALSE(P1->isSingleton(2));
  EXPECT_FALSE(P1->isSingleton(3));
  EXPECT_TRUE(P1->isSingleton(4));

  delete P1;
}

// Test case where the order in which elements are added to an
// equivalence set does not match the order in which they are 
// removed from an equivalence set.
TEST(PartitionRefinementTest, OrderDifference) {
  const int size = 20;  // must be even
  assert(size % 2 == 0);

  Partition *P1 = new Partition();
  // Put everything into one set.
  for (int i = 0; i < size/2; i++) {
    P1->add(0, i);
    P1->add(0, size - 1 - i);
  }
  // Adding elements again should be OK
  for (int i = 0; i < size; i++)
    P1->add(i, i + 1);

  for (int i = 0; i < size; i++)
    for (int j = 0; j < size; j++) {
      EXPECT_EQ(P1->getRepresentative(i), 
                P1->getRepresentative(j));
    }

  // Place even elements into singleton sets.
  for (int i = 0; i < size; i += 2)
    P1->makeSingleton(i);

  // Test that all odd elements are in the same set.
  // and that all even elements are not in the same
  // set as odd elements.
  for (int i = 1; i < size; i += 2)
    for (int j = 1; j < size; j += 2) {
      EXPECT_EQ(P1->getRepresentative(i), 
                P1->getRepresentative(j));
      EXPECT_NE(P1->getRepresentative(i),
                P1->getRepresentative(j - 1));
    }
  
  // Test that even elements are not in the same set.
  for (int i = 0; i < size; i += 2) {
    for (int j = 0; j < size; j += 2) {
      if (i == j) 
        EXPECT_EQ(P1->getRepresentative(i),
                  P1->getRepresentative(j));
      else
        EXPECT_NE(P1->getRepresentative(i),
                  P1->getRepresentative(j));
    }
  }

  delete P1;
}

// Test case where the order in which elements are added to an
// equivalence set is the same order in which they are removed
// from an equivalence set.
TEST(PartitionRefinementTest, SameOrder) {
  const int size = 20;

  Partition *P1 = new Partition();

  // Put everything into one set.
  for (int i = 0; i < size; i++)
    P1->add(0, i);

  for (int i = 0; i < size; i++)
    for (int j = 0; j < size; j++)
      EXPECT_EQ(P1->getRepresentative(i),
                P1->getRepresentative(j));

  // Make elements into singletons in the same order.
  for (int k = 0; k < size; k++) {
    P1->makeSingleton(k);
    for (int i = 0; i < size; i++)
      for (int j = 0; j < size; j++)
        if (i == j)
          EXPECT_EQ(P1->getRepresentative(i),
                    P1->getRepresentative(j));
        else if (i <= k || j <= k)
          EXPECT_NE(P1->getRepresentative(i),
                    P1->getRepresentative(j));
        else
         EXPECT_EQ(P1->getRepresentative(i),
                   P1->getRepresentative(j));
  }

  delete P1;
}

// Test case where the order in which elements are added to an
// equivalence set is the reverse order in which they are removed
// from an equivalence set.
TEST(PartitionRefinementTest, ReverseOrder) {
  const int size = 20;

  Partition *P1 = new Partition();

  // Put everything into one set.
  for (int i = 0; i < size; i++)
    P1->add(0, i);

  for (int i = 0; i < size; i++)
    for (int j = 0; j < size; j++)
      EXPECT_EQ(P1->getRepresentative(i),
                P1->getRepresentative(j));

  // Make elements into singletons in reverse order
  for (int k = size - 1; k >= 0; k--) {
    P1->makeSingleton(k);
    for (int i = 0; i < size; i++)
      for (int j = 0; j < size; j++)
        if (i == j)
          EXPECT_EQ(P1->getRepresentative(i),
                    P1->getRepresentative(j));
        else if (i >= k || j >= k)
          EXPECT_NE(P1->getRepresentative(i),
                    P1->getRepresentative(j));
        else
          EXPECT_EQ(P1->getRepresentative(i),
                    P1->getRepresentative(j));
  }

  delete P1;
}


static void initPartition1(Partition &P) {
  // {0, 4, 5, 6}, {1, 2, 3}, {7, 9, 10}, {12, 18, 19}
  // everything else is a singleton.
  P.add(0, 4);
  P.add(0, 5);
  P.add(0, 6);
  P.add(1, 2);
  P.add(1, 3);
  P.add(7, 9);
  P.add(9, 10);
  P.add(12, 18);
  P.add(18, 19);
}

static void initPartition2(Partition &P) {
  // {0, 1}, {7, 9}, {12, 18}, {14, 15}}
  // Everything else is a singleton.
  P.add(7, 9);
  P.add(12, 18);
  P.add(14, 15);
  P.add(0, 1);
}

// The refinement of Partition 1 by Partition 2 is
// {7, 9} {12, 18}.  Everything else is a singleton

static int isSet1(int i) {
  return i == 0 || i == 4 || i == 5 || i == 6;
}

static int isSet2(int i) {
  return i == 1 || i == 2 || i == 3;
}

static int isSet3(int i) {
  return i == 7 || i == 9 || i == 10;
}

static int isSet4(int i) {
  return i == 12 || i == 18 || i == 19;
}

static void checkPartition1(Partition &P) {
  for (int i = 0; i < 20; i++) 
    for (int j = 0; j < 20; j++)
      if (i == j || 
          (isSet1(i) && isSet1(j)) ||
          (isSet2(i) && isSet2(j)) ||
          (isSet3(i) && isSet3(j)) ||
          (isSet4(i) && isSet4(j)))
        EXPECT_EQ(P.getRepresentative(i),
                  P.getRepresentative(j));
      else
        EXPECT_NE(P.getRepresentative(i),
                  P.getRepresentative(j));
}

static int isIntersectedSet1(int i) {
    return i == 7 || i == 9;
}

static int isIntersectedSet2(int i) {
  return i == 12 || i == 18;
}

static void checkIntersectedPartitions(Partition &P) {
  for (int i = 0; i < 20; i++)
    for (int j = 0; j < 20; j++)
      if (i == j || 
          (isIntersectedSet1(i) && isIntersectedSet1(j)) ||
          (isIntersectedSet2(i) && isIntersectedSet2(j)))
        EXPECT_EQ(P.getRepresentative(i),
                  P.getRepresentative(j));
      else
        EXPECT_NE(P.getRepresentative(i),
                  P.getRepresentative(j));

  for (int i = 0; i < 20; i++)
    if (!isIntersectedSet1(i) && !isIntersectedSet2(i))
      EXPECT_TRUE(P.isSingleton(i));
}

TEST(PartitionRefinementTest, SimplePartition) {
  Partition P1;
  Partition P2;
  Partition P3;
  initPartition1(P1);
  initPartition1(P2);
  // By default, everything in P3 in a singleton.
  // After refinement, everything in P1 should be a singleton.
  P1.refine(&P3);
  for (int i = 0; i < 20; i++)
    EXPECT_TRUE(P1.isSingleton(i));

 initPartition1(P1);
 // P1 and P2 are the same partition.  Refining one by
 // the other should result in identical partitions.
 P1.refine(&P2);
 for (int i=0; i<20; i++)
   for (int j=0; j<20; j++)
     EXPECT_EQ(P1.getRepresentative(i) == P1.getRepresentative(j),
               P2.getRepresentative(i) == P2.getRepresentative(j));
 checkPartition1(P1);
 checkPartition1(P2);

 initPartition2(P3);
 P1.refine(&P3);
 checkIntersectedPartitions(P1);
}

TEST(PartitionRefinementTest, DumpTrivial) {
  Partition P1;
  const std::string TrivialMessage = "Equivalence classes are all trivial\n";
  std::string Result;
  raw_string_ostream OS(Result);
  P1.dump(OS);
  OS.flush();
  EXPECT_EQ(TrivialMessage, Result);
}

TEST(PartitionRefinementTest, DumpSingleton) {
  Partition P1;
  const std::string Expected = "Equivalence classes are all trivial\n";
  std::string Result;
  raw_string_ostream OS(Result);
  P1.dump(OS);
  OS.flush();
  EXPECT_EQ(Expected, Result);
}

TEST(PartitionRefinementTest, DumpElementSet1) {
  Partition P1;
  const std::string Output = "10: Itself";
  std::string Result;
  raw_string_ostream OS(Result);
  P1.dump(OS, 10);
  OS.flush();
  EXPECT_EQ(Output, Result);
}

TEST(PartitionRefinementTest, DumpElementSet2) {
  Partition P1;
  P1.add(10, 15);
  std::string Result;
  raw_string_ostream OS(Result);
  P1.dump(OS, 10);
  // We expect a string of the form 10: Set (Internal Id {0-9}+) {10, 15},
  // where the 10 and 15 between the brackets can appear in any order.
  std::regex Output
   ("10: Set \\(Internal Id [0-9]+\\) \\{(10, 15)|(15, 10)\\}");
  OS.flush();
  EXPECT_TRUE(std::regex_search(Result, Output));
}

TEST(PartitionRefinementTest, DumpAll) {
  Partition P1;
  P1.add(10, 15);
  std::string Result;
  raw_string_ostream OS(Result);
  P1.dump(OS, 10);
  // We expect a string of the form 10: Set (Internal Id {0-9}+) {10, 15},
  // where the 10 and 15 between the brackets can appear in any order.
  std::regex Output("Non-trivial equivalence classes:\n"
                    "10: Set \\(Internal Id [0-9]+\\) "
                    "\\{(10, 15)|(15, 10)\\}");
  OS.flush();
  EXPECT_TRUE(std::regex_search(Result, Output));
}
