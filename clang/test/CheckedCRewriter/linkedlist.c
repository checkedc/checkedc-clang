// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

#include <stdio.h>

#include <stdlib.h> 

typedef unsigned long size_t;
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

typedef struct node Node;
typedef struct list List;
List * makelist();
void add(int data, List * list);
void delete(int data, List * list);
void display(List * list);
void reverse(List * list);
void destroy(List * list);
//CHECK: _Ptr<List> makelist(void);
//CHECK-NEXT: void add(int data, _Ptr<List> list);
//CHECK-NEXT: void delete(int data, _Ptr<List> list);
//CHECK-NEXT: void display(_Ptr<List> list);
//CHECK-NEXT: void reverse(_Ptr<List> list);
//CHECK-NEXT: void destroy(List * list);


struct node {

  int data;

  struct node * next;

};
//CHECK: struct node * next;


struct list {

  Node * head;

};



Node * createnode(int data);
//CHECK: Node *createnode(int data) : itype(_Ptr<Node>);



Node * createnode(int data){

  Node * newNode = malloc(sizeof(Node));

  if (!newNode) {

    return NULL;

  }

  newNode->data = data;

  newNode->next = NULL;

  return newNode;

}
//CHECK: Node *createnode(int data) : itype(_Ptr<Node>){
//CHECK: _Ptr<Node> newNode =  malloc<Node>(sizeof(Node));



List * makelist(){

  List * list = malloc(sizeof(List));

  if (!list) {

    return NULL;

  }

  list->head = NULL;

  return list;

}
//CHECK: _Ptr<List> makelist(void){
//CHECK: _Ptr<List> list =  malloc<List>(sizeof(List));



void display(List * list) {

  Node * current = list->head;

  if(list->head == NULL)

    return;



  for(; current != NULL; current = current->next) {

    printf("%d\n", current->data);

  }

}
//CHECK: void display(_Ptr<List> list) {
//CHECK: Node * current = list->head;


void add(int data, List * list){

  Node * current = NULL;

  if(list->head == NULL){

    list->head = createnode(data);

  }

  else {

    current = list->head;

    while (current->next!=NULL){

      current = current->next;

    }

    current->next = createnode(data);

  }

}
//CHECK: void add(int data, _Ptr<List> list){
//CHECK: Node * current = NULL;


void delete(int data, List * list){

  Node * current = list->head;

  Node * previous = current;

  while(current != NULL){

    if(current->data == data){

      previous->next = current->next;

      if(current == list->head)

        list->head = current->next;

      free(current);

      return;

    }

    previous = current;

    current = current->next;

  }

}

//CHECK: void delete(int data, _Ptr<List> list){


void reverse(List * list){

  Node * reversed = NULL;

  Node * current = list->head;

  Node * temp = NULL;

  while(current != NULL){

    temp = current;

    current = current->next;

    temp->next = reversed;

    reversed = temp;

  }

  list->head = reversed;

}
//CHECK: void reverse(_Ptr<List> list){



void destroy(List * list){

  Node * current = list->head;

  Node * next = current;

  while(current != NULL){
    next = current->next;
    free(current);
    current = next;
  }

  free(list);
}
//CHECK: void destroy(List * list){
//CHECK: Node * current = list->head;
//CHECK: Node * next = current;
