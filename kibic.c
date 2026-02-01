#include "hala.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

void* kibic(void* arg)
{
    Kibic* k=(Kibic*)arg;
    char buf[128];

    usleep((rand()%3000)*1000);

    if(przerwanie){ free(k); return NULL; }

    if(k->vip) stat_vip++; else stat_normalni++;
    if(k->wiek<15) stat_dzieci++;

    pthread_mutex_lock(&mutex_kolejka);
    if(k->vip){
        kolejka_vip[qv_end]=k;
        qv_end=(qv_end+1)%MAX_KOLEJKA;
        qv_size++;
        sprintf(buf,"VIP %d stanal w kolejce VIP",k->id);
    } else {
        kolejka[q_end]=k;
        q_end=(q_end+1)%MAX_KOLEJKA;
        q_size++; kibice_w_kolejce++;
        sprintf(buf,"Kibic %d stanal w kolejce (dlugosc=%d)",k->id,kibice_w_kolejce);
    }
    loguj(buf);
    pthread_cond_signal(&cond_kolejka);
    pthread_mutex_unlock(&mutex_kolejka);

    pthread_mutex_lock(&mutex_wejsc);
    while(stop_sektor[k->sektor] && !przerwanie)
        pthread_cond_wait(&cond_wejsc,&mutex_wejsc);
    pthread_mutex_unlock(&mutex_wejsc);

    if(!k->vip && !przerwanie){
        int s;
        while(!przerwanie){
            for(s=0;s<2;s++){
                if(sem_trywait(&kontrola[k->sektor][s])==0){
                    pthread_mutex_lock(&mutex_wejsc);
                    if(stanowisko_druzyna[k->sektor][s]==-1 ||
                       stanowisko_druzyna[k->sektor][s]==k->druzyna){
                        stanowisko_druzyna[k->sektor][s]=k->druzyna;
                        pthread_mutex_unlock(&mutex_wejsc);
                        usleep(500000);
                        pthread_mutex_lock(&mutex_wejsc);
                        stanowisko_druzyna[k->sektor][s]=-1;
                        pthread_mutex_unlock(&mutex_wejsc);
                        sem_post(&kontrola[k->sektor][s]);
                        goto po_kontroli;
                    }
                    pthread_mutex_unlock(&mutex_wejsc);
                    sem_post(&kontrola[k->sektor][s]);
                }
            }
            usleep(100000);
        }
    }
po_kontroli:

    pthread_mutex_lock(&mutex_wejsc);
    osoby_w_sektorze[k->sektor]+=k->bilety;
    pthread_mutex_unlock(&mutex_wejsc);

    loguj("Kibic wszedl do sektora");
    stat_wejsc++;

    while(!ewakuacja && !przerwanie) sleep(1);

    pthread_mutex_lock(&mutex_wejsc);
    osoby_w_sektorze[k->sektor]-=k->bilety;
    pthread_mutex_unlock(&mutex_wejsc);

    loguj("Kibic opuszcza hale");
    free(k);
    return NULL;
}
