#ifndef QUEUE_H_
#define QUEUE_H_

#include <stdlib.h>

typedef struct node
{
    void *data;
    struct node *prev;
    struct node *next;
}node_t;

typedef struct queue
{
    int size;
    node_t *head;
    node_t *tail;
}queue_t;

queue_t *newqueue(void);

void enqueue(queue_t *queue, void *data);

void *dequeue(queue_t *queue);

void freequeue(queue_t *queue);

#endif
