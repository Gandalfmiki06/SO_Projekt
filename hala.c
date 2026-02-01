#include "hala.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>

/* Domyślne wartości */
int K = 80;
int miejsca[SEKTORY];
int sprzedane = 0;

/* Definicja licznika sprzedanych biletów na sektor */
int sold_per_sector[SEKTORY];

int osoby_w_sektorze[SEKTORY] = {0};
int stop_sektor[SEKTORY] = {0};

/* SEKTOR VIP */
int miejsca_vip = 0;
int osoby_w_sektorze_vip = 0;
int sprzedane_vip = 0;

int stat_vip = 0, stat_normalni = 0, stat_dzieci = 0, stat_wejsc = 0;
int stat_vip_wejsc = 0;

int urgent_count = 0;

int czynne_kasy = 2;
pthread_mutex_t mutex_kasy = PTHREAD_MUTEX_INITIALIZER;

/* Statyczne kolejki */
Kibic* kolejka[MAX_KOLEJKA];
Kibic* kolejka_vip[MAX_KOLEJKA];
int q_start = 0, q_end = 0, q_size = 0;
int qv_start = 0, qv_end = 0, qv_size = 0;
int kibice_w_kolejce = 0;

/* tablica wskaźników na utworzone struktury kibiców */
Kibic* created_kibice[MAX_KOLEJKA];

pthread_mutex_t mutex_kolejka = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_kolejka = PTHREAD_COND_INITIALIZER;

pthread_mutex_t mutex_bilety = PTHREAD_MUTEX_INITIALIZER;

sem_t kontrola[SEKTORY][2];
int stanowisko_druzyna[SEKTORY][2];
int stanowisko_count[SEKTORY][2];
int waiting_count[SEKTORY];
pthread_mutex_t mutex_wejsc = PTHREAD_MUTEX_INITIALIZER;

int entered_flag[MAX_KOLEJKA];
int adults_entered_in_sector[SEKTORY];

int ewakuacja = 0;
pthread_cond_t cond_wejsc = PTHREAD_COND_INITIALIZER;

volatile sig_atomic_t przerwanie = 0;
int sig_pipe[2] = {-1, -1};

volatile sig_atomic_t remaining_arrivals = 0;

pthread_mutex_t mutex_log = PTHREAD_MUTEX_INITIALIZER;

int max_vip = 0;
int next_kibic_id = 0;

/* Czas rozpoczęcia meczu */
time_t Tp = 0;

/* Bezpieczne logowanie */
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
    if(fd == -1){
        pthread_mutex_unlock(&mutex_log);
        return;
    }
    ssize_t written = 0;
    while(written < len){
        ssize_t w = write(fd, buf + written, (size_t)(len - written));
        if(w <= 0) break;
        written += w;
    }
    close(fd);
    pthread_mutex_unlock(&mutex_log);
}

/* Zapis podsumowania z rozbiciem na sektory + VIP */
void zapisz_podsumowanie()
{
    char buf[512];
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);

    pthread_mutex_lock(&mutex_log);
    int fd = open("raport.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if(fd != -1){
        char header[256];
        int hlen = snprintf(header, sizeof(header),
                            "\n=== PODSUMOWANIE =====\nCzas: %04d-%02d-%02d %02d:%02d:%02d\nSprzedane: %d\nVIP: %d\nNormalni: %d\nDzieci: %d\nWeszli: %d\nSprzedane VIP: %d\n",
                            tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
                            tm.tm_hour, tm.tm_min, tm.tm_sec,
                            sprzedane, stat_vip, stat_normalni, stat_dzieci, stat_wejsc,
                            sprzedane_vip);
        write(fd, header, hlen);

        char line[512];
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
        write(fd, "\n", 1);
        close(fd);
    }
    pthread_mutex_unlock(&mutex_log);

    snprintf(buf, sizeof(buf),
             "Zapisano podsumowanie: Sprzedane=%d VIP=%d Normalni=%d Dzieci=%d Weszli=%d Urgent=%d Sprzedane_VIP=%d Miejsca_VIP=%d",
             sprzedane, stat_vip, stat_normalni, stat_dzieci, stat_wejsc, urgent_count,
             sprzedane_vip, miejsca_vip);
    loguj(buf);
}
