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

void insertFront(userlist_t* list, char* un, int fd) {
    if (list->length == 0)
        list->head = NULL;

    user_t** head = &(list->head);
    user_t* new_node;
    new_node = malloc(sizeof(user_t));

    strcpy(new_node->username, un);
    new_node->user_fd = fd;

    new_node->next = *head;
    *head = new_node;
    list->length++; 
}

void addUser(userlist_t* list, char* un, int fd) {
    if (list->length == 0) {
        insertFront(list, un, fd);
        return;
    }

    user_t* head = list->head;
    user_t* current = head;
    while (current->next != NULL) {
        current = current->next;
    }

    current->next = malloc(sizeof(user_t));
    strcpy(current->next->username, un);
    current->next->user_fd = fd;
    current->next->next = NULL;
    list->length++;
}

void removeFront(userlist_t* list) {
    user_t** head = &(list->head);
    user_t* next_node = NULL;

    if (list->length == 0) {
        return;
    }

    next_node = (*head)->next;
    list->length--;

    user_t* temp = *head;
    *head = next_node;
    free(temp);
}

void removeRear(userlist_t* list) {
    if (list->length == 0) {
        return;
    } else if (list->length == 1) {
        removeFront(list);
		return;
    }

    user_t* head = list->head;
    user_t* current = head;

    while (current->next->next != NULL) { 
        current = current->next;
    }

    free(current->next);
    current->next = NULL;

    list->length--;
}

/* indexed by 0 */
void removeByIndex(userlist_t* list, int index) {
    if (list->length <= index) {
        return;
    }

    user_t** head = &(list->head);
    user_t* current = *head;
    user_t* prev = NULL;
    int i = 0;

    if (index == 0) {
		user_t* temp = *head;
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

void printList(userlist_t *list) {
    user_t *c = list->head;
    for (int i = 0; i < list->length; ++i) {
        printf("%s\n", c->username);
        c = c->next;
    }
}

void deleteUserList(userlist_t* list) {
    if (list->length == 0)
        return;
    while (list->head != NULL){
        removeFront(list);
    }
    list->length = 0;
}

int getIndexByFD(userlist_t* list, int fd)
{
    user_t *c = list->head;
    for (int i = 0; i < list->length; ++i) {
        if (c->user_fd == fd)
            return i;
        else
            c = c->next;
    }

    return -1;
}

user_t* getUser(userlist_t* list, int index)
{
    if (index >= list->length)
        return NULL;
    
    user_t *c = list->head;
    int i = 0;
    while (i++ != index) {
        c = c->next;
    }

    return c;
}

user_t* getUserByName(userlist_t* list, char* name)
{
    for (user_t *u = list->head; u != NULL; u = u->next) {
        if (strcmp(u->username, name) == 0) {
            return u;
        }
    }
    return NULL;
}

int nameExists(userlist_t *list, char *name)
{
    int exists = 0;
    for (user_t *c = list->head; c != NULL; c = c->next) {
        if (strcmp(c->username, name) == 0) {
            exists = 1;
            break;
        }
    }
    return exists;
}

void addUserToRoom(room_t* room, user_t user) {
    if (room->userlist == NULL)
        room->userlist = malloc(sizeof(userlist_t));

    addUser(room->userlist, user.username, user.user_fd);
}

// rooms
void addRoomFront(roomlist_t* list, char* name, user_t owner) {
    if (list->length == 0)
        list->head = NULL;

    room_t** head = &(list->head);
    room_t* new_node;
    new_node = malloc(sizeof(room_t));

    strcpy(new_node->roomname, name);
    strcpy(new_node->owner, owner.username);
    new_node->userlist = NULL;
    addUserToRoom(new_node, owner); // add owner to room

    new_node->next = *head;
    *head = new_node;
    list->length++; 
}

void addRoom(roomlist_t* list, char* name, user_t owner) {
    if (list->length == 0) {
        addRoomFront(list, name, owner);
        return;
    }

    room_t* head = list->head;
    room_t* current = head;
    while (current->next != NULL) {
        current = current->next;
    }

    current->next = malloc(sizeof(room_t));
    strcpy(current->next->roomname, name);
    strcpy(current->next->owner, owner.username);
    current->next->userlist = NULL;
    addUserToRoom(current->next, owner); // add owner to room
    current->next->next = NULL;
    list->length++;
}

room_t* getRoom(roomlist_t* list, char *name) {
    for (room_t *c = list->head; c != NULL; c = c->next) {
        if (strcmp(c->roomname, name) == 0) {
            return c;
        }
    }
    return NULL;
}

int removeRoom(roomlist_t* list, char* name) {
    room_t* prev = NULL;
    
    for (room_t* c = list->head; c != NULL; c = c->next) {
        if (c->roomname == name) {
            if (c == list->head) {
                list->head = c->next;
                deleteUserList(c->userlist);
                free(c);

                list->length--;
                return 0;
            } else {
                prev->next = c->next;
                deleteUserList(c->userlist);
                free(c);

                list->length--;
                return 0;
            }
        }
        prev = c; // before moving c to next
    }

    return -1;
}

int removeUserFromRoom(roomlist_t* list, room_t* room, user_t u) {
    int index = getIndexByFD(room->userlist, u.user_fd);
    
    if (index < 0) {
        return -1;
    } else {
        removeByIndex(room->userlist, index);
        return 0;
    }
}

void removeRoomListFront(roomlist_t* list) {
    room_t** head = &(list->head);
    room_t* next_node = NULL;

    if (list->length == 0) {
        return;
    }

    next_node = (*head)->next;
    list->length--;

    room_t* temp = *head;
    *head = next_node;
    deleteUserList(temp->userlist);
    free(temp);
}


void deleteRoomList(roomlist_t* list) {
    if (list->length == 0)
        return;
    while (list->head != NULL){
        removeRoomListFront(list);
    }
    list->length = 0;
}