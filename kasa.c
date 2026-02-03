#define _XOPEN_SOURCE 700
#include <unistd.h>

#include "shared.h"
#include "log.h"
#include <stdlib.h>
#include <errno.h>
#include <time.h>

static int queue_pop(void)
{
    pthread_mutex_lock(&SH->mutex_kolejka);
    int kid = -1;
    if(SH->qv_size > 0){
        kid = SH->qv_ids[SH->qv_start];
        SH->qv_start = (SH->qv_start + 1) % MAX_KOLEJKA;
        SH->qv_size--;
    } else if(SH->q_size > 0){
        kid = SH->q_ids[SH->q_start];
        SH->q_start = (SH->q_start + 1) % MAX_KOLEJKA;
        SH->q_size--;
    }
    pthread_mutex_unlock(&SH->mutex_kolejka);
    return kid;
}

void proc_kasa(int id)
{
    srand((unsigned int)(time(NULL) ^ (getpid()<<16)));
    loguj("Kasa %d: start", id);

    while(!SH->przerwanie){
        pthread_mutex_lock(&SH->mutex_global);
        int czynne = SH->czynne_kasy;
        pthread_mutex_unlock(&SH->mutex_global);
        if(id >= czynne){
            msleep(50);
            continue;
        }

        if(sem_wait(&SH->items_sem) == -1){
            if(errno == EINTR) continue;
        }
        if(SH->przerwanie) break;

        int kid = queue_pop();
        if(kid < 0) continue;

        Kibic *k = &SH->kibice[kid];

        pthread_mutex_lock(&SH->mutex_bilety);
        if(SH->sprzedane >= SH->K){
            pthread_mutex_unlock(&SH->mutex_bilety);
            k->sektor = -2;
            k->has_ticket = 0;
            continue;
        }

        int avail[SEKTORY];
        int ac = 0;
        for(int s=0;s<SEKTORY;s++){
            if(SH->miejsca[s] >= k->bilety){
                avail[ac++] = s;
            }
        }
        if(ac == 0){
            k->sektor = -2;
            k->has_ticket = 0;
            pthread_mutex_unlock(&SH->mutex_bilety);
            loguj("Kasa %d: brak miejsc dla kibica %d", id, k->id);
            continue;
        }
        int chosen = avail[rand()%ac];
        SH->miejsca[chosen] -= k->bilety;
        SH->sprzedane += k->bilety;
        SH->sold_per_sector[chosen] += k->bilety;

        k->sektor = chosen;
        k->has_ticket = 1;

        if(k->vip){
            SH->vip_sold += k->bilety;
        } else if(k->is_child){
            SH->stat_dzieci += k->bilety;
        } else {
            SH->stat_normalni += k->bilety;
        }

        loguj("Kasa %d: sprzedano %d bilety kibicowi %d do sektora %d (sprzedane=%d)",
              id, k->bilety, k->id, chosen, SH->sprzedane);

        pthread_mutex_unlock(&SH->mutex_bilety);

        pthread_cond_broadcast(&SH->cond_wejsc);

        if(SH->sprzedane >= SH->K){
            loguj("Kasa %d: wszystkie bilety sprzedane", id);
            break;
        }
    }

    loguj("Kasa %d: koniec", id);
    _exit(0);
}
