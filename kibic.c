#include "hala.h"
#include <unistd.h>
#include <stdlib.h>

void* kibic(void* arg) {
    Kibic* k = (Kibic*)arg;

    pthread_mutex_lock(&mutex_kolejka);

    if (k->vip) {
        q_start = (q_start - 1 + MAX_KOLEJKA) % MAX_KOLEJKA;
        kolejka[q_start] = k;
        loguj("VIP wszedł na początek kolejki");
    } else {
        kolejka[q_end] = k;
        q_end = (q_end + 1) % MAX_KOLEJKA;
    }

    q_size++;
    kibice_w_kolejce++;

    pthread_cond_signal(&cond_kolejka);
    pthread_mutex_unlock(&mutex_kolejka);

    /* czeka na zakup */
    while (k->bilety == 0 && sprzedane < K) {
        sleep(1);
    }

    return NULL;
}
