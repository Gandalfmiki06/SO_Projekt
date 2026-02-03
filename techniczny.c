#define _XOPEN_SOURCE 700
#include <unistd.h>

#include "shared.h"
#include "log.h"

void proc_techniczny(void)
{
    loguj("Techniczny: start");
    while(!SH->przerwanie){
        if(SH->ewakuacja){
            pthread_mutex_lock(&SH->mutex_wejsc);
            for(int s=0;s<SEKTORY;s++){
                if(SH->osoby_w_sektorze[s] > 0){
                    loguj("Techniczny: sektor %d opuszcza %d osob", s, SH->osoby_w_sektorze[s]);
                    SH->osoby_w_sektorze[s] = 0;
                }
            }
            pthread_mutex_unlock(&SH->mutex_wejsc);
            break;
        }
        msleep(200);
    }
    loguj("Techniczny: koniec");
    _exit(0);
}
