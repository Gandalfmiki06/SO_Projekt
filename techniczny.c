#include "hala.h"
#include <unistd.h>
#include <stdio.h>

void* techniczny(void* arg)
{
    (void)arg;
    char buf[256];

    while(!ewakuacja && !przerwanie){
        sleep(1);
    }

    int puste;
    do{
        puste = 1;
        pthread_mutex_lock(&mutex_wejsc);
        for(int i=0;i<SEKTORY;i++){
            if(osoby_w_sektorze[i] > 0) { puste = 0; break; }
        }
        if(osoby_w_sektorze_vip > 0) puste = 0;
        pthread_mutex_unlock(&mutex_wejsc);
        sleep(1);
    } while(!puste && !przerwanie);

    snprintf(buf, sizeof(buf), "Techniczny: wszystkie sektory (w tym VIP) puste");
    loguj(buf);
    loguj("Techniczny -> Kierownik: sektory oproznione");

    return NULL;
}
