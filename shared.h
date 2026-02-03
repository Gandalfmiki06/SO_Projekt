#ifndef SHARED_H
#define SHARED_H

#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define SEKTORY 8
#define MAX_KOLEJKA 2000
#define MAX_KASY 10

typedef struct {
    int id;
    int vip;
    int wiek;
    int druzyna;
    int bilety;
    int sektor;          /* -1 brak, -2 anulowany, >=0 sektor */
    int has_ticket;      /* 1 gdy kasa przydzieliła bilety */
    int guardian_id;     /* id opiekuna */
    int is_child;        /* 1 jeśli <15 */
    int pass_count;
    int entered;
} Kibic;

typedef struct {
    int K;
    int miejsca[SEKTORY];
    int sprzedane;
    int sold_per_sector[SEKTORY];

    int max_vip;
    int vip_reserved;
    int vip_sold;
    int vip_entered;

    int stat_normalni;
    int stat_dzieci;
    int stat_wejsc;
    int stat_vip_wejsc;

    int stop_sektor[SEKTORY];
    int osoby_w_sektorze[SEKTORY];

    int q_start, q_end, q_size;
    int q_ids[MAX_KOLEJKA];

    int qv_start, qv_end, qv_size;
    int qv_ids[MAX_KOLEJKA];

    Kibic kibice[MAX_KOLEJKA];
    int next_kibic_id;

    int stanowisko_count[SEKTORY][2];
    int stanowisko_druzyna[SEKTORY][2];

    int total_kibice;
    int finished_kibice;

    time_t Tp;

    int ewakuacja;
    int przerwanie;

    int czynne_kasy;

    pthread_mutex_t mutex_log;
    pthread_mutex_t mutex_kolejka;
    pthread_mutex_t mutex_bilety;
    pthread_mutex_t mutex_wejsc;
    pthread_mutex_t mutex_global;

    pthread_cond_t cond_kolejka;
    pthread_cond_t cond_wejsc;
    pthread_cond_t cond_global;

    sem_t items_sem;
} Shared;

extern Shared *SH;

/* inicjalizacja i sprzątanie pamięci współdzielonej */
void shared_init(int K);
void shared_cleanup(void);

/* pomocnicze: msleep (milisekundy) - używa nanosleep, przenośne i bez warningów */
static inline void msleep(int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

#endif
