#define _XOPEN_SOURCE 700
#include <unistd.h>

#include "shared.h"
#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

static const char *LOG_FILE = "raport_proc.txt";

void loguj(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char buf[1024];

    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    char timestr[64];
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &tm);

    int off = snprintf(buf, sizeof(buf), "[%s] [PID=%ld] ",
                       timestr, (long)getpid());
    vsnprintf(buf + off, sizeof(buf) - off, fmt, ap);
    va_end(ap);

    pthread_mutex_lock(&SH->mutex_log);
    FILE *f = fopen(LOG_FILE, "a");
    if(f){
        fprintf(f, "%s\n", buf);
        fclose(f);
    }
    pthread_mutex_unlock(&SH->mutex_log);
}

void zapisz_podsumowanie(void)
{
    pthread_mutex_lock(&SH->mutex_log);
    FILE *f = fopen("raport_proc.txt", "a");
    if(!f){
        pthread_mutex_unlock(&SH->mutex_log);
        return;
    }
    fprintf(f, "\n===== PODSUMOWANIE =====\n");
    fprintf(f, "K = %d\n", SH->K);
    fprintf(f, "Sprzedane: %d\n", SH->sprzedane);
    fprintf(f, "VIP reserved: %d\nVIP sold: %d\nVIP entered: %d\n",
            SH->vip_reserved, SH->vip_sold, SH->vip_entered);
    fprintf(f, "Normalni: %d\nDzieci: %d\nWeszli: %d\nVIP wejscia: %d\n",
            SH->stat_normalni, SH->stat_dzieci, SH->stat_wejsc, SH->stat_vip_wejsc);

    fprintf(f, "Sprzedane na sektorach: ");
    for(int s=0;s<SEKTORY;s++){
        fprintf(f, "%d%s", SH->sold_per_sector[s], (s<SEKTORY-1?"; ":"\n"));
    }
    fprintf(f, "Miejsca pozostale na sektory: ");
    for(int s=0;s<SEKTORY;s++){
        fprintf(f, "%d%s", SH->miejsca[s], (s<SEKTORY-1?"; ":"\n"));
    }
    fclose(f);
    pthread_mutex_unlock(&SH->mutex_log);
}
