#define _XOPEN_SOURCE 700
#include <unistd.h>

#include "shared.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

/*
    ============================================================
    PRACOWNIK TECHNICZNY — WERSJA POPRAWIONA
    ============================================================

    Najważniejsze zmiany:

    ✔ Reaguje natychmiast na SH->przerwanie i SH->ewakuacja  
    ✔ Kończy się natychmiast po sygnale 3 (ewakuacja)  
    ✔ WYSYŁA kill(0, SIGTERM) po ewakuacji — to budzi kasy/kibiców  
    ✔ Nie blokuje się na fdopen/fscanf po sygnale  
    ✔ Nie pozostawia zombie  
    ✔ Nie blokuje procesu głównego w wait()  
*/

static void sig_handler(int sig)
{
    (void)sig;
    if (SH) SH->przerwanie = 1;
}

/* ---------------------------------------------------------
   GŁÓWNA FUNKCJA TECHNICZNEGO
   --------------------------------------------------------- */
void proc_techniczny(int read_fd)
{
    /* instalacja handlera sygnałów */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    loguj("Techniczny: start");

    /* otwarcie pipe do kierownika */
    FILE *f = fdopen(read_fd, "r");
    if (!f) {
        perror("fdopen techniczny");
        _exit(1);
    }

    int cmd, sektor;

    /*
        ---------------------------------------------------------
        Pętla odbierająca sygnały od kierownika
        ---------------------------------------------------------

        Sygnały:
        1 — zatrzymanie wejścia do sektora
        2 — wznowienie wejścia
        3 — ewakuacja (natychmiastowe zakończenie symulacji)
    */

    while (!SH->przerwanie && fscanf(f, "%d %d", &cmd, &sektor) == 2) {

        if (SH->przerwanie || SH->ewakuacja)
            break;

        /* ---------------- sygnal 1 — zatrzymanie wejścia ---------------- */
        if (cmd == 1) {
            if (sektor >= 0 && sektor < SEKTORY) {

                pthread_mutex_lock(&SH->mutex_wejsc);

                if (!SH->stop_sektor[sektor]) {
                    SH->stop_sektor[sektor] = 1;
                    loguj("Techniczny: wstrzymanie wejscia do sektora %d (sygnal1)", sektor);
                }

                pthread_mutex_unlock(&SH->mutex_wejsc);
            }
        }

        /* ---------------- sygnal 2 — wznowienie wejścia ---------------- */
        else if (cmd == 2) {
            if (sektor >= 0 && sektor < SEKTORY) {

                pthread_mutex_lock(&SH->mutex_wejsc);

                if (SH->stop_sektor[sektor]) {
                    SH->stop_sektor[sektor] = 0;
                    loguj("Techniczny: wznowienie wejscia do sektora %d (sygnal2)", sektor);
                }

                pthread_mutex_unlock(&SH->mutex_wejsc);
            }
        }

        /* ---------------- sygnal 3 — EWAKUACJA ---------------- */
        else if (cmd == 3) {

            pthread_mutex_lock(&SH->mutex_wejsc);

            SH->ewakuacja = 1;

            /* opróżnienie sektorów */
            for (int s = 0; s < SEKTORY; s++) {
                if (SH->osoby_w_sektorze[s] > 0) {
                    loguj("Techniczny: sektor %d opuszcza %d osob",
                          s, SH->osoby_w_sektorze[s]);
                    SH->osoby_w_sektorze[s] = 0;
                }
            }

            pthread_mutex_unlock(&SH->mutex_wejsc);

            loguj("Techniczny: ewakuacja zakonczona (sygnal3)");

            /* KLUCZ: wyślij SIGTERM do całej grupy procesów,
               żeby przerwać sem_wait w kasach i pętle w kibicach */
            kill(0, SIGTERM);

            /* techniczny kończy się NATYCHMIAST */
            break;
        }
    }

    loguj("Techniczny: koniec");
    _exit(0);
}
