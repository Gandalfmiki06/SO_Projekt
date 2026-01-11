#include "hala.h"
#include <stdlib.h>
#include <unistd.h>

void* kasa(void* arg) {
    while (1) {
        pthread_mutex_lock(&mutex_bilety);

        if (sprzedane >= K) {
            pthread_mutex_unlock(&mutex_bilety);
            break;
        }

        int sektor = rand() % SEKTORY;

        if (miejsca[sektor] >= 1) {
            miejsca[sektor]--;
            sprzedane++;
            loguj("Kasa: sprzedano bilet");
        }

        pthread_mutex_unlock(&mutex_bilety);
        sleep(1);
    }
    return NULL;
}
