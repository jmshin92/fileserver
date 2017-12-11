#ifndef _SVR_QUEUE_H
#define _SVR_QUEUE_H
#include "svr_internal.h"

typedef struct elem_s elem_t;
struct elem_s {
    void           *data;
    struct elem_s  *next;
};

typedef struct queue_s queue_t;
struct queue_s {
    elem_t *head;
    elem_t *tail;
    int     count;
};

void queue_init(queue_t *queue);
void enqueue(queue_t *queue, void *data);
void* dequeue(queue_t *dequeue);

#endif
