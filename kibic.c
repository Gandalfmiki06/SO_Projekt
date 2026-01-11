#include "hala.h"
#include <unistd.h>

void* kibic(void* arg) {
    int sektor = *(int*)arg;

    pthread_mutex_lock(&mutex_wejsc);
    while (stop_wejsc) {
        pthread_cond_wait(&cond_wejsc, &mutex_wejsc);
    }
    pthread_mutex_unlock(&mutex_wejsc);

    sem_wait(&kontrola[sektor][0]);
    sem_wait(&kontrola[sektor][1]);

    loguj("Kibic: przeszed³ kontrolê");

    sem_post(&kontrola[sektor][0]);
    sem_post(&kontrola[sektor][1]);

    while (!ewakuacja) {
        sleep(1);
    }

    loguj("Kibic: opuœci³ sektor");
    return NULL;
}
