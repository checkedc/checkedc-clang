// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/linkedlist.c -- | diff %t.checked/linkedlist.c -

#include <stdio.h>

#include <stdlib.h>

#include <stddef.h>

typedef struct node Node;
typedef struct list List;
List *makelist();
//CHECK: _Ptr<List> makelist(void);
void add(int data, List *list);
//CHECK: void add(int data, _Ptr<List> list);
void delete (int data, List *list);
//CHECK: void delete (int data, _Ptr<List> list);
void display(List *list);
//CHECK: void display(_Ptr<List> list);
void reverse(List *list);
//CHECK: void reverse(_Ptr<List> list);
void destroy(List *list);
//CHECK: void destroy(_Ptr<List> list);

struct node {

  int data;

  struct node *next;
  //CHECK: _Ptr<struct node> next;
};

struct list {

  Node *head;
  //CHECK: _Ptr<Node> head;
};

Node *createnode(int data);
//CHECK: _Ptr<Node> createnode(int data);

Node *createnode(int data) {
  //CHECK: _Ptr<Node> createnode(int data) {

  Node *newNode = malloc(sizeof(Node));
  //CHECK: _Ptr<Node> newNode = malloc<Node>(sizeof(Node));

  if (!newNode) {

    return NULL;
  }

  newNode->data = data;

  newNode->next = NULL;

  return newNode;
}

List *makelist() {
  //CHECK: _Ptr<List> makelist(void) {

  List *list = malloc(sizeof(List));
  //CHECK: _Ptr<List> list = malloc<List>(sizeof(List));

  if (!list) {

    return NULL;
  }

  list->head = NULL;

  return list;
}

void display(List *list) {
  //CHECK: void display(_Ptr<List> list) {

  Node *current = list->head;
  //CHECK: _Ptr<Node> current = list->head;

  if (list->head == NULL)

    return;

  for (; current != NULL; current = current->next) {

    printf("%d\n", current->data);
  }
}

void add(int data, List *list) {
  //CHECK: void add(int data, _Ptr<List> list) {

  Node *current = NULL;
  //CHECK: _Ptr<Node> current = NULL;

  if (list->head == NULL) {
    //CHECK: if (list->head == NULL) _Checked {

    list->head = createnode(data);

  }

  else {

    current = list->head;

    while (current->next != NULL) {
      //CHECK: while (current->next != NULL) _Checked {

      current = current->next;
    }

    current->next = createnode(data);
  }
}

void delete (int data, List *list) {
  //CHECK: void delete (int data, _Ptr<List> list) {

  Node *current = list->head;
  //CHECK: _Ptr<Node> current = list->head;

  Node *previous = current;
  //CHECK: _Ptr<Node> previous = current;

  while (current != NULL) {
    //CHECK: while (current != NULL) _Checked {

    if (current->data == data) {

      previous->next = current->next;

      if (current == list->head)

        list->head = current->next;

      free(current);

      return;
    }

    previous = current;

    current = current->next;
  }
}

void reverse(List *list) {
  //CHECK: void reverse(_Ptr<List> list) {

  Node *reversed = NULL;
  //CHECK: _Ptr<Node> reversed = NULL;

  Node *current = list->head;
  //CHECK: _Ptr<Node> current = list->head;

  Node *temp = NULL;
  //CHECK: _Ptr<Node> temp = NULL;

  while (current != NULL) {
    //CHECK: while (current != NULL) _Checked {

    temp = current;

    current = current->next;

    temp->next = reversed;

    reversed = temp;
  }

  list->head = reversed;
}

void destroy(List *list) {
  //CHECK: void destroy(_Ptr<List> list) {

  Node *current = list->head;
  //CHECK: _Ptr<Node> current = list->head;

  Node *next = current;
  //CHECK: _Ptr<Node> next = current;

  while (current != NULL) {
    //CHECK: while (current != NULL) _Checked {
    next = current->next;
    free(current);
    current = next;
  }

  free(list);
}
