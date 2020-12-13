#include "linkedList.h"
#include <string.h>
/*
    What is a linked list?
    A linked list is a set of dynamically allocated nodes, arranged in
    such a way that each node contains one value and one pointer.
    The pointer always points to the next member of the list.
    If the pointer is NULL, then it is the last node in the list.

    A linked list is held using a local pointer variable which
    points to the first item of the list. If that pointer is also NULL,
    then the list is considered to be empty.
    -------------------------------               ------------------------------              ------------------------------
    |HEAD                         |             \ |              |             |            \ |              |             |
    |                             |-------------- |     DATA     |     NEXT    |--------------|     DATA     |     NEXT    |
    |-----------------------------|             / |              |             |            / |              |             |
    |LENGTH                       |               ------------------------------              ------------------------------
    |COMPARATOR                   |
    |PRINTER                      |
    |DELETER                      |
    -------------------------------

*/

void insertFront(List_t* list, char* un, int fd) {
    if (list->length == 0)
        list->head = NULL;

    node_t** head = &(list->head);
    node_t* new_node;
    new_node = malloc(sizeof(node_t));

    strcpy(new_node->username, un);
    new_node->user_fd = fd;

    new_node->next = *head;
    *head = new_node;
    list->length++; 
}

void insertRear(List_t* list, char* un, int fd) {
    if (list->length == 0) {
        insertFront(list, un, fd);
        return;
    }

    node_t* head = list->head;
    node_t* current = head;
    while (current->next != NULL) {
        current = current->next;
    }

    current->next = malloc(sizeof(node_t));
    strcpy(current->next->username, un);
    current->next->user_fd = fd;
    current->next->next = NULL;
    list->length++;
}

void removeFront(List_t* list) {
    node_t** head = &(list->head);
    node_t* next_node = NULL;

    if (list->length == 0) {
        return;
    }

    next_node = (*head)->next;
    list->length--;

    node_t* temp = *head;
    *head = next_node;
    free(temp);
}

void removeRear(List_t* list) {
    if (list->length == 0) {
        return;
    } else if (list->length == 1) {
        removeFront(list);
		return;
    }

    node_t* head = list->head;
    node_t* current = head;

    while (current->next->next != NULL) { 
        current = current->next;
    }

    free(current->next);
    current->next = NULL;

    list->length--;
}

/* indexed by 0 */
void removeByIndex(List_t* list, int index) {
    if (list->length <= index) {
        return;
    }

    node_t** head = &(list->head);
    node_t* current = *head;
    node_t* prev = NULL;
    int i = 0;

    if (index == 0) {
		node_t* temp = *head;
        *head = current->next;
        free(temp);
        
		list->length--;
		return;
    }

    while (i++ != index) {
        prev = current;
        current = current->next;
    }

    prev->next = current->next;
    free(current);

    list->length--;
}

void printList(List_t *list) {
    printf("list size: %d\n", list->length);
    node_t *c = list->head;
    for (int i = 0; i < list->length; ++i) {
        printf("%d | username: %s | fd: %d\n", i, c->username, c->user_fd);
        c = c->next;
    }
    printf("fini!!!\n");
}

void deleteList(List_t* list) {
    if (list->length == 0)
        return;
    while (list->head != NULL){
        removeFront(list);
    }
    list->length = 0;
}

int getIndexByFD(List_t* list, int fd)
{
    node_t *c = list->head;
    for (int i = 0; i < list->length; ++i) {
        if (c->user_fd == fd)
            return i;
        else
            c = c->next;
    }

    return -1;
}

node_t* getNode(List_t* list, int index)
{
    if (index >= list->length)
        return NULL;
    
    node_t *c = list->head;
    int i = 0;
    while (i++ != index) {
        c = c->next;
    }

    return c;
}