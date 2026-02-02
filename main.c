/* main.c - uruchomienie symulacji i testów */

#include "hala.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>

/* Deklaracje wątków (implementacje w innych plikach) */
extern void* kasa(void* arg);
extern void* kibic(void* arg);
extern void* kierownik(void* arg);
extern void* techniczny(void* arg);
extern void menu_testow(void);
extern void init_simulation(int NUM_KIBIC);
extern void install_signal_handlers(void);
extern pthread_t start_signal_monitor(void);

int main(int argc, char** argv)
{
    int tryb = 1;
    printf("Wybierz tryb:\n1 - Symulacja\n2 - Testy\nWybor (domyslnie 1): ");
    fflush(stdout);

    char buf[64];
    if(fgets(buf, sizeof(buf), stdin) != NULL){
        char *endptr = NULL;
        long v = strtol(buf, &endptr, 10);
        if(endptr != buf && v >= 1 && v <= 2) tryb = (int)v;
    }

    int NUM_KIBIC = 120;
    if(argc >= 2){
        int newK = atoi(argv[1]);
        if(newK > 0) K = newK;
    }
    if(argc >= 3){
        int nk = atoi(argv[2]);
        if(nk > 0) NUM_KIBIC = nk;
    }

    if(tryb == 2){
        menu_testow();
        return 0;
    }

    Tp = time(NULL) + 10;
    {
        char tbuf[128];
        struct tm tm;
        localtime_r(&Tp, &tm);
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm);
        char msg[256];
        snprintf(msg, sizeof(msg), "Mecz rozpocznie sie o Tp=%s", tbuf);
        loguj(msg);
    }

    init_simulation(NUM_KIBIC);
    remaining_arrivals = NUM_KIBIC;

    install_signal_handlers();
    pthread_t t_sig = start_signal_monitor();

    pthread_t t_kier = 0, t_tech = 0;
    if(pthread_create(&t_kier, NULL, kierownik, NULL) != 0){
        przerwanie = 1;
    }
    if(pthread_create(&t_tech, NULL, techniczny, NULL) != 0){
        przerwanie = 1;
    }

    pthread_t kasy[MAX_KASY];
    for(int i=0;i<MAX_KASY;i++){
        if(pthread_create(&kasy[i], NULL, kasa, (void*)(size_t)i) != 0){
            przerwanie = 1;
        }
    }

    pthread_t *kibice = malloc(sizeof(pthread_t) * NUM_KIBIC);
    if(!kibice) przerwanie = 1;

    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());

    /* Tworzenie kibiców: dzieci dostają guardian_id (losowy dorosły jeśli dostępny) */
    for(int i=0;i<NUM_KIBIC && !przerwanie;i++){
        Kibic* k = malloc(sizeof(Kibic));
        if(!k){
            __sync_fetch_and_sub(&remaining_arrivals, 1);
            continue;
        }
        memset(k, 0, sizeof(Kibic));
        k->id = next_kibic_id++;
        k->vip = 0;
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
        k->requeue_attempts = 0;

        /* jeśli dziecko (<15) - przypisz guardiana (najprościej: ostatnio utworzony dorosły) */
        if(k->wiek < 15){
            pthread_mutex_lock(&mutex_kolejka);
            for(int j = next_kibic_id-2; j >= 0; --j){
                if(j >= 0 && j < MAX_KOLEJKA && created_kibice[j]){
                    if(created_kibice[j]->wiek >= 18){
                        k->guardian_id = created_kibice[j]->id;
                        created_kibice[j]->guardian_for = k->id;
                        break;
                    }
                }
            }
            pthread_mutex_unlock(&mutex_kolejka);
        }

        /* próbuj zarezerwować VIP pod mutexem (rezerwujemy bilety) */
        if (miejsca_vip > 0) {
            int tryv = rand() % 1000;
            if (tryv < (int)(0.003 * 1000)) { /* ~0.3% * 1000 scaled */
                pthread_mutex_lock(&mutex_vip);
                if (vip_reserved + k->bilety <= max_vip) {
                    vip_reserved += k->bilety;
                    k->vip = 1;
                } else {
                    k->vip = 0;
                }
                pthread_mutex_unlock(&mutex_vip);
            }
        }

        pthread_mutex_lock(&mutex_kolejka);
        if(k->id >= 0 && k->id < MAX_KOLEJKA) created_kibice[k->id] = k;
        pthread_mutex_unlock(&mutex_kolejka);

        if(pthread_create(&kibice[i], NULL, kibic, k) != 0){
            pthread_mutex_lock(&mutex_kolejka);
            if(k->id >= 0 && k->id < MAX_KOLEJKA) created_kibice[k->id] = NULL;
            pthread_mutex_unlock(&mutex_kolejka);
            free(k);
            __sync_fetch_and_sub(&remaining_arrivals, 1);
            continue;
        }

        usleep(20000);
    }

    if(kibice){
        for(int i=0;i<NUM_KIBIC;i++){
            pthread_join(kibice[i], NULL);
        }
    }

    pthread_cond_broadcast(&cond_kolejka);
    pthread_cond_broadcast(&cond_wejsc);
    for(int i=0;i<MAX_KASY;i++) sem_post(&items_sem);

    for(int i=0;i<MAX_KASY;i++){
        pthread_join(kasy[i], NULL);
    }

    pthread_cond_broadcast(&cond_wejsc);
    pthread_cond_broadcast(&cond_kolejka);

    if(t_kier) pthread_join(t_kier, NULL);
    if(t_tech) pthread_join(t_tech, NULL);

    if(t_sig) pthread_join(t_sig, NULL);

    printf("\n===== PODSUMOWANIE =====\n");
    printf("Sprzedane: %d\nVIP reserved: %d\nVIP sold: %d\nVIP entered: %d\nNormalni: %d\nDzieci: %d\nWeszli: %d\n",
           sprzedane, vip_reserved, vip_sold, vip_entered, stat_normalni, stat_dzieci, stat_wejsc);

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

    printf("VIP: miejsca_pozostale=%d sprzedane_vip=%d osoby_w_sektorze_vip=%d\n",
           miejsca_vip, sprzedane_vip, osoby_w_sektorze_vip);

    zapisz_podsumowanie();
    loguj("Koniec symulacji");

    if(kibice) free(kibice);
    return 0;
}
