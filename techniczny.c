/* techniczny.c - centralne potwierdzanie wejść, bezpieczne vip_entered, inkrementacja pass_count */

#include "hala.h"
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

void* techniczny(void* arg)
{
    (void)arg;
    char buf[256];

    while(!przerwanie){
        int any = 0;
        pthread_mutex_lock(&mutex_kolejka);
        for(int i=0;i<MAX_KOLEJKA;i++){
            Kibic* k = created_kibice[i];
            if(!k) continue;
            /* przepuszczaj tylko nieanulowanych i nieprzepuszczonych */
            if(k->to_enter > 0 && k->sektor >= 0 && entered_flag[k->id] == 0){
                int n = k->to_enter;

                /* techniczny potwierdza wejście */
                k->to_enter = 0;
                entered_flag[k->id] = 1;

                /* aktualizacje globalne - tylko tutaj */
                adults_entered_in_sector[k->sektor] += n;
                __sync_fetch_and_add(&stat_wejsc, n);

                if(k->vip) {
                    /* bezpieczna aktualizacja vip_entered: nie przekraczaj vip_sold */
                    pthread_mutex_lock(&mutex_vip);
                    int can_add = vip_sold - vip_entered;
                    int add = n;
                    if (add > can_add) add = can_add;
                    if (add > 0) vip_entered += add;
                    pthread_mutex_unlock(&mutex_vip);
                    if (add > 0) __sync_fetch_and_add(&stat_vip_wejsc, add);
                    if (add < n) {
                        snprintf(buf, sizeof(buf), "WARN: techniczny ograniczyl VIP wejsc z %d do %d (vip_sold=%d vip_entered=%d)", n, add, vip_sold, vip_entered);
                        loguj(buf);
                    }
                }

                snprintf(buf, sizeof(buf), "Techniczny: potwierdzono wejscie %d osob kibica %d do sektora %d", n, k->id, k->sektor);
                loguj(buf);

                /* inkrementuj pass_count dla pominiętych (id < k->id) w tym samym sektorze */
                for(int j=0;j<MAX_KOLEJKA;j++){
                    Kibic* w = created_kibice[j];
                    if(!w) continue;
                    if(w->in_queue && w->sektor == k->sektor && w->id < k->id){
                        w->pass_count++;
                        if(w->pass_count > 5){
                            /* oznacz opuszczenie kolejki; cofamy rezerwację VIP jeśli była */
                            w->sektor = -2;
                            w->to_enter = 0;
                            if(w->vip){
                                pthread_mutex_lock(&mutex_vip);
                                if (vip_reserved >= w->bilety) vip_reserved -= w->bilety;
                                pthread_mutex_unlock(&mutex_vip);
                            }
                            snprintf(buf, sizeof(buf), "Techniczny: kibic %d przekroczyl pass_count=%d i opuszcza kolejke", w->id, w->pass_count);
                            loguj(buf);
                        }
                    }
                }

                any = 1;
            }
        }
        pthread_mutex_unlock(&mutex_kolejka);

        if(any){
            pthread_mutex_lock(&mutex_wejsc);
            pthread_cond_broadcast(&cond_wejsc); /* powiadom kibiców, że są potwierdzeni */
            pthread_mutex_unlock(&mutex_wejsc);
        }

        for(int i=0;i<5 && !przerwanie;i++) usleep(100);
    }

    loguj("Techniczny: zamyka sie");
    return NULL;
}
