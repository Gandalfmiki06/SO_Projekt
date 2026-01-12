#include "hala.h"
#include <stdlib.h>
#include <unistd.h>

void* kasa(void* arg) {
    int id = *(int*)arg;

    while (1) {
        pthread_mutex_lock(&mutex_kolejka);

        while (q_size == 0 && sprzedane < K) {
            pthread_cond_wait(&cond_kolejka, &mutex_kolejka);
        }

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

        if (sprzedane >= K) {
            pthread_mutex_unlock(&mutex_bilety);
            break;
        }

        int sektor = rand() % SEKTORY;
        int ile = (miejsca[sektor] >= 2) ? 2 : miejsca[sektor];

        if (ile > 0) {
            miejsca[sektor] -= ile;
            sprzedane += ile;

            k->sektor = sektor;
            k->bilety = ile;

            loguj("Kasa: sprzedano bilety");
        }

        pthread_mutex_unlock(&mutex_bilety);
        sleep(1);
    }

    loguj("Kasa: zamknięta");
    return NULL;
}
