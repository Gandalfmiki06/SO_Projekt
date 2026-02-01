#include "hala.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

extern void* kasa(void* arg);
extern void* kibic(void* arg);
extern void* kierownik(void* arg);
extern void* techniczny(void* arg);
extern void menu_testow(void);

static void* debug_thread(void* arg)
{
    (void)arg;
    char buf[256];
    while(!przerwanie && !ewakuacja){
        pthread_mutex_lock(&mutex_kolejka);
        int q = q_size;
        int qv = qv_size;
        int kw = kibice_w_kolejce;
        pthread_mutex_unlock(&mutex_kolejka);

        pthread_mutex_lock(&mutex_bilety);
        int sold = sprzedane;
        pthread_mutex_unlock(&mutex_bilety);

        int waiting_total = 0;
        pthread_mutex_lock(&mutex_wejsc);
        for(int s=0;s<SEKTORY;s++) waiting_total += waiting_count[s];
        pthread_mutex_unlock(&mutex_wejsc);

        snprintf(buf, sizeof(buf), "DEBUG: q=%d qv=%d kibice_w_kolejce=%d sold=%d waiting_total=%d czynne_kasy=%d",
                 q, qv, kw, sold, waiting_total, czynne_kasy);
        loguj(buf);
        sleep(5);
    }
    return NULL;
}

static void signal_handler(int sig)
{
    (void)sig;
    przerwanie = 1;
    ewakuacja = 1;
    if(sig_pipe[1] != -1){
        const char c = 'x';
        write(sig_pipe[1], &c, 1);
    }
}

static void* sig_monitor(void* arg)
{
    (void)arg;
    char buf;
    while(1){
        ssize_t r = read(sig_pipe[0], &buf, 1);
        if(r <= 0){
            if(errno == EINTR) continue;
            break;
        }
        loguj("SYGNAL: odebrano powiadomienie w monitorze sygnalow");
        pthread_cond_broadcast(&cond_wejsc);
        pthread_cond_broadcast(&cond_kolejka);

        for(int i=0;i<5;i++){
            sleep(1);
            if(przerwanie==0) break;
        }
        if(przerwanie){
            loguj("SIG monitor: wymuszone zakonczenie po timeout");
            kill(getpid(), SIGKILL);
        }
    }
    return NULL;
}

static void init_simulation(int NUM_KIBIC)
{
    (void)NUM_KIBIC;

    /* maksymalna liczba VIP – powiązana z pojemnością sektora VIP */
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
}

