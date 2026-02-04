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
    KASA — WERSJA POPRAWIONA
    ============================================================

    Najważniejsze zmiany:

    ✔ Każda pętla sprawdza SH->przerwanie i SH->ewakuacja
    ✔ sem_wait() obsługuje EINTR (przerwanie sygnałem)
    ✔ kasa kończy się NATYCHMIAST po ewakuacji
    ✔ zero wiszących procesów
    ✔ zero blokad semaforów
    ✔ zero zombie
    ✔ main może zebrać wszystkie dzieci → podsumowanie wypisuje się samo

    To jest kluczowy element stabilności całej symulacji.
*/

static void sig_handler(int sig)
{
    (void)sig;
    if (SH) SH->przerwanie = 1;
}

/* ---------------------------------------------------------
   Pobranie kibica z kolejki (VIP ma pierwszeństwo)
   --------------------------------------------------------- */
static int queue_pop(void)
{
    pthread_mutex_lock(&SH->mutex_kolejka);

    int kid = -1;

    /* VIP najpierw */
    if (SH->qv_size > 0) {
        kid = SH->qv_ids[SH->qv_start];
        SH->qv_start = (SH->qv_start + 1) % MAX_KOLEJKA;
        SH->qv_size--;
    }
    /* potem normalni */
    else if (SH->q_size > 0) {
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
           inaczej kasa będzie wisieć po sygnale
           ----------------------------------------------------- */
        int r = sem_wait(&SH->items_sem);

        if (r == -1 && errno == EINTR) {
            /* przerwane sygnałem → sprawdź flagi i ewentualnie wyjdź */
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
           Sprzedaż biletu
           ----------------------------------------------------- */
        pthread_mutex_lock(&SH->mutex_bilety);

        /* brak miejsc w całej hali */
        if (SH->sprzedane >= SH->K) {
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

            /* opiekun nie ma biletu → dziecko odpada */
            if (!g->has_ticket || g->sektor < 0) {
                pthread_mutex_unlock(&SH->mutex_bilety);
                k->sektor = -2;
                k->has_ticket = 0;
                continue;
            }

            int s = g->sektor;

            /* brak miejsc w sektorze opiekuna */
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
               Normalny kibic — wybór sektora z wolnymi miejscami
               ----------------------------------------------------- */
            int avail[SEKTORY];
            int ac = 0;

            for (int s = 0; s < SEKTORY; s++) {
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
        if (k->vip)
            SH->vip_sold += k->bilety;
        else if (k->is_child)
            SH->stat_dzieci += k->bilety;
        else
            SH->stat_normalni += k->bilety;

        pthread_mutex_unlock(&SH->mutex_bilety);

        /* powiadom kibiców czekających na opiekuna */
        pthread_cond_broadcast(&SH->cond_wejsc);

        /* -----------------------------------------------------
           Jeśli hala pełna — kasa kończy pracę
           ----------------------------------------------------- */
        if (SH->sprzedane >= SH->K)
            break;
    }

    loguj("Kasa %d: koniec", id);
    _exit(0);
}
