// RUN: CConvertStandalone %s -- | FileCheck -match-full-lines %s

#include <stdio.h>

#include <stdlib.h>

#include "linkedlist.h"



struct node {

  int data;

  struct node * next;

};
//CHECK: struct node * next;


struct list {

  Node * head;

};



Node * createnode(int data);
//CHECK: Node * createnode(int data);



Node * createnode(int data){

  Node * newNode = malloc(sizeof(Node));

  if (!newNode) {

    return NULL;

  }

  newNode->data = data;

  newNode->next = NULL;

  return newNode;

}
//CHECK: Node * newNode = malloc(sizeof(Node));



List * makelist(){

  List * list = malloc(sizeof(List));

  if (!list) {

    return NULL;

  }

  list->head = NULL;

  return list;

}
//CHECK: List * makelist(){
//CHECK: List * list = malloc(sizeof(List));



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
//CHECK: void destroy(List *list){
//CHECK: Node * current = list->head;
//CHECK: Node * next = current;
