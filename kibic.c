#define _XOPEN_SOURCE 700
#include <unistd.h>

#include "shared.h"
#include "log.h"
#include <stdlib.h>
#include <time.h>

static int queue_push(int vip, int kid)
{
    pthread_mutex_lock(&SH->mutex_kolejka);
    int ok = 0;
    if(vip){
        if(SH->qv_size < MAX_KOLEJKA){
            SH->qv_ids[SH->qv_end] = kid;
            SH->qv_end = (SH->qv_end + 1) % MAX_KOLEJKA;
            SH->qv_size++;
            ok = 1;
        }
    } else {
        if(SH->q_size < MAX_KOLEJKA){
            SH->q_ids[SH->q_end] = kid;
            SH->q_end = (SH->q_end + 1) % MAX_KOLEJKA;
            SH->q_size++;
            ok = 1;
        }
    }
    pthread_mutex_unlock(&SH->mutex_kolejka);
    if(ok){
        sem_post(&SH->items_sem);
        pthread_cond_broadcast(&SH->cond_kolejka);
    }
    return ok;
}

static void przejdz_kontrole(Kibic *k)
{
    if(k->vip){
        pthread_mutex_lock(&SH->mutex_wejsc);
        SH->vip_entered += k->bilety;
        SH->stat_vip_wejsc += k->bilety;
        pthread_mutex_unlock(&SH->mutex_wejsc);
        loguj("Kibic %d (VIP): wszedl bez kontroli", k->id);
        return;
    }

    int s = k->sektor;
    if(s < 0 || s >= SEKTORY) return;

    int remaining = k->bilety;
    while(remaining > 0 && !SH->przerwanie && !SH->ewakuacja){
        pthread_mutex_lock(&SH->mutex_wejsc);
        if(SH->stop_sektor[s]){
            pthread_mutex_unlock(&SH->mutex_wejsc);
            msleep(50);
            continue;
        }

        int assigned = 0;
        for(int st=0;st<2;st++){
            int cnt = SH->stanowisko_count[s][st];
            int team = SH->stanowisko_druzyna[s][st];
            if(cnt < 3 && (team == -1 || team == k->druzyna)){
                SH->stanowisko_count[s][st] = cnt + 1;
                SH->stanowisko_druzyna[s][st] = k->druzyna;
                assigned = 1;
                break;
            }
        }
        pthread_mutex_unlock(&SH->mutex_wejsc);

        if(!assigned){
            k->pass_count++;
            if(k->pass_count > 5){
                loguj("Kibic %d: przekroczyl pass_count, rezygnuje z wejscia", k->id);
                return;
            }
            msleep(50);
            continue;
        }

        msleep(50 + (rand()%150));

        pthread_mutex_lock(&SH->mutex_wejsc);
        for(int st=0;st<2;st++){
            if(SH->stanowisko_druzyna[s][st] == k->druzyna &&
               SH->stanowisko_count[s][st] > 0){
                SH->stanowisko_count[s][st]--;
                if(SH->stanowisko_count[s][st] == 0){
                    SH->stanowisko_druzyna[s][st] = -1;
                }
                break;
            }
        }
        SH->osoby_w_sektorze[s] += 1;
        SH->stat_wejsc += 1;
        pthread_mutex_unlock(&SH->mutex_wejsc);

        remaining--;
        loguj("Kibic %d: przeszedl kontrole do sektora %d (pozostalo=%d)",
              k->id, s, remaining);
    }
}

void proc_kibic(int kid)
{
    Kibic *k = &SH->kibice[kid];

    if(!queue_push(k->vip, kid)){
        loguj("Kibic %d: kolejka pelna, rezygnuje", k->id);
        _exit(0);
    }
    loguj("Kibic %d: dolaczyl do kolejki (vip=%d)", k->id, k->vip);

    time_t start = time(NULL);
    while(!SH->przerwanie && !SH->ewakuacja){
        if(k->has_ticket || k->sektor == -2) break;
        if(time(NULL) - start > 120){
            loguj("Kibic %d: timeout w kolejce, rezygnuje", k->id);
            _exit(0);
        }
        msleep(50);
    }

    if(k->sektor == -2 || !k->has_ticket){
        loguj("Kibic %d: nie otrzymal biletu", k->id);
        _exit(0);
    }

    loguj("Kibic %d: ma bilet do sektora %d", k->id, k->sektor);

    if(k->is_child && k->guardian_id >= 0){
        loguj("Kibic %d: dziecko, opiekun=%d", k->id, k->guardian_id);
    }

    przejdz_kontrole(k);

    pthread_mutex_lock(&SH->mutex_global);
    SH->finished_kibice++;
    pthread_mutex_unlock(&SH->mutex_global);

    _exit(0);
}
