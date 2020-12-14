#ifndef SBUF_H
#define SBUF_H

#include <semaphore.h>
#include <stdlib.h>
#include "protocol.h"
#include "server.h"
#include "linkedList.h"

// P -> semwait
// V -> sem_post

// job message
typedef struct {
    user_t user;
    petr_header header;
    char msg[BUFFER_SIZE];
} j_msg;

typedef struct {
    j_msg *buf;
    int n;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
} sbuf_t;

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, j_msg item);
j_msg sbuf_remove(sbuf_t *sp);

#endif