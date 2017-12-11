#include "svr_queue.h"

void
queue_init(queue_t *queue)
{
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
}

void
enqueue(queue_t *queue, void *data)
{
    elem_t *elem = (elem_t *) malloc(sizeof(elem_t));
    elem->data = data;
    elem->next = NULL;

    if (queue->count == 0) {
        queue->head = elem;
        queue->tail = elem;
    } else {
        queue->tail->next = elem;
        queue->tail = elem;
    }

    queue->count++;
}

void*
dequeue(queue_t *queue)
{
    elem_t *elem;

    if (queue->count == 0) {
        return NULL;
    }

    elem = queue->head;
    queue->head = queue->head->next;
    elem->next = NULL;
    queue->count--;

    if (queue->count == 0) {
        queue->head = NULL;
        queue->tail = NULL;
    }

    return elem->data;
}
