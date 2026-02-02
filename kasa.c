/* kasa.c - poprawiona obsługa VIP, losowy wybór sektora i cofanie rezerwacji */

#include "hala.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>

void* kasa(void* arg)
{
    int id = (int)(size_t)arg;
    char buf[256];

    const useconds_t service_usleep_us = 0;

    while(!przerwanie){
        pthread_mutex_lock(&mutex_kasy);
        if(id >= czynne_kasy){
            pthread_mutex_unlock(&mutex_kasy);
            if(przerwanie) break;
            sched_yield();
            continue;
        }
        pthread_mutex_unlock(&mutex_kasy);

        if(sem_wait(&items_sem) != 0){
            if(przerwanie) break;
            continue;
        }

        if(przerwanie) break;

        pthread_mutex_lock(&mutex_kolejka);
        Kibic* k = NULL;
        if(qv_size > 0){
            k = kolejka_vip[qv_start];
            kolejka_vip[qv_start] = NULL;
            qv_start = (qv_start + 1) % MAX_KOLEJKA;
            qv_size--;
        } else if(q_size > 0){
            k = kolejka[q_start];
            kolejka[q_start] = NULL;
            q_start = (q_start + 1) % MAX_KOLEJKA;
            q_size--;
            if(kibice_w_kolejce > 0) kibice_w_kolejce--;
        }
        if(k){
            __sync_fetch_and_add(&dequeued_count, 1);
            k->in_queue = 0;
            k->processing = 1;
        }
        pthread_mutex_unlock(&mutex_kolejka);

        if(!k) continue;
        if(k->sektor == -2){
            k->processing = 0;
            continue;
        }

        if(service_usleep_us > 0) usleep(service_usleep_us);

        pthread_mutex_lock(&mutex_bilety);
        if(przerwanie){
            pthread_mutex_unlock(&mutex_bilety);
            break;
        }

        /* losowy wybór sektora spośród dostępnych */
        int avail[SEKTORY];
        int ac = 0;
        for(int s = 0; s < SEKTORY; ++s){
            if(miejsca[s] >= k->bilety) avail[ac++] = s;
        }

        int chosen = -1;
        if(ac > 0){
            chosen = avail[rand() % ac];
        }

        if(chosen >= 0){
            miejsca[chosen] -= k->bilety;
            sprzedane += k->bilety;
            sold_per_sector[chosen] += k->bilety;

            /* ustaw stan przed broadcastem */
            k->to_enter = k->bilety;
            k->sektor = chosen;

            if (k->wiek < 18) {
                stat_dzieci += k->bilety;
            } else if (k->vip) {
                /* VIP: operacje pod mutex_vip, jednostka = bilety */
                pthread_mutex_lock(&mutex_vip);
                if (vip_reserved >= k->bilety) {
                    vip_reserved -= k->bilety;
                } else {
                    vip_reserved = 0;
                }
                vip_sold += k->bilety;
                sprzedane_vip += k->bilety;
                pthread_mutex_unlock(&mutex_vip);
            } else {
                stat_normalni += k->bilety;
            }

            __sync_fetch_and_add(&sold_count, k->bilety);

            snprintf(buf, sizeof(buf), "Kasa %d: sprzedano %d bilety kibicowi %d do sektora %d (sprzedane=%d)",
                     id, k->bilety, k->id, chosen, sprzedane);
            loguj(buf);
        } else {
            /* brak miejsc: anuluj i obudź kibica, cofnij rezerwację VIP jeśli była */
            k->sektor = -2;
            k->to_enter = 0;
            if (k->vip) {
                pthread_mutex_lock(&mutex_vip);
                if (vip_reserved >= k->bilety) vip_reserved -= k->bilety;
                pthread_mutex_unlock(&mutex_vip);
            }
            snprintf(buf, sizeof(buf), "Kasa %d: brak miejsc dla kibica %d, anulowano", id, k->id);
            loguj(buf);

            pthread_mutex_lock(&mutex_wejsc);
            pthread_cond_broadcast(&cond_wejsc);
            pthread_mutex_unlock(&mutex_wejsc);

            pthread_mutex_unlock(&mutex_bilety);
            k->processing = 0;
            continue;
        }

        pthread_mutex_unlock(&mutex_bilety);

        /* powiadom wejścia po ustawieniu stanu */
        pthread_mutex_lock(&mutex_wejsc);
        pthread_cond_broadcast(&cond_wejsc);
        pthread_mutex_unlock(&mutex_wejsc);

        if(k->sektor >= 0){
            snprintf(buf, sizeof(buf), "Kasa %d: to_enter=%d dla kibica %d sektor=%d", id, k->to_enter, k->id, k->sektor);
            loguj(buf);
        }

        k->processing = 0;

        if(przerwanie) break;
    }

    snprintf(buf, sizeof(buf), "Kasa %d: zamyka sie", id);
    loguj(buf);
    return NULL;
}
