#include "hala.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

void* kasa(void* arg)
{
    int id=(int)(size_t)arg;
    char buf[128];

    while(!przerwanie){
        pthread_mutex_lock(&mutex_kasy);
        if(id>=czynne_kasy){
            pthread_mutex_unlock(&mutex_kasy);
            sleep(1);
            continue;
        }
        pthread_mutex_unlock(&mutex_kasy);

        pthread_mutex_lock(&mutex_kolejka);
        while(q_size==0 && qv_size==0 && sprzedane<K && !przerwanie)
            pthread_cond_wait(&cond_kolejka,&mutex_kolejka);

        if(sprzedane>=K || przerwanie){
            pthread_mutex_unlock(&mutex_kolejka);
            break;
        }

        Kibic* k;
        if(qv_size>0){
            k=kolejka_vip[qv_start];
            qv_start=(qv_start+1)%MAX_KOLEJKA;
            qv_size--;
        } else {
            k=kolejka[q_start];
            q_start=(q_start+1)%MAX_KOLEJKA;
            q_size--; kibice_w_kolejce--;
        }
        pthread_mutex_unlock(&mutex_kolejka);

        pthread_mutex_lock(&mutex_bilety);
        if(miejsca[k->sektor]>=k->bilety){
            miejsca[k->sektor]-=k->bilety;
            sprzedane+=k->bilety;
            sprintf(buf,"Kasa %d sprzedala %d bilety sektor %d",id,k->bilety,k->sektor);
            loguj(buf);
        }
        pthread_mutex_unlock(&mutex_bilety);
    }

    loguj("Kasa zamknieta");
    return NULL;
}
