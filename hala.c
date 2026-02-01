#include "hala.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>

int K = 80;
int miejsca[SEKTORY];
int sprzedane = 0;

int osoby_w_sektorze[SEKTORY]={0};
int stop_sektor[SEKTORY]={0};

int stat_vip=0, stat_normalni=0, stat_dzieci=0, stat_wejsc=0;

int czynne_kasy=2;
pthread_mutex_t mutex_kasy=PTHREAD_MUTEX_INITIALIZER;

Kibic* kolejka[MAX_KOLEJKA];
Kibic* kolejka_vip[MAX_KOLEJKA];
int q_start=0,q_end=0,q_size=0;
int qv_start=0,qv_end=0,qv_size=0;
int kibice_w_kolejce=0;

pthread_mutex_t mutex_kolejka=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_kolejka=PTHREAD_COND_INITIALIZER;

pthread_mutex_t mutex_bilety=PTHREAD_MUTEX_INITIALIZER;

sem_t kontrola[SEKTORY][2];
int stanowisko_druzyna[SEKTORY][2];
pthread_mutex_t mutex_wejsc=PTHREAD_MUTEX_INITIALIZER;

int ewakuacja=0;
pthread_cond_t cond_wejsc=PTHREAD_COND_INITIALIZER;

volatile sig_atomic_t przerwanie=0;

pthread_mutex_t mutex_log=PTHREAD_MUTEX_INITIALIZER;

void loguj(const char* tekst)
{
    pthread_mutex_lock(&mutex_log);
    FILE* f=fopen("raport.txt","a");
    if(f){
        fprintf(f,"[%ld] [TID=%lu] %s\n",time(NULL),(unsigned long)pthread_self(),tekst);
        fclose(f);
    }
    pthread_mutex_unlock(&mutex_log);
}
