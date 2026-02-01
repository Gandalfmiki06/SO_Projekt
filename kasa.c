#include "hala.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

static int all_queue_need_two_and_no_sector(void)
{
    int all_need_two = 1;
    int found_block = 0;

    pthread_mutex_lock(&mutex_kolejka);
    if(q_size == 0 && qv_size == 0){
        pthread_mutex_unlock(&mutex_kolejka);
        return 0;
    }

    for(int i=0;i<qv_size;i++){
        int idx = (qv_start + i) % MAX_KOLEJKA;
        Kibic *kk = kolejka_vip[idx];
        if(kk && kk->bilety < 2){ all_need_two = 0; break; }
    }
    if(all_need_two){
        for(int i=0;i<q_size;i++){
            int idx = (q_start + i) % MAX_KOLEJKA;
            Kibic *kk = kolejka[idx];
            if(kk && kk->bilety < 2){ all_need_two = 0; break; }
        }
    }
    pthread_mutex_unlock(&mutex_kolejka);

    if(!all_need_two) return 0;

    pthread_mutex_lock(&mutex_bilety);
    for(int s=0;s<SEKTORY;s++){
        if(miejsca[s] >= 2){ found_block = 1; break; }
    }
    pthread_mutex_unlock(&mutex_bilety);

    return (!found_block && all_need_two) ? 1 : 0;
}

