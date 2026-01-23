#include "hala.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

void* kasa(void* arg)
{
    while (1) {
        pthread_mutex_lock(&mutex_kolejka);

        while (q_size == 0 && sprzedane < K)
            pthread_cond_wait(&cond_kolejka, &mutex_kolejka);

        if (sprzedane >= K) {
            pthread_mutex_unlock(&mutex_kolejka);
            break;
        }

        Kibic* k = kolejka[q_start];
        q_start = (q_start + 1) % MAX_KOLEJKA;
        q_size--;
        kibice_w_kolejce--;

        pthread_mutex_unlock(&mutex_kolejka);

        pthread_mutex_lock(&mutex_bilety);
        if (sprzedane + k->bilety <= K && miejsca[k->sektor] >= k->bilety) {
            miejsca[k->sektor] -= k->bilety;
            sprzedane += k->bilety;
            loguj("Kasa: sprzedano bilety");
        }
        pthread_mutex_unlock(&mutex_bilety);

        usleep(100000);
    }

    loguj("Kasa: zamknieta");
    return NULL;
}
