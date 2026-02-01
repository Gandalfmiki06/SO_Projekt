#include "hala.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>

/*
    WERSJA BEZ KORUPCJI PAMIĘCI:
    - kibic NIGDY nie usuwa się z kolejki
    - kibic NIGDY nie robi free(k)
    - kibic przy timeout ustawia sektor = -2 i kończy wątek
    - kasa widząc sektor = -2 ignoruje kibica
*/

static void enqueue_kibic_local(Kibic* k)
{
    pthread_mutex_lock(&mutex_kolejka);

    if(k->vip){
        if(qv_size < MAX_KOLEJKA){
            kolejka_vip[qv_end] = k;
            qv_end = (qv_end + 1) % MAX_KOLEJKA;
            qv_size++;
        }
    } else {
        if(q_size < MAX_KOLEJKA){
            kolejka[q_end] = k;
            q_end = (q_end + 1) % MAX_KOLEJKA;
            q_size++;
            kibice_w_kolejce++;
        }
    }

    k->in_queue = 1;
    pthread_cond_broadcast(&cond_kolejka);
    pthread_mutex_unlock(&mutex_kolejka);
}

void* kibic(void* arg)
{
    Kibic* k = (Kibic*)arg;
    char buf[256];

    /* losowe opóźnienie przyjścia */
    usleep((rand()%3000) * 1000);

    /* log przyjścia */
    time_t now = time(NULL);
    if(Tp != 0){
        snprintf(buf, sizeof(buf),
                 "Kibic %d przyszedl %s rozpoczeciem meczu (Tp)",
                 k->id, (now < Tp ? "PRZED" : "PO"));
        loguj(buf);
    }

    if(przerwanie){
        __sync_fetch_and_sub(&remaining_arrivals, 1);
        return NULL;
    }

    if(!k->vip) __sync_fetch_and_add(&stat_normalni, 1);
    if(k->wiek < 15) __sync_fetch_and_add(&stat_dzieci, 1);

    k->sektor = -1;

    snprintf(buf, sizeof(buf),
             "Kibic %d stanal w kolejce (bilety=%d vip=%d)",
             k->id, k->bilety, k->vip);
    loguj(buf);

    enqueue_kibic_local(k);

    /* timeout oczekiwania na sektor */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 120;

    pthread_mutex_lock(&mutex_wejsc);

    while(!przerwanie){

        /* anulowany przez kasę */
        if(k->sektor == -2){
            snprintf(buf, sizeof(buf),
                     "Kibic %d: sprzedaz anulowana — opuszcza", k->id);
            loguj(buf);
            pthread_mutex_unlock(&mutex_wejsc);
            __sync_fetch_and_sub(&remaining_arrivals, 1);
            return NULL;
        }

        /* VIP czeka tylko na to_enter */
        if(k->vip){
            if(k->to_enter > 0) break;

            int rc = pthread_cond_timedwait(&cond_wejsc, &mutex_wejsc, &ts);
            if(rc == ETIMEDOUT){
                snprintf(buf, sizeof(buf),
                         "Kibic %d (VIP): timeout oczekiwania — opuszcza", k->id);
                loguj(buf);

                k->sektor = -2;   // oznacz jako anulowany
                pthread_mutex_unlock(&mutex_wejsc);
                __sync_fetch_and_sub(&remaining_arrivals, 1);
                return NULL;
            }
            continue;
        }

        /* zwykły kibic */
        if(k->sektor >= 0){
            if(!stop_sektor[k->sektor]) break;

            snprintf(buf, sizeof(buf),
                     "Kibic %d czeka — sektor %d wstrzymany",
                     k->id, k->sektor);
            loguj(buf);

            pthread_cond_timedwait(&cond_wejsc, &mutex_wejsc, &ts);
        } else {
            snprintf(buf, sizeof(buf),
                     "Kibic %d czeka na przydzial sektora", k->id);
            loguj(buf);

            int rc = pthread_cond_timedwait(&cond_wejsc, &mutex_wejsc, &ts);
            if(rc == ETIMEDOUT){
                snprintf(buf, sizeof(buf),
                         "Kibic %d: timeout oczekiwania — opuszcza", k->id);
                loguj(buf);

                k->sektor = -2;   // oznacz jako anulowany
                pthread_mutex_unlock(&mutex_wejsc);
                __sync_fetch_and_sub(&remaining_arrivals, 1);
                return NULL;
            }
        }
    }

    pthread_mutex_unlock(&mutex_wejsc);

    if(przerwanie){
        __sync_fetch_and_sub(&remaining_arrivals, 1);
        return NULL;
    }

    /* KONTROLA WEJŚCIA — tylko nie-VIP */
    if(!k->vip){
        int entered = 0;

        if(k->sektor >= 0){
            pthread_mutex_lock(&mutex_wejsc);
            waiting_count[k->sektor]++;
            k->waiting_for_control = 1;
            pthread_mutex_unlock(&mutex_wejsc);
        }

        /* dzieci czekają na opiekuna */
        if(k->wiek < 15){
            while(!przerwanie){
                pthread_mutex_lock(&mutex_wejsc);
                int ok = 0;

                if(k->guardian_id >= 0 &&
                   k->guardian_id < MAX_KOLEJKA &&
                   entered_flag[k->guardian_id])
                    ok = 1;

                if(adults_entered_in_sector[k->sektor] > 0)
                    ok = 1;

                pthread_mutex_unlock(&mutex_wejsc);

                if(ok) break;
                usleep(100000);
            }
        }

        /* próba wejścia */
        while(!entered && !przerwanie){
            int sec = k->sektor;
            if(sec < 0 || sec >= SEKTORY){
                usleep(100000);
                continue;
            }

            for(int s=0;s<2 && !entered;s++){
                if(sem_trywait(&kontrola[sec][s]) == 0){

                    pthread_mutex_lock(&mutex_wejsc);
                    if(stanowisko_druzyna[sec][s] == -1 ||
                       stanowisko_druzyna[sec][s] == k->druzyna){

                        stanowisko_druzyna[sec][s] = k->druzyna;
                        stanowisko_count[sec][s]++;
                        pthread_mutex_unlock(&mutex_wejsc);

                        usleep(200000);

                        pthread_mutex_lock(&mutex_wejsc);
                        stanowisko_count[sec][s]--;
                        if(stanowisko_count[sec][s] == 0)
                            stanowisko_druzyna[sec][s] = -1;
                        pthread_mutex_unlock(&mutex_wejsc);

                        sem_post(&kontrola[sec][s]);
                        entered = 1;
                        break;
                    }
                    pthread_mutex_unlock(&mutex_wejsc);
                    sem_post(&kontrola[sec][s]);
                }
            }

            if(!entered) usleep(100000);
        }

        if(k->sektor >= 0){
            pthread_mutex_lock(&mutex_wejsc);
            if(waiting_count[k->sektor] > 0)
                waiting_count[k->sektor]--;
            k->waiting_for_control = 0;
            pthread_mutex_unlock(&mutex_wejsc);
        }
    }

    /* WEJŚCIE DO SEKTORA */
    if(k->vip){
        pthread_mutex_lock(&mutex_wejsc);
        osoby_w_sektorze_vip++;
        pthread_mutex_unlock(&mutex_wejsc);

        __sync_fetch_and_add(&stat_vip_wejsc, 1);
        __sync_fetch_and_add(&stat_wejsc, 1);

        snprintf(buf, sizeof(buf),
                 "Kibic %d (VIP) wszedl do sektora VIP", k->id);
        loguj(buf);
    }
    else if(k->sektor >= 0){
        pthread_mutex_lock(&mutex_wejsc);
        osoby_w_sektorze[k->sektor]++;
        entered_flag[k->id] = 1;
        if(k->wiek >= 15)
            adults_entered_in_sector[k->sektor]++;
        pthread_mutex_unlock(&mutex_wejsc);

        __sync_fetch_and_add(&stat_wejsc, 1);

        snprintf(buf, sizeof(buf),
                 "Kibic %d wszedl do sektora %d", k->id, k->sektor);
        loguj(buf);
    }
    else {
        snprintf(buf, sizeof(buf),
                 "Kibic %d: brak sektora — opuszcza", k->id);
        loguj(buf);
        __sync_fetch_and_sub(&remaining_arrivals, 1);
        return NULL;
    }

    /* CZEKANIE NA EWAKUACJĘ */
    while(!ewakuacja && !przerwanie){
        sleep(1);
    }

    /* WYJŚCIE */
    if(k->vip){
        pthread_mutex_lock(&mutex_wejsc);
        if(osoby_w_sektorze_vip > 0)
            osoby_w_sektorze_vip--;
        pthread_mutex_unlock(&mutex_wejsc);

        snprintf(buf, sizeof(buf),
                 "Kibic %d (VIP) opuszcza hale", k->id);
        loguj(buf);
    } else {
        pthread_mutex_lock(&mutex_wejsc);
        if(osoby_w_sektorze[k->sektor] > 0)
            osoby_w_sektorze[k->sektor]--;
        if(k->wiek >= 15 &&
           adults_entered_in_sector[k->sektor] > 0)
            adults_entered_in_sector[k->sektor]--;
        pthread_mutex_unlock(&mutex_wejsc);

        snprintf(buf, sizeof(buf),
                 "Kibic %d opuszcza hale (sektor %d)",
                 k->id, k->sektor);
        loguj(buf);
    }

    __sync_fetch_and_sub(&remaining_arrivals, 1);
    return NULL;
}
