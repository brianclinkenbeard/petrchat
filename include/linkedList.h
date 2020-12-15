#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#define INT_MODE 0
#define STR_MODE 1
#define STR_MAX 256

/*
 * Structre for each node of the linkedList
 *
 * value - a pointer to the data of the node. 
 * next - a pointer to the next node in the list. 
 */
typedef struct user_node {
    char username[STR_MAX];
    int user_fd;
    struct user_node* next;
} user_t;

/*
 * Structure for the base linkedList
 * 
 * head - a pointer to the first node in the list. NULL if length is 0.
 * length - the current length of the linkedList. Must be initialized to 0.
 * comparator - function pointer to linkedList comparator. Must be initialized!
 */
typedef struct userlist {
    user_t* head;
    int length;
} userlist_t;

/* 
 * Each of these functions inserts the reference to the data (valref)
 * into the linkedList list at the specified position
 *
 * @param list pointer to the linkedList struct
 * @param valref pointer to the data to insert into the linkedList
 */
void insertFront(userlist_t* list, char* un, int fd);
void addUser(userlist_t* list, char* un, int fd);

/*
 * Each of these functions removes a single linkedList node from
 * the LinkedList at the specfied function position.
 * @param list pointer to the linkedList struct
 */ 
void removeFront(userlist_t* list);
void removeRear(userlist_t* list);
void removeByIndex(userlist_t* list, int n);

/* 
 * Free all nodes from the linkedList
 *
 * @param list pointer to the linkedList struct
 */
void deleteUserList(userlist_t* list);

/*
 * Traverse the list printing each node in the current order.
 * @param list pointer to the linkedList strut
 * @param mode STR_MODE to print node.value as a string,
 * INT_MODE to print node.value as an int
 */
void printList(userlist_t* list);

int getIndexByFD(userlist_t* list, int fd);
user_t* getUser(userlist_t* list, int index);
user_t* getUserByName(userlist_t* list, char* name);
int nameExists(userlist_t* list, char* name);

typedef struct room_node {
    char roomname[STR_MAX];
    char owner[STR_MAX];
    userlist_t* userlist;
    struct room_node* next;
} room_t;

typedef struct room_list {
    room_t *head;
    int length;
} roomlist_t;

void addRoomFront(roomlist_t*, char*, user_t);
void addRoom(roomlist_t*, char*, user_t);
void addUserToRoom(room_t*, user_t);
room_t* getRoom(roomlist_t*, char*);
int removeRoom(roomlist_t*, char*);
int removeUserFromRoom(roomlist_t*, room_t*, user_t);
void deleteRoomList(roomlist_t*);

#endif