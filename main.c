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

static volatile sig_atomic_t got_signal = 0;

static void sig_handler(int sig)
{
    (void)sig;
    got_signal = 1;
    if (SH) SH->przerwanie = 1;
}

/* ---------------------------------------------------------
   TRYBY RÓL (po exec) – bez setpgid
   --------------------------------------------------------- */
static void run_role(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "kier") == 0) {
        shared_attach();

        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sig_handler;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGINT,  &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGQUIT, &sa, NULL);

        int fd = atoi(argv[2]);
        proc_kierownik(fd);
    }

    if (argc >= 2 && strcmp(argv[1], "tech") == 0) {
        shared_attach();

        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sig_handler;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGINT,  &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGQUIT, &sa, NULL);

        int fd = atoi(argv[2]);
        proc_techniczny(fd);
    }

    if (argc >= 2 && strcmp(argv[1], "kasa") == 0) {
        shared_attach();

        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sig_handler;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGINT,  &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGQUIT, &sa, NULL);

        int id = atoi(argv[2]);
        proc_kasa(id);
    }

    if (argc >= 2 && strcmp(argv[1], "kibic") == 0) {
        shared_attach();

        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sig_handler;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGINT,  &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGQUIT, &sa, NULL);

        int id = atoi(argv[2]);
        proc_kibic(id);
    }
}

/* ---------------------------------------------------------
   GŁÓWNY PROGRAM
   --------------------------------------------------------- */
int main(int argc, char **argv)
{
    /* jeśli to rola po exec – wejdź i nie wracaj */
    if (argc >= 2 &&
        (!strcmp(argv[1], "kier") ||
         !strcmp(argv[1], "tech") ||
         !strcmp(argv[1], "kasa") ||
         !strcmp(argv[1], "kibic"))) {
        run_role(argc, argv);
        /* proc_* nie wracają */
        return 0;
    }

    /* tu jesteśmy tylko w procesie głównym */
    setpgid(0, 0);

    int K = 80;
    int NUM_KIBIC = 120;

    if (argc >= 2) {
        int v = atoi(argv[1]);
        if (v > 0) K = v;
    }
    if (argc >= 3) {
        int v = atoi(argv[2]);
        if (v > 0) NUM_KIBIC = v;
    }

    unlink("raport_proc.txt");

    shared_init_master(K);

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
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    /* pipe kierownik → techniczny */
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(1);
    }

    /* kierownik */
    pid_t pid_kier = fork();
    if (pid_kier == 0) {
        char fdstr[16];
        snprintf(fdstr, sizeof(fdstr), "%d", pipefd[1]);
        execlp(argv[0], argv[0], "kier", fdstr, (char*)NULL);
        perror("execlp kier");
        _exit(1);
    }

    /* techniczny */
    pid_t pid_tech = fork();
    if (pid_tech == 0) {
        char fdstr[16];
        snprintf(fdstr, sizeof(fdstr), "%d", pipefd[0]);
        execlp(argv[0], argv[0], "tech", fdstr, (char*)NULL);
        perror("execlp tech");
        _exit(1);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    /* kasy */
    pid_t kasy_pids[MAX_KASY];
    for (int i = 0; i < MAX_KASY; i++) {
        pid_t p = fork();
        if (p == 0) {
            char idstr[16];
            snprintf(idstr, sizeof(idstr), "%d", i);
            execlp(argv[0], argv[0], "kasa", idstr, (char*)NULL);
            perror("execlp kasa");
            _exit(1);
        }
        kasy_pids[i] = p;
    }

    /* kibice */
    srand((unsigned int)(time(NULL) ^ getpid()));
    SH->total_kibice = NUM_KIBIC;

    for (int i = 0; i < NUM_KIBIC; i++) {

        if (SH->przerwanie || got_signal || SH->ewakuacja)
            break;

        int id = SH->next_kibic_id++;
        Kibic *k = &SH->kibice[id];
        memset(k, 0, sizeof(*k));

        k->id = id;
        k->wiek = rand() % 60;
        k->druzyna = rand() % 2;
        k->bilety = 1 + rand() % 2;
        if (k->bilety > 2) k->bilety = 2;
        k->sektor = -1;
        k->has_ticket = 0;
        k->guardian_id = -1;
        k->is_child = (k->wiek < 15);
        k->vip = 0;
        k->pass_count = 0;
        k->entered = 0;

        if (SH->vip_reserved < SH->max_vip) {
            int r = rand() % 1000;
            if (r < (int)(0.3 * 10)) {
                k->vip = 1;
                SH->vip_reserved += k->bilety;
            }
        }

        if (k->is_child && id > 0) {
            for (int j = id - 1; j >= 0; j--) {
                Kibic *cand = &SH->kibice[j];
                if (cand->wiek >= 18) {
                    k->guardian_id = cand->id;
                    break;
                }
            }
        }

        pid_t p = fork();
        if (p == 0) {
            char idstr[16];
            snprintf(idstr, sizeof(idstr), "%d", id);
            execlp(argv[0], argv[0], "kibic", idstr, (char*)NULL);
            perror("execlp kibic");
            _exit(1);
        }

        msleep(1);
    }

    /* klasyczne czekanie na wszystkie dzieci tego procesu */
    int status;
    while (wait(&status) > 0) {
        /* nic – po prostu zbieramy dzieci */
    }

    if (got_signal)
        loguj("Odebrano sygnal, przerwanie=1");

    zapisz_podsumowanie();

    printf("\n\033[1;33m===== PODSUMOWANIE (terminal) =====\033[0m\n");

    printf("\033[1;32mK =\033[0m %d\n", SH->K);
    printf("\033[1;36mSprzedane:\033[0m %d\n", SH->sprzedane);

    printf("\033[1;32mVIP reserved:\033[0m %d\n", SH->vip_reserved);
    printf("\033[1;32mVIP sold:\033[0m %d\n", SH->vip_sold);
    printf("\033[1;32mVIP entered:\033[0m %d\n", SH->vip_entered);

    printf("\033[1;32mNormalni:\033[0m %d\n", SH->stat_normalni);
    printf("\033[1;32mDzieci:\033[0m %d\n", SH->stat_dzieci);
    printf("\033[1;36mWeszli:\033[0m %d\n", SH->stat_wejsc);
    printf("\033[1;36mVIP wejscia:\033[0m %d\n", SH->stat_vip_wejsc);

    printf("\033[1;34mSprzedane na sektorach:\033[0m ");
    for (int s = 0; s < SEKTORY; s++) {
        printf("%d%s", SH->sold_per_sector[s], (s < SEKTORY - 1 ? "; " : "\n"));
    }

    printf("\033[1;34mMiejsca pozostale na sektory:\033[0m ");
    for (int s = 0; s < SEKTORY; s++) {
        printf("%d%s", SH->miejsca[s], (s < SEKTORY - 1 ? "; " : "\n"));
    }

    printf("\n");

    shared_cleanup();

    printf("Symulacja zakonczona. Raport w pliku raport_proc.txt\n");

    return 0;
}
