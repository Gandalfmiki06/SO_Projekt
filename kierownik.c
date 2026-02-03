#define _XOPEN_SOURCE 700
#include <unistd.h>

#include "shared.h"
#include "log.h"
#include <stdlib.h>
#include <time.h>

void proc_kierownik(void)
{
    loguj("Kierownik: start");
    unsigned int seed = (unsigned int)time(NULL);

    while(!SH->przerwanie){
        pthread_mutex_lock(&SH->mutex_kolejka);
        int q = SH->q_size + SH->qv_size;
        pthread_mutex_unlock(&SH->mutex_kolejka);

        int baza = (SH->K / 10);
        if(baza <= 0) baza = 1;
        int potrzebne = q / baza + 1;
        if(potrzebne < 2) potrzebne = 2;
        if(potrzebne > MAX_KASY) potrzebne = MAX_KASY;

        pthread_mutex_lock(&SH->mutex_global);
        if(potrzebne != SH->czynne_kasy){
            loguj("Kierownik: zmiana liczby kas z %d na %d (kolejka=%d, baza=%d)",
                  SH->czynne_kasy, potrzebne, q, baza);
            SH->czynne_kasy = potrzebne;
        }
        pthread_mutex_unlock(&SH->mutex_global);

        if(rand_r(&seed) % 20 == 0){
            int s = rand_r(&seed) % SEKTORY;
            int akcja = rand_r(&seed) % 2;
            pthread_mutex_lock(&SH->mutex_wejsc);
            if(akcja == 0){
                if(!SH->stop_sektor[s]){
                    SH->stop_sektor[s] = 1;
                    loguj("Kierownik: sygnal1 - wstrzymanie wejscia do sektora %d", s);
                }
            } else {
                if(SH->stop_sektor[s]){
                    SH->stop_sektor[s] = 0;
                    loguj("Kierownik: sygnal2 - wznowienie wejscia do sektora %d", s);
                }
            }
            pthread_mutex_unlock(&SH->mutex_wejsc);
        }

        time_t now = time(NULL);
        if(now >= SH->Tp && !SH->ewakuacja){
            SH->ewakuacja = 1;
            loguj("Kierownik: sygnal3 - ewakuacja");
        }

        pthread_mutex_lock(&SH->mutex_global);
        int done = SH->finished_kibice;
        int total = SH->total_kibice;
        pthread_mutex_unlock(&SH->mutex_global);
        if(total > 0 && done >= total){
            loguj("Kierownik: wszyscy kibice obsluzeni, koniec");
            break;
        }

        sleep(1);
    }

    _exit(0);
}
