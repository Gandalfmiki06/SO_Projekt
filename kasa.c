#define _XOPEN_SOURCE 700
#include <unistd.h>

#include "shared.h"
#include "log.h"

#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <string.h>

/*
    ============================================================
    KASA — WERSJA Z POPRAWIONĄ OBSŁUGĄ VIP (A1‑V1 + S1)
    ============================================================

    Zmiany:

    ✔ Kasy NIE obsługują VIP
    ✔ VIP nie używają kolejki ani semafora
    ✔ Kasy obsługują wyłącznie zwykłych kibiców
    ✔ VIP nie wpływają na liczbę czynnych kas
    ✔ VIP nie mogą trafić do queue_pop()
    ✔ VIP nie mogą być sprzedani przez kasę
*/

static void sig_handler(int sig)
{
    (void)sig;
    if (SH) SH->przerwanie = 1;
}

/* ---------------------------------------------------------
   Pobranie zwykłego kibica z kolejki
   --------------------------------------------------------- */
static int queue_pop(void)
{
    pthread_mutex_lock(&SH->mutex_kolejka);

    int kid = -1;

    if (SH->q_size > 0) {
        kid = SH->q_ids[SH->q_start];
        SH->q_start = (SH->q_start + 1) % MAX_KOLEJKA;
        SH->q_size--;
    }

    pthread_mutex_unlock(&SH->mutex_kolejka);
    return kid;
}

/* ---------------------------------------------------------
   GŁÓWNA FUNKCJA KASY
   --------------------------------------------------------- */
void proc_kasa(int id)
{
    /* instalacja handlera sygnałów */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    loguj("Kasa %d: start", id);

    while (1) {

        /* -----------------------------------------------------
           NATYCHMIASTOWE WYJŚCIE PO EWAKUACJI / CTRL+C
           ----------------------------------------------------- */
        if (SH->przerwanie || SH->ewakuacja)
            break;

        /* -----------------------------------------------------
           Kasa może być czasowo nieczynna (kierownik steruje)
           ----------------------------------------------------- */
        pthread_mutex_lock(&SH->mutex_global);
        int czynne = SH->czynne_kasy;
        pthread_mutex_unlock(&SH->mutex_global);

        if (id >= czynne) {
            msleep(50);
            continue;
        }

        /* -----------------------------------------------------
           sem_wait — MUSI obsługiwać EINTR
           ----------------------------------------------------- */
        int r = sem_wait(&SH->items_sem);

        if (r == -1 && errno == EINTR) {
            if (SH->przerwanie || SH->ewakuacja)
                break;
            continue;
        }

        if (SH->przerwanie || SH->ewakuacja)
            break;

        /* -----------------------------------------------------
           Pobranie kibica z kolejki
           ----------------------------------------------------- */
        int kid = queue_pop();
        if (kid < 0)
            continue;

        Kibic *k = &SH->kibice[kid];

        /* -----------------------------------------------------
           Kasa obsługuje TYLKO zwykłych kibiców
           ----------------------------------------------------- */
        if (k->vip) {
            /* VIP nie powinien tu trafić */
            loguj("Kasa %d: BLAD — VIP w kolejce (kid=%d)", id, kid);
            continue;
        }

        /* -----------------------------------------------------
           Sprzedaż biletu
           ----------------------------------------------------- */
        pthread_mutex_lock(&SH->mutex_bilety);

        /* brak miejsc w całej hali (sektory 0–7 + VIP) */
        if (SH->sprzedane >= SH->K + SH->max_vip * 2) {
            pthread_mutex_unlock(&SH->mutex_bilety);
            k->sektor = -2;
            k->has_ticket = 0;
            continue;
        }

        int chosen = -1;

        /* -----------------------------------------------------
           Dziecko — musi iść do sektora opiekuna
           ----------------------------------------------------- */
        if (k->is_child && k->guardian_id >= 0) {

            Kibic *g = &SH->kibice[k->guardian_id];

            if (!g->has_ticket || g->sektor < 0) {
                pthread_mutex_unlock(&SH->mutex_bilety);
                k->sektor = -2;
                k->has_ticket = 0;
                continue;
            }

            int s = g->sektor;

            if (s == SEKTOR_VIP) {
                /* dziecko nie może wejść do sektora VIP */
                pthread_mutex_unlock(&SH->mutex_bilety);
                k->sektor = -2;
                k->has_ticket = 0;
                continue;
            }

            if (SH->miejsca[s] < k->bilety) {
                pthread_mutex_unlock(&SH->mutex_bilety);
                k->sektor = -2;
                k->has_ticket = 0;
                continue;
            }

            chosen = s;
        }
        else {
            /* -----------------------------------------------------
               Normalny kibic — wybór sektora 0–7
               ----------------------------------------------------- */
            int avail[8];
            int ac = 0;

            for (int s = 0; s < 8; s++) {
                if (SH->miejsca[s] >= k->bilety)
                    avail[ac++] = s;
            }

            if (ac == 0) {
                pthread_mutex_unlock(&SH->mutex_bilety);
                k->sektor = -2;
                k->has_ticket = 0;
                continue;
            }

            chosen = avail[rand() % ac];
        }

        /* -----------------------------------------------------
           Sprzedaż biletu do wybranego sektora
           ----------------------------------------------------- */
        SH->miejsca[chosen] -= k->bilety;
        SH->sprzedane += k->bilety;
        SH->sold_per_sector[chosen] += k->bilety;

        k->sektor = chosen;
        k->has_ticket = 1;

        /* statystyki */
        if (k->is_child)
            SH->stat_dzieci += k->bilety;
        else
            SH->stat_normalni += k->bilety;

        pthread_mutex_unlock(&SH->mutex_bilety);

        /* powiadom kibiców czekających na opiekuna */
        pthread_cond_broadcast(&SH->cond_wejsc);

        /* -----------------------------------------------------
           Jeśli hala pełna — kasa kończy pracę
           ----------------------------------------------------- */
        if (SH->sprzedane >= SH->K + SH->max_vip * 2)
            break;
    }

    loguj("Kasa %d: koniec", id);
    _exit(0);
}
