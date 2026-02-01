#ifndef QUEUE_H
#define QUEUE_H

#include "hala.h"
#include <pthread.h>

typedef struct {
    Kibic* buf[MAX_KOLEJKA];
    int head;
    int tail;
    int size;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} Queue;

void queue_init(Queue* q);
int queue_push(Queue* q, Kibic* k);
Kibic* queue_pop(Queue* q);
int queue_empty(Queue* q);

#endif
