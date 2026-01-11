#ifndef HALA_H
#define HALA_H

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>

#define SEKTORY 8
#define MAX_KASY 10

extern int K;
extern int miejsca[SEKTORY];
extern int sprzedane;
extern int czynne_kasy;

/* Mutexy */
extern pthread_mutex_t mutex_bilety;
extern pthread_mutex_t mutex_kasy;
extern pthread_mutex_t mutex_log;

/* Sygna³y kierownika */
extern int stop_wejsc;
extern int ewakuacja;
extern pthread_cond_t cond_wejsc;
extern pthread_mutex_t mutex_wejsc;

/* Semafory kontroli */
extern sem_t kontrola[SEKTORY][2];

/* Funkcja loguj¹ca */
void loguj(const char* tekst);

#endif
#pragma once
