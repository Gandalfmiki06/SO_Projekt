#include "hala.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void* kierownik(void* arg)
{
    (void)arg;
    char buf[256];
    unsigned int seed = (unsigned int)time(NULL);

    int open_close_count = 0;
    time_t sector_block_until[SEKTORY];
    for(int i=0;i<SEKTORY;i++) sector_block_until[i] = 0;

    while(!przerwanie){
        pthread_mutex_lock(&mutex_kolejka);
        int q = q_size;
        int qv = qv_size;
        pthread_mutex_unlock(&mutex_kolejka);

        /* jeśli nie ma już przyjść i kolejka pusta -> ewakuacja */
        if(remaining_arrivals == 0 && q == 0 && qv == 0){
            loguj("Kierownik: brak dalszych przyjsc, wymuszam ewakuacje");
            ewakuacja = 1;
            pthread_cond_broadcast(&cond_wejsc);
            pthread_cond_broadcast(&cond_kolejka);
            break;
        }

        int baza = (K/10);
        if(baza <= 0) baza = 1;
        int total_queue = q + qv;
        int potrzebne = total_queue / baza + 1;
        if(potrzebne < 2) potrzebne = 2;
        if(potrzebne > MAX_KASY) potrzebne = MAX_KASY;

        pthread_mutex_lock(&mutex_kasy);
        if(potrzebne != czynne_kasy){
            snprintf(buf, sizeof(buf), "Kierownik: zmiana liczby kas z %d na %d (kolejka=%d total=%d)", czynne_kasy, potrzebne, q, total_queue);
            loguj(buf);
            open_close_count++;
        }
        czynne_kasy = potrzebne;
        pthread_mutex_unlock(&mutex_kasy);

        time_t now = time(NULL);

        /* automatyczne wznowienie sektorów, których blokada wygasła */
        pthread_mutex_lock(&mutex_wejsc);
        for(int s=0; s<SEKTORY; s++){
            if(stop_sektor[s] && sector_block_until[s] > 0 && now >= sector_block_until[s]){
                stop_sektor[s] = 0;
                sector_block_until[s] = 0;
                pthread_cond_broadcast(&cond_wejsc);
                snprintf(buf, sizeof(buf), "Kierownik: sygnal2 - automatyczne wznowienie wejscia do sektora %d (timeout)", s);
                loguj(buf);
            }
        }
        pthread_mutex_unlock(&mutex_wejsc);

        /* losowe akcje: wstrzymanie / wznowienie sektorów */
        if(rand_r(&seed) % 20 == 0){
            int s = rand_r(&seed) % SEKTORY;
            int akcja = rand_r(&seed) % 2;

            pthread_mutex_lock(&mutex_wejsc);

            if(akcja == 0){
                if(!stop_sektor[s]){
                    int hold = 4 + (rand_r(&seed) % 7); /* 4..10 */
                    stop_sektor[s] = 1;
                    sector_block_until[s] = now + hold;
                    snprintf(buf, sizeof(buf), "Kierownik: sygnal1 - wstrzymanie wejscia do sektora %d (czas=%ds)", s, hold);
                    loguj(buf);
                } else {
                    snprintf(buf, sizeof(buf), "Kierownik DEBUG: sektor %d juz zablokowany, pomijam", s);
                    loguj(buf);
                }
            } else {
                if(stop_sektor[s]){
                    stop_sektor[s] = 0;
                    sector_block_until[s] = 0;
                    pthread_cond_broadcast(&cond_wejsc);
                    snprintf(buf, sizeof(buf), "Kierownik: sygnal2 - wznowienie wejscia do sektora %d", s);
                    loguj(buf);
                } else {
                    snprintf(buf, sizeof(buf), "Kierownik DEBUG: sektor %d juz otwarty, pomijam", s);
                    loguj(buf);
                }
            }

            pthread_mutex_unlock(&mutex_wejsc);
        }

        if(sprzedane >= K) break;
        sleep(1);
    }

    loguj("Kierownik: sygnal3 - ewakuacja");
    ewakuacja = 1;
    pthread_cond_broadcast(&cond_wejsc);
    pthread_cond_broadcast(&cond_kolejka);
    return NULL;
}
