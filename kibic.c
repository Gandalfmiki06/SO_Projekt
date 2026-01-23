#include "hala.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

void* kibic(void* arg)
{
    Kibic* k = (Kibic*)arg;

    pthread_mutex_lock(&mutex_kolejka);
    kolejka[q_end] = k;
    q_end = (q_end + 1) % MAX_KOLEJKA;
    q_size++;
    kibice_w_kolejce++;
    pthread_cond_signal(&cond_kolejka);
    pthread_mutex_unlock(&mutex_kolejka);

    char buf[100];
    snprintf(buf, sizeof(buf), "Kibic %d w kolejce", k->id);
    loguj(buf);

    /* ===== WEJŚCIE ===== */

    pthread_mutex_lock(&mutex_wejsc);
    while (stop_wejsc)
        pthread_cond_wait(&cond_wejsc, &mutex_wejsc);
    pthread_mutex_unlock(&mutex_wejsc);

    if (!k->vip) {
        sem_wait(&kontrola[k->sektor][0]);
        sem_post(&kontrola[k->sektor][0]);
    }

    loguj("Kibic wszedl do sektora");
    return NULL;
}
