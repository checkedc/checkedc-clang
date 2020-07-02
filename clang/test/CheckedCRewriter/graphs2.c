// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s

#include<stdio.h>

#include<stdlib.h>

#include<limits.h>

#include<string.h>





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
//CHECK: int ** toadd = malloc<int *>(V * sizeof(int*));
//CHECK: int *adder = malloc<int>(V * sizeof(int));
