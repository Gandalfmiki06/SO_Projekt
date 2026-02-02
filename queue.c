#include "queue.h"
#include <string.h>

void queue_init(Queue* q)
{
    memset(q->buf, 0, sizeof(q->buf));
    q->head = 0;
    q->tail = 0;
    q->size = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
}

int queue_push(Queue* q, Kibic* k)
{
    pthread_mutex_lock(&q->lock);

    if(q->size >= MAX_KOLEJKA){
        pthread_mutex_unlock(&q->lock);
        return 0; // pełna
    }

    if(q->buf[q->tail] != NULL){
        loguj("ERROR: queue_push nadpisalby niepuste miejsce!");
    }

    q->buf[q->tail] = k;
    q->tail = (q->tail + 1) % MAX_KOLEJKA;
    q->size++;

    pthread_cond_broadcast(&q->cond);
    pthread_mutex_unlock(&q->lock);
    return 1;
}

Kibic* queue_pop(Queue* q)
{
    pthread_mutex_lock(&q->lock);

    while(q->size == 0 && !przerwanie){
        pthread_cond_wait(&q->cond, &q->lock);
    }

    if(q->size == 0){
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }

    Kibic* k = q->buf[q->head];
    q->buf[q->head] = NULL;
    q->head = (q->head + 1) % MAX_KOLEJKA;
    q->size--;

    pthread_mutex_unlock(&q->lock);
    return k;
}

int queue_empty(Queue* q)
{
    pthread_mutex_lock(&q->lock);
    int e = (q->size == 0);
    pthread_mutex_unlock(&q->lock);
    return e;
}
