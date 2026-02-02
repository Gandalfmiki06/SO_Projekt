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
    int vip;            /* 1 jeœli VIP (rezerwacja biletu VIP) */
    int wiek;
    int druzyna;
    int bilety;         /* liczba biletów (1 lub 2) */
    int to_enter;       /* ile osób ma wejœæ (ustawiane po sprzeda¿y) */
    int sektor;         /* -1: oczekuje; -2: anulowany; >=0: sektor */
    int guardian_id;    /* id opiekuna jeœli dziecko */
    int guardian_for;   /* id dziecka jeœli opiekun */
    int pass_count;     /* ile osób przepuœci³ */
    int waiting_for_control;
    int urgent;
    int in_queue;
    int processing;
    int requeue_attempts;
} Kibic;

/* GLOBALNE */
extern int K;
extern int miejsca[SEKTORY];
extern int sprzedane;
extern int sold_per_sector[SEKTORY];

/* SEKTORY */
extern int osoby_w_sektorze[SEKTORY];
extern int stop_sektor[SEKTORY];

/* VIP */
extern int miejsca_vip;
extern int osoby_w_sektorze_vip;
extern int sprzedane_vip;

/* STATYSTYKI */
extern int stat_vip;
extern int stat_normalni;
extern int stat_dzieci;
extern int stat_wejsc;
extern int stat_vip_wejsc;
extern int urgent_count;

/* diagnostyka */
extern int timeout_count;
extern int requeue_count;
extern int timedout_before_dequeue;
extern int timedout_after_dequeue;
extern int dequeued_count;
extern int sold_count;

/* KASY */
extern int czynne_kasy;
extern pthread_mutex_t mutex_kasy;
extern pthread_cond_t cond_kasy;

/* KOLEJKI */
extern Kibic* kolejka[MAX_KOLEJKA];
extern Kibic* kolejka_vip[MAX_KOLEJKA];
extern int q_start;
extern int q_end;
extern int q_size;
extern int qv_start;
extern int qv_end;
extern int qv_size;
extern int kibice_w_kolejce;

extern Kibic* created_kibice[MAX_KOLEJKA];

extern pthread_mutex_t mutex_kolejka;
extern pthread_cond_t cond_kolejka;

/* semafor licznikowy elementów w kolejce */
extern sem_t items_sem;

/* BILETY */
extern pthread_mutex_t mutex_bilety;

/* WEJŒCIA */
extern sem_t kontrola[SEKTORY][2];
extern int stanowisko_druzyna[SEKTORY][2];
extern int stanowisko_count[SEKTORY][2];
extern int waiting_count[SEKTORY];
extern pthread_mutex_t mutex_wejsc;

/* pomocnicze */
extern int entered_flag[MAX_KOLEJKA];
extern int adults_entered_in_sector[SEKTORY];

/* KIEROWNIK */
extern int ewakuacja;
extern pthread_cond_t cond_wejsc;

/* SYGNA£Y */
extern volatile sig_atomic_t przerwanie;
extern int sig_pipe[2];

/* REMAINING ARRIVALS */
extern volatile sig_atomic_t remaining_arrivals;

/* LOG i RAPORT */
void loguj(const char* tekst);
void zapisz_podsumowanie(void);

/* INNE */
extern int max_vip;
extern int next_kibic_id;

/* VIP szczegó³owe liczniki (wszystko w jednostce biletu) */
extern int vip_reserved;
extern int vip_sold;
extern int vip_entered;
extern pthread_mutex_t mutex_vip;

/* CZAS MECZU */
extern time_t Tp;

/* W¥TKI */
void* kasa(void* arg);
void* kibic(void* arg);
void* kierownik(void* arg);
void* techniczny(void* arg);

/* inicjalizacja (widoczna globalnie) */
void init_simulation(int NUM_KIBIC);

/* sygna³y helpers */
void install_signal_handlers(void);
pthread_t start_signal_monitor(void);

#endif /* HALA_H */
