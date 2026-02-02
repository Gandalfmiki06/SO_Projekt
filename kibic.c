/* kibic.c - kontrola stanowisk z regu³¹ dru¿ynow¹, oczekiwanie na potwierdzenie technicznego,
   cofanie rezerwacji VIP przy timeoutach, pass_count nie inkrementowany tutaj */

#include "hala.h"
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#ifndef MAX_WAIT_SEC
#define MAX_WAIT_SEC 12
#endif

void* kibic(void* arg)
{
    Kibic* k = (Kibic*)arg;
    char buf[256];

    while(!przerwanie){
        /* dodajemy do odpowiedniej kolejki */
        pthread_mutex_lock(&mutex_kolejka);
        if(k->vip){
            if(qv_size < MAX_KOLEJKA){
                kolejka_vip[qv_end] = k;
                qv_end = (qv_end + 1) % MAX_KOLEJKA;
                qv_size++;
                k->in_queue = 1;
                sem_post(&items_sem);
            } else {
                k->sektor = -2;
                pthread_mutex_unlock(&mutex_kolejka);
                break;
            }
        } else {
            if(q_size < MAX_KOLEJKA){
                kolejka[q_end] = k;
                q_end = (q_end + 1) % MAX_KOLEJKA;
                q_size++;
                k->in_queue = 1;
                kibice_w_kolejce++;
                sem_post(&items_sem);
            } else {
                k->sektor = -2;
                pthread_mutex_unlock(&mutex_kolejka);
                break;
            }
        }
        pthread_mutex_unlock(&mutex_kolejka);

        /* czekamy na przydzial (k->to_enter ustawione przez kasê) */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += MAX_WAIT_SEC;

        pthread_mutex_lock(&mutex_wejsc);
        int rc = 0;
        while(!k->to_enter && !przerwanie){
            rc = pthread_cond_timedwait(&cond_wejsc, &mutex_wejsc, &ts);
            if(rc == ETIMEDOUT) break;
        }
        pthread_mutex_unlock(&mutex_wejsc);

        if(przerwanie) break;

        if(!k->to_enter){
            /* timeout: przed pobraniem lub po (sprawdŸ in_queue i sektor) */
            if(k->in_queue){
                __sync_fetch_and_add(&timedout_before_dequeue, 1);
            } else {
                if (k->sektor >= 0) __sync_fetch_and_add(&timedout_after_dequeue, 1);
            }
            __sync_fetch_and_add(&timeout_count, 1);

            /* jeœli VIP mia³ rezerwacjê, cofnij j¹ */
            if (k->vip) {
                pthread_mutex_lock(&mutex_vip);
                if (vip_reserved >= k->bilety) vip_reserved -= k->bilety;
                pthread_mutex_unlock(&mutex_vip);
            }

            k->sektor = -2;
            snprintf(buf, sizeof(buf), "Kibic %d: timeout, opuszcza kolejke", k->id);
            loguj(buf);
            break;
        }

        /* Mamy przydzia³ (k->to_enter > 0). Teraz przechodzimy przez kontrolê.
           Kontrola: 2 stanowiska na sektor, max 3 osoby na stanowisko, jeœli wiêcej ni¿ 1 osoba
           na stanowisku - musz¹ byæ tej samej dru¿yny. */

        int remaining = k->to_enter;
        int entered_here = 0;

        while(remaining > 0 && !przerwanie){
            int assigned = 0;
            pthread_mutex_lock(&mutex_wejsc);
            for(int st = 0; st < 2; ++st){
                int cnt = stanowisko_count[k->sektor][st];
                int team = stanowisko_druzyna[k->sektor][st];
                if(cnt < 3 && (team == -1 || team == k->druzyna)){
                    stanowisko_count[k->sektor][st] = cnt + 1;
                    stanowisko_druzyna[k->sektor][st] = k->druzyna;
                    assigned = 1;
                    break;
                }
            }
            if(!assigned){
                /* poczekaj na powiadomienie technicznego/zwolnienie stanowiska */
                pthread_cond_wait(&cond_wejsc, &mutex_wejsc);
                pthread_mutex_unlock(&mutex_wejsc);
                /* zwiêkszamy pass_count tylko jeœli ktoœ inny zosta³ potwierdzony przed nim
                   (techniczny bêdzie inkrementowa³ pass_count dla pominiêtych) */
                continue;
            } else {
                pthread_mutex_unlock(&mutex_wejsc);
            }

            /* symulacja kontroli: krótki czas na stanowisku */
            usleep(1000 * (50 + (rand()%150))); /* 50-200ms */

            /* po kontroli: opuszczamy stanowisko */
            pthread_mutex_lock(&mutex_wejsc);
            for(int st = 0; st < 2; ++st){
                if(stanowisko_druzyna[k->sektor][st] == k->druzyna && stanowisko_count[k->sektor][st] > 0){
                    stanowisko_count[k->sektor][st] -= 1;
                    if(stanowisko_count[k->sektor][st] == 0){
                        stanowisko_druzyna[k->sektor][st] = -1;
                    }
                    break;
                }
            }
            pthread_mutex_unlock(&mutex_wejsc);

            /* zaliczamy lokalnie, ale globalne liczniki zrobi techniczny */
            remaining -= 1;
            entered_here += 1;
        }

        if(entered_here > 0){
            snprintf(buf, sizeof(buf), "Kibic %d: przeszedl kontrolê i jest gotowy do wejœcia (%d)", k->id, entered_here);
            loguj(buf);
        }

        /* czekaj na potwierdzenie technicznego (entered_flag) */
        while(!entered_flag[k->id] && !przerwanie){
            pthread_mutex_lock(&mutex_wejsc);
            pthread_cond_wait(&cond_wejsc, &mutex_wejsc);
            pthread_mutex_unlock(&mutex_wejsc);
        }

        /* po potwierdzeniu technicznego w¹tek kibica koñczy siê */
        break;
    }

    /* sprz¹tanie: jeœli wci¹¿ w kolejce, usuñ */
    pthread_mutex_lock(&mutex_kolejka);
    if(k->in_queue){
        for(int i=0;i<MAX_KOLEJKA;i++){
            if(kolejka[i] == k){
                kolejka[i] = NULL;
                if(q_size > 0) q_size--;
                k->in_queue = 0;
                break;
            }
            if(kolejka_vip[i] == k){
                kolejka_vip[i] = NULL;
                if(qv_size > 0) qv_size--;
                k->in_queue = 0;
                break;
            }
        }
    }
    pthread_mutex_unlock(&mutex_kolejka);

    return NULL;
}
