#define _XOPEN_SOURCE 700
#include <unistd.h>

#include "shared.h"
#include "log.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

/*
    ============================================================
    KIEROWNIK — WERSJA Z OBSŁUGĄ SEKTORA VIP (A1‑V1)
    ============================================================

    Zmiany:

    ✔ kierownik NIE wysyła sygnałów 1/2 do sektora VIP (8)
    ✔ sektor VIP nie podlega kontroli wejścia
    ✔ sektor VIP nie jest zatrzymywany ani wznawiany
    ✔ ewakuacja obejmuje wszystkie sektory 0–8
*/

static void sig_handler(int sig)
{
    (void)sig;
    if (SH) SH->przerwanie = 1;
}

/*
   Komunikaty do technicznego:
   "1 s\n" - sygnal1, zatrzymanie wejścia do sektora s (0–7)
   "2 s\n" - sygnal2, wznowienie wejścia do sektora s (0–7)
   "3 -1\n" - sygnal3, ewakuacja (0–8)
*/
static void send_cmd(int fd, int cmd, int sektor)
{
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "%d %d\n", cmd, sektor);
    if (write(fd, buf, n) < 0) {
        perror("write cmd");
    }
}

void proc_kierownik(int write_fd)
{
    /* instalacja handlera sygnałów */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    loguj("Kierownik: start");

    unsigned int seed = (unsigned int)time(NULL);

    while (!SH->przerwanie) {

        /* --- Sterowanie liczbą kas --- */
        pthread_mutex_lock(&SH->mutex_kolejka);
        int q = SH->q_size;
        pthread_mutex_unlock(&SH->mutex_kolejka);

        int baza = SH->K / 10;
        if (baza <= 0) baza = 1;

        pthread_mutex_lock(&SH->mutex_global);
        int aktualne = SH->czynne_kasy;

        int wymagane = (q + baza - 1) / baza;
        if (wymagane < 2) wymagane = 2;
        if (wymagane > MAX_KASY) wymagane = MAX_KASY;

        int nowe = aktualne;

        if (aktualne > 2 && q < baza * (aktualne - 1)) {
            nowe = aktualne - 1;
        }
        else if (wymagane > aktualne) {
            nowe = wymagane;
        }

        if (nowe != SH->czynne_kasy) {
            loguj("Kierownik: zmiana liczby kas z %d na %d (kolejka=%d, baza=%d)",
                  SH->czynne_kasy, nowe, q, baza);
            SH->czynne_kasy = nowe;
        }
        pthread_mutex_unlock(&SH->mutex_global);

        /* --- Losowe zatrzymanie/wznowienie wejść do sektorów 0–7 --- */
        if (rand_r(&seed) % 20 == 0) {

            int s = rand_r(&seed) % 8; /* tylko sektory 0–7 */
            int akcja = rand_r(&seed) % 2; /* 0 = stop, 1 = start */

            send_cmd(write_fd, (akcja == 0 ? 1 : 2), s);
            loguj("Kierownik: sygnal%d dla sektora %d (przez pipe)",
                  (akcja == 0 ? 1 : 2), s);
        }

        /* --- Start meczu --- */
        time_t now = time(NULL);

        if (!SH->mecz_started && now >= SH->Tp) {
            SH->mecz_started = 1;
            loguj("Kierownik: mecz rozpoczal sie (Tp osiągnięte)");
        }

        /* --- Ewakuacja po 60 sekundach meczu --- */
        if (SH->mecz_started && !SH->ewakuacja && now >= SH->Tp + 60) {
            send_cmd(write_fd, 3, -1);
            loguj("Kierownik: sygnal3 - ewakuacja (przez pipe)");
        }

        /* --- Zakończenie pracy gdy wszyscy kibice obsłużeni --- */
        pthread_mutex_lock(&SH->mutex_global);
        int done  = SH->finished_kibice;
        int total = SH->total_kibice;
        pthread_mutex_unlock(&SH->mutex_global);

        if (total > 0 && done >= total) {
            loguj("Kierownik: wszyscy kibice obsluzeni, koniec");
            break;
        }

        sleep(1);
    }

    loguj("Kierownik: koniec");
    _exit(0);
}
