#define _XOPEN_SOURCE 700
#include <unistd.h>

#include "shared.h"
#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/* Plik raportu */
static const char *LOG_FILE = "raport_proc.txt";

/* ---------------------------------------------------------
   loguj(fmt, ...)
   Zapisuje linię do raportu z timestampem i PID.
   --------------------------------------------------------- */
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
    if (f) {
        fprintf(f, "%s\n", buf);
        fclose(f);
    }
    pthread_mutex_unlock(&SH->mutex_log);
}

/* ---------------------------------------------------------
   zapisz_podsumowanie()
   Końcowy raport z całej symulacji.
   --------------------------------------------------------- */
void zapisz_podsumowanie(void)
{
    pthread_mutex_lock(&SH->mutex_log);
    FILE *f = fopen(LOG_FILE, "a");
    if (!f) {
        pthread_mutex_unlock(&SH->mutex_log);
        return;
    }

    fprintf(f, "\n===== PODSUMOWANIE =====\n");

    fprintf(f, "K (sektory 0–7) = %d\n", SH->K);
    fprintf(f, "Sprzedane (łącznie, z VIP) = %d\n", SH->sprzedane);

    fprintf(f, "VIP reserved (osoby): %d\n", SH->vip_reserved);
    fprintf(f, "VIP sold (bilety): %d\n", SH->vip_sold);
    fprintf(f, "VIP entered (bilety): %d\n", SH->vip_entered);

    fprintf(f, "Normalni: %d\n", SH->stat_normalni);
    fprintf(f, "Dzieci: %d\n", SH->stat_dzieci);
    fprintf(f, "Weszli (zwykli): %d\n", SH->stat_wejsc);
    fprintf(f, "VIP wejscia: %d\n", SH->stat_vip_wejsc);

    fprintf(f, "\nSprzedane na sektorach:\n");
    for (int s = 0; s < SEKTORY; s++) {
        if (s == SEKTOR_VIP)
            fprintf(f, "  sektor %d (VIP): %d\n", s, SH->sold_per_sector[s]);
        else
            fprintf(f, "  sektor %d: %d\n", s, SH->sold_per_sector[s]);
    }

    fprintf(f, "\nMiejsca pozostale:\n");
    for (int s = 0; s < SEKTORY; s++) {
        if (s == SEKTOR_VIP)
            fprintf(f, "  sektor %d (VIP): %d\n", s, SH->miejsca[s]);
        else
            fprintf(f, "  sektor %d: %d\n", s, SH->miejsca[s]);
    }

    fprintf(f, "\n===== KONIEC RAPORTU =====\n\n");

    fclose(f);
    pthread_mutex_unlock(&SH->mutex_log);
}
