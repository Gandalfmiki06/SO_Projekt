#ifndef SHARED_H
#define SHARED_H

#include <pthread.h>
#include <semaphore.h>
#include <time.h>

/* 8 sektorów zwykłych + 1 sektor VIP */
#define SEKTORY 9
#define SEKTOR_VIP 8

#define MAX_KOLEJKA 200000
#define MAX_KASY 10

typedef struct {
    int id;
    int vip;            /* 1 = VIP */
    int wiek;
    int druzyna;        /* -1 dla VIP */
    int bilety;         /* 1 lub 2 */
    int sektor;         /* 0–7 zwykłe, 8 = VIP, -1 brak, -2 odrzucony */
    int has_ticket;     /* 1 gdy kasa/VIP przydzieliła bilety */
    int guardian_id;    /* id opiekuna */
    int is_child;       /* 1 jeśli <15 */
    int pass_count;
    int entered;
} Kibic;

typedef struct {

    /* --- Pojemność stadionu --- */
    int K;                          /* pojemność sektorów 0–7 */
    int miejsca[SEKTORY];           /* miejsca w sektorach 0–7 i sektorze VIP */
    int sprzedane;                  /* łącznie sprzedane bilety (z VIP) */
    int sold_per_sector[SEKTORY];

    /* --- VIP --- */
    int max_vip;                    /* liczba VIP (osób), nie biletów */
    int vip_reserved;               /* ilu VIP wygenerowano */
    int vip_sold;                   /* ile biletów VIP sprzedano */
    int vip_entered;                /* ile biletów VIP weszło */

    /* --- Statystyki --- */
    int stat_normalni;
    int stat_dzieci;
    int stat_wejsc;
    int stat_vip_wejsc;

    /* --- Kontrola wejścia (tylko sektory 0–7) --- */
    int stop_sektor[SEKTORY];       /* sektor VIP ignoruje kontrolę */
    int osoby_w_sektorze[SEKTORY];

    int stanowisko_count[SEKTORY][2];
    int stanowisko_druzyna[SEKTORY][2];

    /* --- Kolejki (VIP nie używają kolejki) --- */
    int q_start, q_end, q_size;
    int q_ids[MAX_KOLEJKA];

    Kibic kibice[MAX_KOLEJKA];
    int next_kibic_id;

    /* --- Kibice --- */
    int total_kibice;
    int finished_kibice;

    /* --- Czas meczu --- */
    time_t Tp;
    int mecz_started;

    /* --- Sterowanie --- */
    int ewakuacja;
    int przerwanie;

    int czynne_kasy;

    /* --- Synchronizacja --- */
    pthread_mutex_t mutex_log;
    pthread_mutex_t mutex_kolejka;
    pthread_mutex_t mutex_bilety;
    pthread_mutex_t mutex_wejsc;
    pthread_mutex_t mutex_global;

    pthread_cond_t cond_kolejka;
    pthread_cond_t cond_wejsc;
    pthread_cond_t cond_global;

    sem_t items_sem; /* tylko dla zwykłych kibiców */

} Shared;

extern Shared *SH;

void shared_init_master(int K);
void shared_attach(void);
void shared_cleanup(void);

static inline void msleep(int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

#endif
