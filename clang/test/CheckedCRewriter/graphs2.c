// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

#include<stdio.h>

#include<stdlib.h>

#include<limits.h>

#include<string.h>

#include <stddef.h>
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

//Structure for storing a graph

struct Graph{

	int vertexNum;

	int** edges;

};



//Constructs a graph with V vertices and E edges

void createGraph(struct Graph* G,int V){

		G->vertexNum = V;

		int ** toadd = malloc(V * sizeof(int*));
		G->edges = toadd;

		for(int i=0; i<V; i++){
			int *adder = malloc(V * sizeof(int));
			G->edges[i] = adder;

			for(int j=0; j<V; j++)

				G->edges[i][j] = INT_MAX;

			G->edges[i][i] = 0;

		}

}
//CHECK: void createGraph(_Ptr<struct Graph> G, int V){
//CHECK_NOALL: int ** toadd = malloc<int *>(V * sizeof(int*));
//CHECK_NOALL: int *adder = malloc<int>(V * sizeof(int));
//CHECK_ALL: _Array_ptr<_Array_ptr<int>> toadd : count(V) =  malloc<int *>(V * sizeof(int*)); 
//CHECK_ALL: _Array_ptr<int> adder : count(V) =  malloc<int>(V * sizeof(int));
