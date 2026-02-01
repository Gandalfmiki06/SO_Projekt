#include "hala.h"
#include <unistd.h>

void* techniczny(void* arg)
{
    while(!ewakuacja && !przerwanie) sleep(1);

    int puste;
    do{
        puste=1;
        pthread_mutex_lock(&mutex_wejsc);
        for(int i=0;i<SEKTORY;i++)
            if(osoby_w_sektorze[i]>0) puste=0;
        pthread_mutex_unlock(&mutex_wejsc);
        sleep(1);
    }while(!puste);

    loguj("Techniczny: wszystkie sektory puste");
    return NULL;
}
