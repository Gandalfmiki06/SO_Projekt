#include "hala.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>

/* ===== PARAMETRY ===== */

int K = 80;
int miejsca[SEKTORY];
int sprzedane = 0;

/* ===== KASY ===== */

int czynne_kasy = 2;
pthread_mutex_t mutex_kasy = PTHREAD_MUTEX_INITIALIZER;

/* ===== KOLEJKA ===== */

Kibic* kolejka[MAX_KOLEJKA];
int q_start = 0, q_end = 0, q_size = 0, kibice_w_kolejce = 0;

pthread_mutex_t mutex_kolejka = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_kolejka = PTHREAD_COND_INITIALIZER;

/* ===== BILETY ===== */

pthread_mutex_t mutex_bilety = PTHREAD_MUTEX_INITIALIZER;

/* ===== WEJŚCIA ===== */

sem_t kontrola[SEKTORY][2];
pthread_mutex_t mutex_wejsc = PTHREAD_MUTEX_INITIALIZER;

/* ===== KIEROWNIK ===== */

int stop_wejsc = 0;
int ewakuacja = 0;
pthread_cond_t cond_wejsc = PTHREAD_COND_INITIALIZER;

/* ===== LOG ===== */

pthread_mutex_t mutex_log = PTHREAD_MUTEX_INITIALIZER;

void loguj(const char* tekst)
{
    pthread_mutex_lock(&mutex_log);
    FILE* f = fopen("raport.txt", "a");
    if (f) {
        fprintf(f,
            "[%ld] [PID=%d] [TID=%lu] %s\n",
            time(NULL),
            getpid(),
            (unsigned long)pthread_self(),
            tekst);
        fclose(f);
    }
    pthread_mutex_unlock(&mutex_log);
}