/* --- main --- */
int main(int argc, char** argv)
{
    int tryb = 1;
    printf("1 - Symulacja\n2 - Testy jednostkowe\nWybor: ");
    if(scanf("%d",&tryb) != 1) return 0;

    int NUM_KIBIC = 120;
    if(argc >= 2){
        int newK = atoi(argv[1]);
        if(newK > 0) K = newK;
    }
    if(argc >= 3){
        int nk = atoi(argv[2]);
        if(nk > 0) NUM_KIBIC = nk;
    }

    /* ustawienie czasu rozpoczęcia meczu Tp (np. za 10 sekund od teraz) */
    Tp = time(NULL) + 10;
    {
        char buf[128];
        struct tm tm;
        localtime_r(&Tp, &tm);
        char timestr[64];
        strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &tm);
        snprintf(buf, sizeof(buf), "Mecz rozpocznie sie o Tp=%s", timestr);
        loguj(buf);
    }

    init_simulation(NUM_KIBIC);

    if(tryb == 2){
        menu_testow();
        return 0;
    }

    remaining_arrivals = NUM_KIBIC;

    if(pipe(sig_pipe) == -1){
        perror("pipe");
        return 1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    pthread_t t_sig;
    if(pthread_create(&t_sig, NULL, sig_monitor, NULL) != 0){
        perror("pthread_create sig_monitor");
        close(sig_pipe[0]);
        close(sig_pipe[1]);
        return 1;
    }

    pthread_t t_kier, t_tech;
    if(pthread_create(&t_kier, NULL, kierownik, NULL) != 0){
        perror("pthread_create kierownik");
        przerwanie = 1;
    }
    if(pthread_create(&t_tech, NULL, techniczny, NULL) != 0){
        perror("pthread_create techniczny");
        przerwanie = 1;
    }

    pthread_t kasy[MAX_KASY];
    for(int i=0;i<MAX_KASY;i++){
        if(pthread_create(&kasy[i], NULL, kasa, (void*)(size_t)i) != 0){
            char buf[128];
            snprintf(buf, sizeof(buf), "main: nie udalo sie utworzyc watku kasy %d", i);
            loguj(buf);
            przerwanie = 1;
        }
    }

    pthread_t t_debug;
    if(pthread_create(&t_debug, NULL, debug_thread, NULL) != 0){
        loguj("main: nie udalo sie utworzyc watku debug");
    }

    pthread_t *kibice = malloc(sizeof(pthread_t) * NUM_KIBIC);
    if(!kibice){
        fprintf(stderr, "Brak pamieci na tablice watkow kibicow\n");
        przerwanie = 1;
    }

    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());

    for(int i=0;i<NUM_KIBIC && !przerwanie;i++){
        Kibic* k = malloc(sizeof(Kibic));
        if(!k){
            fprintf(stderr,"Brak pamieci na strukture Kibic\n");
            __sync_fetch_and_sub(&remaining_arrivals, 1);
            continue;
        }
        memset(k, 0, sizeof(Kibic));
        k->id = next_kibic_id++;
        k->vip = 0;
        if(max_vip > 0){
            if((rand()%1000) < 3){
                int cur = __sync_fetch_and_add(&stat_vip, 1);
                if(cur < max_vip){
                    k->vip = 1;
                } else {
                    __sync_fetch_and_sub(&stat_vip, 1);
                    k->vip = 0;
                }
            }
        }
        k->wiek = rand()%60;
        k->druzyna = rand()%2;
        k->bilety = 1 + rand()%2;
        if(k->bilety > 2) k->bilety = 2;
        k->sektor = -1;
        k->to_enter = 0;
        k->guardian_id = -1;
        k->guardian_for = 0;
        k->pass_count = 0;
        k->waiting_for_control = 0;
        k->urgent = 0;
        k->in_queue = 0;
        k->processing = 0;

        pthread_mutex_lock(&mutex_kolejka);
        if(k->id >= 0 && k->id < MAX_KOLEJKA) created_kibice[k->id] = k;
        pthread_mutex_unlock(&mutex_kolejka);

        if(pthread_create(&kibice[i], NULL, kibic, k) != 0){
            pthread_mutex_lock(&mutex_kolejka);
            if(k->id >= 0 && k->id < MAX_KOLEJKA) created_kibice[k->id] = NULL;
            pthread_mutex_unlock(&mutex_kolejka);

            free(k);
            __sync_fetch_and_sub(&remaining_arrivals, 1);
            char buf[128];
            snprintf(buf, sizeof(buf), "main: nie udalo sie utworzyc watku kibica %d", i);
            loguj(buf);
            continue;
        }

        usleep(20000);
    }

    if(kibice){
        for(int i=0;i<NUM_KIBIC;i++){
            pthread_join(kibice[i], NULL);
        }
    }

    for(int i=0;i<MAX_KASY;i++){
        pthread_join(kasy[i], NULL);
    }

    pthread_cond_broadcast(&cond_wejsc);
    pthread_cond_broadcast(&cond_kolejka);

    pthread_join(t_kier, NULL);
    pthread_join(t_tech, NULL);

    pthread_cancel(t_debug);
    pthread_join(t_debug, NULL);

    printf("\n===== PODSUMOWANIE =====\n");
    printf("Sprzedane: %d\nVIP: %d\nNormalni: %d\nDzieci: %d\nWeszli: %d\n",
           sprzedane, stat_vip, stat_normalni, stat_dzieci, stat_wejsc);

    printf("Sprzedane na sektorach: ");
    for(int s=0; s<SEKTORY; s++){
        if(s) printf("; ");
        printf("%d", sold_per_sector[s]);
    }
    printf("\n");

    printf("Miejsca pozostale na sektory: ");
    for(int s=0; s<SEKTORY; s++){
        if(s) printf("; ");
        printf("%d", miejsca[s]);
    }
    printf("\n");

    printf("SEKTOR VIP: miejsca_pozostale=%d sprzedane_vip=%d osoby_w_sektorze_vip=%d\n",
           miejsca_vip, sprzedane_vip, osoby_w_sektorze_vip);

    zapisz_podsumowanie();
    loguj("Koniec symulacji");

    close(sig_pipe[0]);
    close(sig_pipe[1]);
    pthread_join(t_sig, NULL);

    if(kibice) free(kibice);
    return 0;
}
