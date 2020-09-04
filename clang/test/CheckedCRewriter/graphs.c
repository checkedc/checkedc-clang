// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/graphs.checked.c -- | diff -w %S/graphs.checked.c -
// RUN: rm %S/graphs.checked.c

#include <stdio.h>

#include <stdlib.h>

#include <stddef.h>
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);

#define MAX_SIZE 40/*Assume 40 nodes at max in graph*/
#define INT_MIN 0 


/*A vertex of the graph*/

struct node

{

    int vertex;

    struct node* next;
	//CHECK_NOALL: struct node* next;
	//CHECK_ALL:     _Ptr<struct node> next;

};

/*Some declarations*/

struct node* createNode(int v);
	//CHECK_NOALL: struct node *createNode(int v) : itype(_Ptr<struct node>);
	//CHECK_ALL: _Ptr<struct node> createNode(int v);

struct Graph

{

    int numVertices;

    int* visited;
	//CHECK_NOALL: int* visited;
	//CHECK_ALL:     _Array_ptr<int> visited : count(numVertices);

	/* we need int** to store a two dimensional array. Similary, we need struct node** to store an array of Linked lists */
    struct node** adjLists;
	//CHECK_NOALL: struct node** adjLists;
	//CHECK_ALL:     _Array_ptr<_Ptr<struct node>> adjLists : count(numVertices);

};

/*Structure to create a stack, necessary for topological sorting*/

struct Stack

{

	int arr[MAX_SIZE];
	//CHECK_NOALL: int arr[MAX_SIZE];
	//CHECK_ALL: 	int arr _Checked[40];

	int top;

};

struct Graph* createGraph(int);
	//CHECK: _Ptr<struct Graph> createGraph(int vertices);

void addEdge(struct Graph*, int, int);
	//CHECK: void addEdge(_Ptr<struct Graph> graph, int src, int dest);

void printGraph(struct Graph*);
	//CHECK: void printGraph(_Ptr<struct Graph> graph);

void topologicalSortHelper(int,struct Graph*, struct Stack*);
	//CHECK: void topologicalSortHelper(int vertex, _Ptr<struct Graph> graph, _Ptr<struct Stack> stack);

void topologicalSort(struct Graph*);
	//CHECK: void topologicalSort(_Ptr<struct Graph> graph);

struct Stack* createStack();
	//CHECK: _Ptr<struct Stack> createStack(void);

void push(struct Stack*, int);
	//CHECK: void push(_Ptr<struct Stack> stack, int element);

int pop(struct Stack*);
	//CHECK: int pop(_Ptr<struct Stack> stack);




int main()

{

	int vertices,edges,i,src,dst;

	printf("Enter the number of vertices\n");

	scanf("%d",&vertices);

	struct Graph* graph = createGraph(vertices);
	//CHECK: _Ptr<struct Graph> graph =  createGraph(vertices);

	printf("Enter the number of edges\n");

	scanf("%d",&edges);

	for(i=0; i<edges; i++)

	{

		printf("Edge %d \nEnter source: ",i+1);

		scanf("%d",&src);

		printf("Enter destination: ");

		scanf("%d",&dst);

		addEdge(graph, src, dst);

	}

	printf("One topological sort order is:\n");

	topologicalSort(graph);

	printf("\n");



	/*Uncomment below part to get a ready-made example*/

    struct Graph* graph2 = createGraph(4);
	//CHECK: _Ptr<struct Graph> graph2 =  createGraph(4);

    addEdge(graph2, 0, 1);

    addEdge(graph2, 0, 2);

    addEdge(graph2, 1, 2);

    addEdge(graph2, 2, 3);

    printf("One topological sort is:\n");

    topologicalSort(graph2);

	printf("\n");

    return 0;

}


void topologicalSortHelper(int vertex, struct Graph* graph, struct Stack* stack)
	//CHECK: void topologicalSortHelper(int vertex, _Ptr<struct Graph> graph, _Ptr<struct Stack> stack)

{

	graph->visited[vertex]=1;

	struct node* adjList = graph->adjLists[vertex];
	//CHECK_NOALL: struct node* adjList = graph->adjLists[vertex];
	//CHECK_ALL: 	_Ptr<struct node> adjList =  graph->adjLists[vertex];

    struct node* temp = adjList;
	//CHECK_NOALL: struct node* temp = adjList;
	//CHECK_ALL:     _Ptr<struct node> temp =  adjList;

    /*First add all dependents (that is, children) to stack*/

    while(temp!=NULL) {
	//CHECK_NOALL: while(temp!=NULL) {
	//CHECK_ALL:     while(temp!=NULL) _Checked {

        int connectedVertex = temp->vertex;

        if(graph->visited[connectedVertex] == 0) {
	//CHECK_NOALL: if(graph->visited[connectedVertex] == 0) {
	//CHECK_ALL:         if(graph->visited[connectedVertex] == 0) _Unchecked {

               topologicalSortHelper(connectedVertex, graph, stack);

            }

        temp=temp->next;

    }

    /*and then add itself*/

    push(stack,vertex);

}



