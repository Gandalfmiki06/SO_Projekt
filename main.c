#define _XOPEN_SOURCE 700
#include <unistd.h>

#include "shared.h"
#include "log.h"
#include "kasa.h"
#include "kibic.h"
#include "kierownik.h"
#include "techniczny.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

static void sig_handler(int sig)
{
    (void)sig;
    if(SH) SH->przerwanie = 1;
    loguj("Odebrano sygnal, przerwanie=1");
}

int main(int argc, char **argv)
{
    int K = 80;
    int NUM_KIBIC = 120;

    if(argc >= 2){
        int v = atoi(argv[1]);
        if(v > 0) K = v;
    }
    if(argc >= 3){
        int v = atoi(argv[2]);
        if(v > 0) NUM_KIBIC = v;
    }

    unlink("raport_proc.txt");

    shared_init(K);

    SH->Tp = time(NULL) + 10;
    {
        char tbuf[128];
        struct tm tm;
        localtime_r(&SH->Tp, &tm);
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm);
        loguj("Mecz rozpocznie sie o Tp=%s", tbuf);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    pid_t pid_kier = fork();
    if(pid_kier == 0){
        proc_kierownik();
    }

    pid_t pid_tech = fork();
    if(pid_tech == 0){
        proc_techniczny();
    }

    pid_t kasy_pids[MAX_KASY];
    for(int i=0;i<MAX_KASY;i++){
        pid_t p = fork();
        if(p == 0){
            proc_kasa(i);
        }
        kasy_pids[i] = p;
    }

    srand((unsigned int)(time(NULL) ^ getpid()));
    SH->total_kibice = NUM_KIBIC;

    for(int i=0;i<NUM_KIBIC;i++){
        int id = SH->next_kibic_id++;
        Kibic *k = &SH->kibice[id];
        memset(k, 0, sizeof(*k));
        k->id = id;
        k->wiek = rand()%60;
        k->druzyna = rand()%2;
        k->bilety = 1 + rand()%2;
        if(k->bilety > 2) k->bilety = 2;
        k->sektor = -1;
        k->has_ticket = 0;
        k->guardian_id = -1;
        k->is_child = (k->wiek < 15);
        k->vip = 0;
        k->pass_count = 0;
        k->entered = 0;

        if(SH->vip_reserved < SH->max_vip){
            int r = rand()%1000;
            if(r < (int)(0.3 * 10)){
                k->vip = 1;
                SH->vip_reserved += k->bilety;
            }
        }

        if(k->is_child && id > 0){
            for(int j=id-1;j>=0;j--){
                Kibic *cand = &SH->kibice[j];
                if(cand->wiek >= 18){
                    k->guardian_id = cand->id;
                    break;
                }
            }
        }

        pid_t p = fork();
        if(p == 0){
            proc_kibic(id);
        }

        msleep(20 + (rand()%50));
    }

    int status;
    while(wait(&status) > 0){}

    zapisz_podsumowanie();
    shared_cleanup();

    printf("Symulacja zakonczona. Raport w pliku raport_proc.txt\n");
    return 0;
}
