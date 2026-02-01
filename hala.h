#ifndef HALA_H
#define HALA_H

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

#define SEKTORY 8
#define MAX_KOLEJKA 300
#define MAX_KASY 10

typedef struct {
    int id, vip, wiek, druzyna, bilety, sektor;
} Kibic;

/* ===== GLOBALNE ===== */
extern int K;
extern int miejsca[SEKTORY];
extern int sprzedane;

/* ===== SEKTORY ===== */
extern int osoby_w_sektorze[SEKTORY];
extern int stop_sektor[SEKTORY];

/* ===== STATYSTYKI ===== */
extern int stat_vip, stat_normalni, stat_dzieci, stat_wejsc;

/* ===== KASY ===== */
extern int czynne_kasy;
extern pthread_mutex_t mutex_kasy;

/* ===== KOLEJKI ===== */
extern Kibic* kolejka[MAX_KOLEJKA];
extern Kibic* kolejka_vip[MAX_KOLEJKA];
extern int q_start,q_end,q_size;
extern int qv_start,qv_end,qv_size;
extern int kibice_w_kolejce;

extern pthread_mutex_t mutex_kolejka;
extern pthread_cond_t cond_kolejka;

/* ===== BILETY ===== */
extern pthread_mutex_t mutex_bilety;

/* ===== WEJŚCIA ===== */
extern sem_t kontrola[SEKTORY][2];
extern int stanowisko_druzyna[SEKTORY][2];
extern pthread_mutex_t mutex_wejsc;

/* ===== KIEROWNIK ===== */
extern int ewakuacja;
extern pthread_cond_t cond_wejsc;

/* ===== SYGNAŁY ===== */
extern volatile sig_atomic_t przerwanie;

/* ===== LOG ===== */
void loguj(const char* tekst);

/* ===== WĄTKI ===== */
void* kasa(void* arg);
void* kibic(void* arg);
void* kierownik(void* arg);
void* techniczny(void* arg);

#endif
