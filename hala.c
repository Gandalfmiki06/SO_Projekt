/* hala.c - definicje globalnych zmiennych i init_simulation */

#include "hala.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>

/* Domyœlne wartoœci */
int K = 80;
int miejsca[SEKTORY];
int sprzedane = 0;
int sold_per_sector[SEKTORY];

/* SEKTORY */
int osoby_w_sektorze[SEKTORY] = {0};
int stop_sektor[SEKTORY] = {0};

/* VIP */
int miejsca_vip = 0;
int osoby_w_sektorze_vip = 0;
int sprzedane_vip = 0;

/* STATYSTYKI */
int stat_vip = 0, stat_normalni = 0, stat_dzieci = 0, stat_wejsc = 0;
int stat_vip_wejsc = 0;

int urgent_count = 0;

/* diagnostyka */
int timeout_count = 0;
int requeue_count = 0;
int timedout_before_dequeue = 0;
int timedout_after_dequeue = 0;
int dequeued_count = 0;
int sold_count = 0;

/* KASY */
int czynne_kasy = 2;
pthread_mutex_t mutex_kasy = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_kasy = PTHREAD_COND_INITIALIZER;

/* KOLEJKI */
Kibic* kolejka[MAX_KOLEJKA];
Kibic* kolejka_vip[MAX_KOLEJKA];
int q_start = 0, q_end = 0, q_size = 0;
int qv_start = 0, qv_end = 0, qv_size = 0;
int kibice_w_kolejce = 0;

Kibic* created_kibice[MAX_KOLEJKA];

pthread_mutex_t mutex_kolejka = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_kolejka = PTHREAD_COND_INITIALIZER;

/* semafor licznikowy elementów w kolejce */
sem_t items_sem;

/* BILETY */
pthread_mutex_t mutex_bilety = PTHREAD_MUTEX_INITIALIZER;

/* WEJŒCIA */
sem_t kontrola[SEKTORY][2];
int stanowisko_druzyna[SEKTORY][2];
int stanowisko_count[SEKTORY][2];
int waiting_count[SEKTORY];
pthread_mutex_t mutex_wejsc = PTHREAD_MUTEX_INITIALIZER;

/* pomocnicze */
int entered_flag[MAX_KOLEJKA];
int adults_entered_in_sector[SEKTORY];

/* KIEROWNIK */
int ewakuacja = 0;
pthread_cond_t cond_wejsc = PTHREAD_COND_INITIALIZER;

/* SYGNA£Y */
volatile sig_atomic_t przerwanie = 0;
int sig_pipe[2] = {-1, -1};

/* REMAINING ARRIVALS */
volatile sig_atomic_t remaining_arrivals = 0;

/* LOG */
pthread_mutex_t mutex_log = PTHREAD_MUTEX_INITIALIZER;

/* brakuj¹ce globalne definicje */
int max_vip = 0;
int next_kibic_id = 0;
time_t Tp = 0;

/* VIP szczegó³owe liczniki */
int vip_reserved = 0;
int vip_sold = 0;
int vip_entered = 0;
pthread_mutex_t mutex_vip = PTHREAD_MUTEX_INITIALIZER;

/* Minimalne logowanie: zapis do pliku raport.txt */
void loguj(const char* tekst)
{
    char buf[1024];
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    char timestr[64];
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &tm);

    int len = snprintf(buf, sizeof(buf), "[%s] [TID=%lu] %s\n",
                       timestr, (unsigned long)pthread_self(), tekst);
    if(len <= 0) return;

    pthread_mutex_lock(&mutex_log);
    int fd = open("raport.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if(fd != -1){
        ssize_t written = 0;
        while(written < len){
            ssize_t w = write(fd, buf + written, (size_t)(len - written));
            if(w <= 0) break;
            written += w;
        }
        close(fd);
    }
    pthread_mutex_unlock(&mutex_log);
}

