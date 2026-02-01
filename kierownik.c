#include "hala.h"
#include <unistd.h>
#include <stdio.h>

void* kierownik(void* arg)
{
    while(sprzedane<K && !przerwanie){
        pthread_mutex_lock(&mutex_kolejka);
        int q=kibice_w_kolejce;
        pthread_mutex_unlock(&mutex_kolejka);

        int potrzebne=q/(K/10)+1;
        if(potrzebne<2) potrzebne=2;
        if(potrzebne>MAX_KASY) potrzebne=MAX_KASY;

        pthread_mutex_lock(&mutex_kasy);
        czynne_kasy=potrzebne;
        pthread_mutex_unlock(&mutex_kasy);

        sleep(1);
    }

    loguj("Kierownik: ewakuacja");
    ewakuacja=1;
    pthread_cond_broadcast(&cond_wejsc);
    return NULL;
}
