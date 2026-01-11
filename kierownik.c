#include "hala.h"
#include <unistd.h>

void* kierownik(void* arg) {
    sleep(5);
    pthread_mutex_lock(&mutex_wejsc);
    stop_wejsc = 1;
    loguj("Kierownik: wstrzymanie wejœæ");
    pthread_mutex_unlock(&mutex_wejsc);

    sleep(5);
    pthread_mutex_lock(&mutex_wejsc);
    stop_wejsc = 0;
    pthread_cond_broadcast(&cond_wejsc);
    loguj("Kierownik: wznowienie wejœæ");
    pthread_mutex_unlock(&mutex_wejsc);

    sleep(10);
    ewakuacja = 1;
    loguj("Kierownik: ewakuacja");
    return NULL;
}
