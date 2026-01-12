#ifndef HALA_H
#define HALA_H

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdbool.h>

#define SEKTORY 8
#define MAX_KASY 10
#define MAX_KOLEJKA 1000

/* ===== STRUKTURY ===== */

typedef struct {
    int id;
    int vip;        // 1 = VIP
    int sektor;
    int bilety;     // 0–2
} Kibic;

/* ===== ZASOBY WSPÓLNE ===== */

extern int K;
extern int miejsca[SEKTORY];
extern int sprzedane;

/* ===== KASY ===== */

extern int czynne_kasy;
extern pthread_mutex_t mutex_kasy;

/* ===== KOLEJKA ===== */

extern Kibic* kolejka[MAX_KOLEJKA];
extern int q_start, q_end, q_size;
extern int kibice_w_kolejce;

extern pthread_mutex_t mutex_kolejka;
extern pthread_cond_t cond_kolejka;

/* ===== BILETY ===== */

extern pthread_mutex_t mutex_bilety;

/* ===== LOG ===== */

extern pthread_mutex_t mutex_log;
void loguj(const char* tekst);

/* ===== FUNKCJE WĄTKÓW ===== */

void* kasa(void* arg);
void* kibic(void* arg);

/* ===== WEJŚCIA / EWAKUACJA ===== */

extern int stop_wejsc;
extern int ewakuacja;

extern pthread_mutex_t mutex_wejsc;
extern pthread_cond_t cond_wejsc;


#endif
