#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#define INT_MODE 0
#define STR_MODE 1
#define STR_MAX  32

/*
 * Structre for each node of the linkedList
 *
 * value - a pointer to the data of the node. 
 * next - a pointer to the next node in the list. 
 */
typedef struct node {
    char username[STR_MAX];
    int user_fd;
    struct node* next;
} node_t;

/*
 * Structure for the base linkedList
 * 
 * head - a pointer to the first node in the list. NULL if length is 0.
 * length - the current length of the linkedList. Must be initialized to 0.
 * comparator - function pointer to linkedList comparator. Must be initialized!
 */
typedef struct userlist {
    node_t* head;
    int length;
} List_t;

/* 
 * Each of these functions inserts the reference to the data (valref)
 * into the linkedList list at the specified position
 *
 * @param list pointer to the linkedList struct
 * @param valref pointer to the data to insert into the linkedList
 */
void insertFront(List_t* list, char* un, int fd);
void addUser(List_t* list, char* un, int fd);

/*
 * Each of these functions removes a single linkedList node from
 * the LinkedList at the specfied function position.
 * @param list pointer to the linkedList struct
 */ 
void removeFront(List_t* list);
void removeRear(List_t* list);
void removeByIndex(List_t* list, int n);

/* 
 * Free all nodes from the linkedList
 *
 * @param list pointer to the linkedList struct
 */
void deleteList(List_t* list);

/*
 * Traverse the list printing each node in the current order.
 * @param list pointer to the linkedList strut
 * @param mode STR_MODE to print node.value as a string,
 * INT_MODE to print node.value as an int
 */
void printList(List_t* list);

int getIndexByFD(List_t* list, int fd);

node_t* getNode(List_t* list, int index);

int nameExists(List_t* list, char* name);

#endif