/*Recursive topologial sort approach*/

void topologicalSort(struct Graph* graph)
	//CHECK: void topologicalSort(_Ptr<struct Graph> graph)

{

	struct Stack* stack=createStack();
	//CHECK: _Ptr<struct Stack> stack =createStack();

	int i=0;

	for(i=0;i<graph->numVertices;i++)

	{
	//CHECK_NOALL: {
	//CHECK_ALL: 	_Checked {

		/*Execute topological sort on all elements*/

		if(graph->visited[i]==0)

		{
	//CHECK_NOALL: {
	//CHECK_ALL: 		_Unchecked {

			topologicalSortHelper(i,graph,stack);

		}

	}

	while(stack->top!=-1)

	printf("%d ",pop(stack));

}



/*Allocate memory for a node*/

struct node* createNode(int v)
	//CHECK_NOALL: struct node *createNode(int v) : itype(_Ptr<struct node>)
	//CHECK_ALL: _Ptr<struct node> createNode(int v)

{

    struct node* newNode = malloc(sizeof(struct node));
	//CHECK: _Ptr<struct node> newNode =  malloc<struct node>(sizeof(struct node));

    newNode->vertex = v;

    newNode->next = NULL;

    return newNode;

}

/*Allocate memory for the entire graph structure*/

struct Graph* createGraph(int vertices)
	//CHECK: _Ptr<struct Graph> createGraph(int vertices)

{

    struct Graph* graph = malloc(sizeof(struct Graph));
	//CHECK: _Ptr<struct Graph> graph =  malloc<struct Graph>(sizeof(struct Graph));

    graph->numVertices = vertices;

    graph->adjLists = malloc(vertices * sizeof(struct node*));
	//CHECK_NOALL: graph->adjLists = malloc<struct node *>(vertices * sizeof(struct node*));
	//CHECK_ALL:     graph->adjLists = malloc<_Ptr<struct node>>(vertices * sizeof(struct node*));

    graph->visited = malloc(vertices * sizeof(int));
	//CHECK: graph->visited = malloc<int>(vertices * sizeof(int));



    int i;

    for (i = 0; i < vertices; i++) {
	//CHECK_NOALL: for (i = 0; i < vertices; i++) {
	//CHECK_ALL:     for (i = 0; i < vertices; i++) {

        graph->adjLists[i] = NULL;

        graph->visited[i] = 0;

    }

    return graph;

}

/*Creates a unidirectional graph*/

void addEdge(struct Graph* graph, int src, int dest)
	//CHECK: void addEdge(_Ptr<struct Graph> graph, int src, int dest)

{
	//CHECK_NOALL: {
	//CHECK_ALL: _Checked {

    /* Add edge from src to dest*/

    struct node* newNode = createNode(dest);
	//CHECK_NOALL: struct node* newNode = createNode(dest);
	//CHECK_ALL:     _Ptr<struct node> newNode =  createNode(dest);

    newNode->next = graph->adjLists[src];

    graph->adjLists[src] = newNode;

}

/*Utility function to see state of graph at a given time*/

void printGraph(struct Graph* graph)
	//CHECK: void printGraph(_Ptr<struct Graph> graph)

{
	//CHECK_NOALL: {
	//CHECK_ALL: _Checked {

    int v;

    for (v = 0; v < graph->numVertices; v++)

    {
	//CHECK_NOALL: {
	//CHECK_ALL:     _Unchecked {

        struct node* temp = graph->adjLists[v];
	//CHECK_NOALL: struct node* temp = graph->adjLists[v];
	//CHECK_ALL:         _Ptr<struct node> temp =  graph->adjLists[v];

        printf("\n Adjacency list of vertex %d\n ", v);

        while (temp)

        {

            printf("%d -> ", temp->vertex);

            temp = temp->next;

        }

        printf("\n");

    }

}

/*Creates a stack*/

struct Stack* createStack()
	//CHECK: _Ptr<struct Stack> createStack(void)

{

	struct Stack* stack=malloc(sizeof(struct Stack));
	//CHECK: _Ptr<struct Stack> stack =malloc<struct Stack>(sizeof(struct Stack));

	stack->top=-1;
    return stack;

}

/*Pushes element into stack*/

void push(struct Stack* stack,int element)
	//CHECK: void push(_Ptr<struct Stack> stack, int element)

{
	//CHECK_NOALL: {
	//CHECK_ALL: _Checked {

	stack->arr[++stack->top]=element;/*Increment then add, as we start from -1*/

}

/*Removes element from stack, or returns INT_MIN if stack empty*/

int pop(struct Stack* stack)
	//CHECK: int pop(_Ptr<struct Stack> stack)

{
	//CHECK_NOALL: {
	//CHECK_ALL: _Checked {

	if(stack->top==-1)

		return INT_MIN;

	else

		return stack->arr[stack->top--];

}
