#ifndef HALA_H
#define HALA_H

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>

#define SEKTORY 8
#define MAX_KOLEJKA 2000
#define MAX_KASY 10

typedef struct {
    int id;
    int vip;
    int wiek;
    int druzyna;
    int bilety;           /* pierwotne zamówienie (informacyjne) */
    int to_enter;         /* ile osób faktycznie ma wejść (ustawiane przez kasę) */
    int sektor;           /* -1: sektor wybierze kasa; -2: anulowany; -10: sektor VIP */
    int guardian_id;
    int guardian_for;
    int pass_count;
    int waiting_for_control;
    int urgent;
    int in_queue;
    int processing;
    int requeue_attempts; /* ile razy już requeue'owano tego kibica */
} Kibic;

/* ===== GLOBALNE ===== */
extern int K;
extern int miejsca[SEKTORY];
extern int sprzedane;

/* ===== SEKTORY ===== */
extern int osoby_w_sektorze[SEKTORY];
extern int stop_sektor[SEKTORY];

/* ===== SEKTOR VIP ===== */
extern int miejsca_vip;
extern int osoby_w_sektorze_vip;
extern int sprzedane_vip;

/* ===== STATYSTYKI ===== */
extern int stat_vip;
extern int stat_normalni;
extern int stat_dzieci;
extern int stat_wejsc;
extern int stat_vip_wejsc;
extern int urgent_count;

/* dodatkowe liczniki diagnostyczne */
extern int timeout_count;
extern int requeue_count;

/* ===== KASY ===== */
extern int czynne_kasy;
extern pthread_mutex_t mutex_kasy;

/* ===== KOLEJKI (statyczne) ===== */
extern Kibic* kolejka[MAX_KOLEJKA];
extern Kibic* kolejka_vip[MAX_KOLEJKA];
extern int q_start;
extern int q_end;
extern int q_size;
extern int qv_start;
extern int qv_end;
extern int qv_size;
extern int kibice_w_kolejce;

/* tablica wskaźników na wszystkie utworzone struktury Kibic */
extern Kibic* created_kibice[MAX_KOLEJKA];

extern pthread_mutex_t mutex_kolejka;
extern pthread_cond_t cond_kolejka;

/* ===== BILETY ===== */
extern pthread_mutex_t mutex_bilety;

/* ===== WEJŚCIA ===== */
extern sem_t kontrola[SEKTORY][2];
extern int stanowisko_druzyna[SEKTORY][2];
extern int stanowisko_count[SEKTORY][2];
extern int waiting_count[SEKTORY];
extern pthread_mutex_t mutex_wejsc;

/* pomocnicze struktury */
extern int entered_flag[MAX_KOLEJKA];
extern int adults_entered_in_sector[SEKTORY];

/* ===== KIEROWNIK ===== */
extern int ewakuacja;
extern pthread_cond_t cond_wejsc;

/* ===== SYGNAŁY ===== */
extern volatile sig_atomic_t przerwanie;
extern int sig_pipe[2];

/* ===== REMAINING ARRIVALS ===== */
extern volatile sig_atomic_t remaining_arrivals;

/* ===== LOG i RAPORT ===== */
void loguj(const char* tekst);
void zapisz_podsumowanie(void);

/* ===== INNE ===== */
extern int max_vip;
extern int next_kibic_id;

/* ===== CZAS MECZU ===== */
extern time_t Tp;

/* ===== WĄTKI ===== */
void* kasa(void* arg);
void* kibic(void* arg);
void* kierownik(void* arg);
void* techniczny(void* arg);

/* ===== DODANE: sprzedane na sektorach ===== */
extern int sold_per_sector[SEKTORY];

#endif