void zapisz_podsumowanie(void)
{
    char line[512];
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);

    pthread_mutex_lock(&mutex_log);
    int fd = open("raport.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if(fd != -1){
        int hlen = snprintf(line, sizeof(line),
                            "\n=== PODSUMOWANIE =====\nCzas: %04d-%02d-%02d %02d:%02d:%02d\nSprzedane: %d\nVIP reserved: %d\nVIP sold: %d\nVIP entered: %d\nNormalni: %d\nDzieci: %d\nWeszli: %d\nSprzedane VIP: %d\n",
                            tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
                            tm.tm_hour, tm.tm_min, tm.tm_sec,
                            sprzedane, vip_reserved, vip_sold, vip_entered,
                            stat_normalni, stat_dzieci, stat_wejsc,
                            sprzedane_vip);
        write(fd, line, hlen);

        int len = snprintf(line, sizeof(line), "Sprzedane na sektorach: ");
        write(fd, line, len);
        for(int i=0;i<SEKTORY;i++){
            int sprzedane_sektor = sold_per_sector[i];
            if(sprzedane_sektor < 0) sprzedane_sektor = 0;
            int l = snprintf(line, sizeof(line), "%d%s", sprzedane_sektor, (i<SEKTORY-1?"; ":"\n"));
            write(fd, line, l);
        }
        len = snprintf(line, sizeof(line), "Miejsca pozostale na sektory: ");
        write(fd, line, len);
        for(int i=0;i<SEKTORY;i++){
            int l = snprintf(line, sizeof(line), "%d%s", miejsca[i], (i<SEKTORY-1?"; ":"\n"));
            write(fd, line, l);
        }
        int lvip = snprintf(line, sizeof(line),
                            "VIP: miejsca_pozostale=%d sprzedane_vip=%d osoby_w_sektorze_vip=%d\n",
                            miejsca_vip, sprzedane_vip, osoby_w_sektorze_vip);
        write(fd, line, lvip);

        int l2 = snprintf(line, sizeof(line), "VIP wejscia: %d\n", stat_vip_wejsc);
        write(fd, line, l2);
        int l3 = snprintf(line, sizeof(line), "Urgent: %d\n", urgent_count);
        write(fd, line, l3);
        int l4 = snprintf(line, sizeof(line), "Timeouts total: %d (before=%d after=%d) Requeues: %d\n",
                          timeout_count, timedout_before_dequeue, timedout_after_dequeue, requeue_count);
        write(fd, line, l4);

        int ld = snprintf(line, sizeof(line), "Dequeued (pobrano z kolejki): %d\n", dequeued_count);
        write(fd, line, ld);

        int ls = snprintf(line, sizeof(line), "Sold count (udane sprzedaze): %d\n", sold_count);
        write(fd, line, ls);

        int l5 = snprintf(line, sizeof(line), "Sprzedane - Weszli = %d\n", sprzedane - stat_wejsc);
        write(fd, line, l5);

        write(fd, "\n", 1);
        close(fd);
    }
    pthread_mutex_unlock(&mutex_log);
}

/* Inicjalizacja struktur symulacji (widoczna globalnie) */
void init_simulation(int NUM_KIBIC)
{
    (void)NUM_KIBIC;

    miejsca_vip = (int)(0.003 * K);
    if(miejsca_vip < 1) miejsca_vip = 1;
    sprzedane_vip = 0;
    osoby_w_sektorze_vip = 0;
    max_vip = miejsca_vip;
    next_kibic_id = 0;

    int base = K / SEKTORY;
    int rem  = K % SEKTORY;
    for(int i=0;i<SEKTORY;i++){
        miejsca[i] = base + (i < rem ? 1 : 0);
        sold_per_sector[i] = 0;
        for(int j=0;j<2;j++){
            sem_init(&kontrola[i][j], 0, 3);
            stanowisko_druzyna[i][j] = -1;
            stanowisko_count[i][j] = 0;
        }
        waiting_count[i] = 0;
        stop_sektor[i] = 0;
        adults_entered_in_sector[i] = 0;
        osoby_w_sektorze[i] = 0;
    }

    pthread_mutex_lock(&mutex_kolejka);
    for(int i=0;i<MAX_KOLEJKA;i++){
        kolejka[i] = NULL;
        kolejka_vip[i] = NULL;
        created_kibice[i] = NULL;
        entered_flag[i] = 0;
    }
    q_start = q_end = q_size = 0;
    qv_start = qv_end = qv_size = 0;
    kibice_w_kolejce = 0;
    pthread_mutex_unlock(&mutex_kolejka);

    sprzedane = 0;
    stat_vip = 0;
    stat_normalni = 0;
    stat_dzieci = 0;
    stat_wejsc = 0;
    stat_vip_wejsc = 0;
    remaining_arrivals = 0;
    ewakuacja = 0;
    przerwanie = 0;
    urgent_count = 0;
    timeout_count = 0;
    requeue_count = 0;
    timedout_before_dequeue = 0;
    timedout_after_dequeue = 0;
    dequeued_count = 0;
    sold_count = 0;
    vip_reserved = 0;
    vip_sold = 0;
    vip_entered = 0;

    sem_init(&items_sem, 0, 0);
}
