#include "hala.h"
#include <time.h>
#include <pthread.h>

int K = 80;  // przyk³adowa pojemnoœæ
int miejsca[SEKTORY];
int sprzedane = 0;
int czynne_kasy = 2;

/* Mutexy */
pthread_mutex_t mutex_bilety = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_kasy = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_log = PTHREAD_MUTEX_INITIALIZER;

/* Sygnaly */
int stop_wejsc = 0;
int ewakuacja = 0;
pthread_cond_t cond_wejsc = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex_wejsc = PTHREAD_MUTEX_INITIALIZER;

/* Kontrola bezpieczenstwa */
sem_t kontrola[SEKTORY][2];

void loguj(const char* tekst) {
    pthread_mutex_lock(&mutex_log);
    FILE* f = fopen("raport.txt", "a");
    fprintf(f, "[%ld] (%lu) %s\n",
            time(NULL),
            pthread_self(),
            tekst);
    fclose(f);
    pthread_mutex_unlock(&mutex_log);
}
