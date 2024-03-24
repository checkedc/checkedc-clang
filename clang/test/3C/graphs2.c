// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/graphs2.c -- | diff %t.checked/graphs2.c -

#include <stdio.h>

#include <stdlib.h>

#include <limits.h>

#include <string.h>

#include <stddef.h>

/*Structure for storing a graph*/

struct Graph {

  int vertexNum;

  int **edges;
  //CHECK_NOALL: int **edges;
  //CHECK_ALL: _Array_ptr<_Array_ptr<int>> edges : count(vertexNum);
};

/*Constructs a graph with V vertices and E edges*/

void createGraph(struct Graph *G, int V) {
  //CHECK: void createGraph(_Ptr<struct Graph> G, int V) {

  G->vertexNum = V;

  int **toadd = malloc(V * sizeof(int *));
  //CHECK_NOALL: int **toadd = malloc<int *>(V * sizeof(int *));
  //CHECK_ALL: _Array_ptr<_Array_ptr<int>> toadd : count(V) = malloc<_Array_ptr<int>>(V * sizeof(int *));
  G->edges = toadd;

  for (int i = 0; i < V; i++) {
    int *adder = malloc(V * sizeof(int));
    //CHECK_NOALL: int *adder = malloc<int>(V * sizeof(int));
    //CHECK_ALL: _Array_ptr<int> adder : count(V) = malloc<int>(V * sizeof(int));
    G->edges[i] = adder;

    for (int j = 0; j < V; j++)

      G->edges[i][j] = INT_MAX;

    G->edges[i][i] = 0;
  }
}
