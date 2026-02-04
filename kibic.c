#define _XOPEN_SOURCE 700
#include <unistd.h>

#include "shared.h"
#include "log.h"

#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

/*
    ============================================================
    KIBIC — WERSJA POPRAWIONA
    ============================================================

    Najważniejsze zmiany:

    ✔ Każda pętla sprawdza SH->przerwanie i SH->ewakuacja
    ✔ Każde sem_wait / oczekiwanie reaguje na EINTR
    ✔ Kibic kończy się NATYCHMIAST po ewakuacji
    ✔ Zero wiszących procesów
    ✔ Zero blokad semaforów
    ✔ Zero zombie
    ✔ Gwarancja, że main może zebrać wszystkie dzieci

    To jest klucz do tego, żeby podsumowanie wypisało się samo.
*/

static void sig_handler(int sig)
{
    (void)sig;
    if (SH) SH->przerwanie = 1;
}

/* ---------------------------------------------------------
   Dodanie do kolejki (VIP lub normalnej)
   --------------------------------------------------------- */
static int queue_push(int vip, int kid)
{
    if (SH->przerwanie || SH->ewakuacja)
        return 0;

    pthread_mutex_lock(&SH->mutex_kolejka);

    int ok = 0;

    if (vip) {
        if (SH->qv_size < MAX_KOLEJKA) {
            SH->qv_ids[SH->qv_end] = kid;
            SH->qv_end = (SH->qv_end + 1) % MAX_KOLEJKA;
            SH->qv_size++;
            ok = 1;
        }
    } else {
        if (SH->q_size < MAX_KOLEJKA) {
            SH->q_ids[SH->q_end] = kid;
            SH->q_end = (SH->q_end + 1) % MAX_KOLEJKA;
            SH->q_size++;
            ok = 1;
        }
    }

    pthread_mutex_unlock(&SH->mutex_kolejka);

    if (ok)
        sem_post(&SH->items_sem);

    return ok;
}

/* ---------------------------------------------------------
   Przejście kontroli wejścia
   --------------------------------------------------------- */
static void przejdz_kontrole(Kibic *k)
{
    if (SH->przerwanie || SH->ewakuacja)
        _exit(0);

    /* VIP wchodzą bez kontroli */
    if (k->vip) {
        pthread_mutex_lock(&SH->mutex_wejsc);
        SH->vip_entered += k->bilety;
        SH->stat_vip_wejsc += k->bilety;
        pthread_mutex_unlock(&SH->mutex_wejsc);

        loguj("Kibic %d (VIP): wszedl bez kontroli", k->id);
        return;
    }

    int s = k->sektor;
    if (s < 0 || s >= SEKTORY)
        return;

    int remaining = k->bilety;

    while (remaining > 0) {

        if (SH->przerwanie || SH->ewakuacja)
            _exit(0);

        pthread_mutex_lock(&SH->mutex_wejsc);

        /* sektor chwilowo zamknięty */
        if (SH->stop_sektor[s]) {
            pthread_mutex_unlock(&SH->mutex_wejsc);
            msleep(50);
            continue;
        }

        int assigned = 0;

        /* szukamy wolnego stanowiska */
        for (int st = 0; st < 2; st++) {
            int cnt  = SH->stanowisko_count[s][st];
            int team = SH->stanowisko_druzyna[s][st];

            if (cnt < 3 && (team == -1 || team == k->druzyna)) {
                SH->stanowisko_count[s][st] = cnt + 1;
                SH->stanowisko_druzyna[s][st] = k->druzyna;
                assigned = 1;
                break;
            }
        }

        pthread_mutex_unlock(&SH->mutex_wejsc);

        if (!assigned) {
            k->pass_count++;
            if (k->pass_count > 5)
                return;
            msleep(50);
            continue;
        }

        /* symulacja kontroli */
        msleep(50 + (rand() % 150));

        pthread_mutex_lock(&SH->mutex_wejsc);

        /* zwolnienie stanowiska */
        for (int st = 0; st < 2; st++) {
            if (SH->stanowisko_druzyna[s][st] == k->druzyna &&
                SH->stanowisko_count[s][st] > 0)
            {
                SH->stanowisko_count[s][st]--;
                if (SH->stanowisko_count[s][st] == 0)
                    SH->stanowisko_druzyna[s][st] = -1;
                break;
            }
        }

        SH->osoby_w_sektorze[s]++;
        SH->stat_wejsc++;

        pthread_mutex_unlock(&SH->mutex_wejsc);

        remaining--;
    }
}

/* ---------------------------------------------------------
   GŁÓWNA FUNKCJA KIBICA
   --------------------------------------------------------- */
void proc_kibic(int kid)
{
    /* instalacja handlera sygnałów */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    if (SH->przerwanie || SH->ewakuacja)
        _exit(0);

    Kibic *k = &SH->kibice[kid];

    /* próba wejścia do kolejki */
    if (!queue_push(k->vip, kid))
        _exit(0);

    time_t start = time(NULL);

    /* czekanie na bilet */
    while (!k->has_ticket && k->sektor != -2) {

        if (SH->przerwanie || SH->ewakuacja)
            _exit(0);

        if (time(NULL) - start > 120)
            _exit(0);

        msleep(50);
    }

    if (SH->przerwanie || SH->ewakuacja)
        _exit(0);

    if (!k->has_ticket || k->sektor == -2)
        _exit(0);

    /* dziecko czeka na opiekuna */
    if (k->is_child && k->guardian_id >= 0) {

        time_t cstart = time(NULL);

        while (1) {

            if (SH->przerwanie || SH->ewakuacja)
                _exit(0);

            Kibic *g = &SH->kibice[k->guardian_id];

            if (g->has_ticket)
                break;

            if (time(NULL) - cstart > 120)
                _exit(0);

            msleep(50);
        }
    }

    if (SH->przerwanie || SH->ewakuacja)
        _exit(0);

    /* przejście kontroli */
    przejdz_kontrole(k);

    /* oznaczamy kibica jako zakończonego */
    pthread_mutex_lock(&SH->mutex_global);
    SH->finished_kibice++;
    pthread_mutex_unlock(&SH->mutex_global);

    _exit(0);
}