void* kasa(void* arg)
{
    int id = (int)(size_t)arg;
    char buf[256];
    unsigned int seed = (unsigned int)time(NULL) ^ (id<<8);

    while(!przerwanie){
        pthread_mutex_lock(&mutex_kasy);
        if(id >= czynne_kasy){
            pthread_mutex_unlock(&mutex_kasy);
            usleep(50000);
            continue;
        }
        pthread_mutex_unlock(&mutex_kasy);

        pthread_mutex_lock(&mutex_kolejka);
        while(q_size == 0 && qv_size == 0 && !przerwanie && remaining_arrivals > 0 && sprzedane < K){
            pthread_cond_wait(&cond_kolejka, &mutex_kolejka);
        }

        if((q_size == 0 && qv_size == 0) && (remaining_arrivals == 0 || sprzedane >= K || przerwanie)){
            pthread_mutex_unlock(&mutex_kolejka);
            break;
        }

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
            k->in_queue = 0;
            k->processing = 1;
        }
        pthread_mutex_unlock(&mutex_kolejka);

        if(!k){
            usleep(20000);
            continue;
        }

        snprintf(buf, sizeof(buf), "Kasa %d: pobral kibica %d (bilety=%d vip=%d)", id, k->id, k->bilety, k->vip);
        loguj(buf);

        /* OBSŁUGA VIP – osobny sektor, bez przydziału zwykłego sektora */
        if(k->vip){
            pthread_mutex_lock(&mutex_bilety);
            if(miejsca_vip > 0){
                miejsca_vip--;
                sprzedane_vip++;

                pthread_mutex_lock(&mutex_wejsc);
                pthread_mutex_lock(&mutex_kolejka);
                k->to_enter = 1;
                k->sektor = -10; /* znacznik: sektor VIP */
                k->processing = 0;
                pthread_cond_broadcast(&cond_kolejka);
                pthread_mutex_unlock(&mutex_kolejka);
                pthread_cond_broadcast(&cond_wejsc);
                pthread_mutex_unlock(&mutex_wejsc);

                pthread_mutex_unlock(&mutex_bilety);

                snprintf(buf, sizeof(buf), "Kasa %d: sprzedano bilet VIP dla kibica %d (miejsca_vip=%d)", id, k->id, miejsca_vip);
                loguj(buf);
            } else {
                pthread_mutex_lock(&mutex_kolejka);
                k->processing = 0;
                pthread_mutex_unlock(&mutex_kolejka);

                k->sektor = -2;
                pthread_mutex_lock(&mutex_wejsc);
                pthread_cond_broadcast(&cond_wejsc);
                pthread_mutex_unlock(&mutex_wejsc);

                pthread_mutex_unlock(&mutex_bilety);

                snprintf(buf, sizeof(buf), "Kasa %d: brak miejsc w sektorze VIP dla kibica %d — anulowano", id, k->id);
                loguj(buf);
            }

            usleep(20000);
            continue;
        }

        /* Sprzedaż i przypisanie sektora dla zwykłych kibiców */
        pthread_mutex_lock(&mutex_bilety);
        int wybrany = -1;

        if(k->sektor < 0){
            int start = rand_r(&seed) % SEKTORY;
            for(int i=0;i<SEKTORY;i++){
                int s = (start + i) % SEKTORY;
                if(miejsca[s] >= k->bilety){
                    wybrany = s;
                    break;
                }
            }

            if(wybrany == -1){
                if(all_queue_need_two_and_no_sector()){
                    snprintf(buf, sizeof(buf), "Kasa %d: brak sektora z 2 miejscami i wszyscy w kolejce potrzebuja >=2 -> koniec symulacji", id);
                    loguj(buf);

                    przerwanie = 1;

                    pthread_mutex_lock(&mutex_kolejka);
                    pthread_cond_broadcast(&cond_kolejka);
                    pthread_mutex_unlock(&mutex_kolejka);

                    pthread_mutex_lock(&mutex_wejsc);
                    pthread_cond_broadcast(&cond_wejsc);
                    pthread_mutex_unlock(&mutex_wejsc);

                    pthread_mutex_unlock(&mutex_bilety);

                    pthread_mutex_lock(&mutex_kolejka);
                    k->processing = 0;
                    pthread_mutex_unlock(&mutex_kolejka);
                    break;
                }

                int best = -1, best_free = 0;
                for(int s=0;s<SEKTORY;s++){
                    if(miejsca[s] > best_free){
                        best_free = miejsca[s];
                        best = s;
                    }
                }

                if(best_free <= 0){
                    pthread_mutex_lock(&mutex_kolejka);
                    k->processing = 0;
                    pthread_mutex_unlock(&mutex_kolejka);

                    k->sektor = -2;
                    snprintf(buf, sizeof(buf), "Kasa %d: brak miejsc dla kibica %d — anulowano", id, k->id);
                    loguj(buf);

                    pthread_mutex_lock(&mutex_wejsc);
                    pthread_cond_broadcast(&cond_wejsc);
                    pthread_mutex_unlock(&mutex_wejsc);

                    pthread_mutex_unlock(&mutex_bilety);
                    continue;
                } else {
                    miejsca[best] -= 1;
                    sprzedane += 1;
                    sold_per_sector[best] += 1;

                    pthread_mutex_lock(&mutex_wejsc);
                    pthread_mutex_lock(&mutex_kolejka);
                    k->to_enter = 1;
                    k->sektor = best;
                    if(k->bilety > 0) k->bilety -= 1;
                    k->processing = 0;
                    pthread_cond_broadcast(&cond_kolejka);
                    pthread_mutex_unlock(&mutex_kolejka);
                    pthread_cond_broadcast(&cond_wejsc);
                    pthread_mutex_unlock(&mutex_wejsc);

                    pthread_mutex_unlock(&mutex_bilety);

                    snprintf(buf, sizeof(buf), "DEBUG-KASA %d: fallback sprzedano 1 bilet sektor %d dla kibica %d", id, best, k->id);
                    loguj(buf);

                    usleep(10000);
                    continue;
                }
            } else {
                miejsca[wybrany] -= k->bilety;
                sprzedane += k->bilety;
                sold_per_sector[wybrany] += k->bilety;

                pthread_mutex_lock(&mutex_wejsc);
                pthread_mutex_lock(&mutex_kolejka);
                k->to_enter = k->bilety;
                k->sektor = wybrany;
                k->bilety = 0;
                k->processing = 0;
                pthread_cond_broadcast(&cond_kolejka);
                pthread_mutex_unlock(&mutex_kolejka);
                pthread_cond_broadcast(&cond_wejsc);
                pthread_mutex_unlock(&mutex_wejsc);

                pthread_mutex_unlock(&mutex_bilety);

                snprintf(buf, sizeof(buf), "Kasa %d: sprzedano bilety sektor %d dla kibica %d", id, wybrany, k->id);
                loguj(buf);

                snprintf(buf, sizeof(buf), "DEBUG-KASA %d: ustawiono sektor=%d dla kibica=%d i wykonano broadcast cond_wejsc", id, wybrany, k->id);
                loguj(buf);

                if(sprzedane >= K){
                    pthread_mutex_lock(&mutex_kolejka);
                    pthread_cond_broadcast(&cond_kolejka);
                    pthread_mutex_unlock(&mutex_kolejka);
                    pthread_mutex_lock(&mutex_wejsc);
                    pthread_cond_broadcast(&cond_wejsc);
                    pthread_mutex_unlock(&mutex_wejsc);
                }

                usleep(20000);
                continue;
            }
        } else {
            if(k->sektor >= 0 && k->sektor < SEKTORY && miejsca[k->sektor] >= k->bilety){
                miejsca[k->sektor] -= k->bilety;
                sprzedane += k->bilety;
                sold_per_sector[k->sektor] += k->bilety;

                pthread_mutex_lock(&mutex_wejsc);
                pthread_mutex_lock(&mutex_kolejka);
                k->to_enter = k->bilety;
                k->bilety = 0;
                k->processing = 0;
                pthread_cond_broadcast(&cond_kolejka);
                pthread_mutex_unlock(&mutex_kolejka);
                pthread_cond_broadcast(&cond_wejsc);
                pthread_mutex_unlock(&mutex_wejsc);

                pthread_mutex_unlock(&mutex_bilety);

                snprintf(buf, sizeof(buf), "Kasa %d: potwierdzono i sprzedano dla kibica %d sektor %d", id, k->id, k->sektor);
                loguj(buf);

                usleep(10000);
                continue;
            } else {
                pthread_mutex_lock(&mutex_kolejka);
                k->processing = 0;
                k->sektor = -1;
                if(q_size < MAX_KOLEJKA){
                    kolejka[q_end] = k;
                    q_end = (q_end + 1) % MAX_KOLEJKA;
                    q_size++;
                    kibice_w_kolejce++;
                    k->in_queue = 1;
                    pthread_cond_broadcast(&cond_kolejka);
                } else {
                    k->sektor = -2;
                    pthread_mutex_lock(&mutex_wejsc);
                    pthread_cond_broadcast(&cond_wejsc);
                    pthread_mutex_unlock(&mutex_wejsc);
                }
                pthread_mutex_unlock(&mutex_kolejka);

                pthread_mutex_unlock(&mutex_bilety);
                continue;
            }
        }
    }

    snprintf(buf, sizeof(buf), "Kasa %d: zamyka sie", id);
    loguj(buf);
    return NULL;
}
