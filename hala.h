#ifndef HALA_H
#define HALA_H

#include <pthread.h>
#include <semaphore.h>

#define SEKTORY 8
#define MAX_KOLEJKA 200
#define MAX_KASY 10

typedef struct {
    int id;
    int vip;
    int wiek;
    int druzyna;
    int bilety;
    int sektor;
} Kibic;

/* ===== GLOBALNE ===== */

extern int K;
extern int miejsca[SEKTORY];
extern int sprzedane;

/* ===== KASY ===== */

extern int czynne_kasy;
extern pthread_mutex_t mutex_kasy;

/* ===== KOLEJKA ===== */

extern Kibic* kolejka[MAX_KOLEJKA];
extern int q_start, q_end, q_size, kibice_w_kolejce;

extern pthread_mutex_t mutex_kolejka;
extern pthread_cond_t cond_kolejka;

/* ===== BILETY ===== */

extern pthread_mutex_t mutex_bilety;

/* ===== WEJŚCIA ===== */

extern sem_t kontrola[SEKTORY][2];   // 2 stanowiska
extern pthread_mutex_t mutex_wejsc;

/* ===== KIEROWNIK ===== */

extern int stop_wejsc;
extern int ewakuacja;
extern pthread_cond_t cond_wejsc;

/* ===== LOG ===== */

void loguj(const char* tekst);

#endif


/* ===== FUNKCJE WĄTKÓW ===== */

void* kasa(void* arg);
void* kibic(void* arg);
void* kierownik(void* arg);
void* techniczny(void* arg);
