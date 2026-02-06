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
    KIBIC — WERSJA Z POPRAWNĄ OBSŁUGĄ VIP (A1‑V1 + S1)
    ============================================================

    Zmiany:

    ✔ VIP nie używają kolejki
    ✔ VIP kupują bilet natychmiast (bez kas)
    ✔ VIP zawsze dostają sektor 8
    ✔ VIP nie przechodzą kontroli
    ✔ VIP nie mogą być dziećmi
    ✔ VIP nie używają semaforów
    ✔ VIP nie mają pass_count
    ✔ VIP są liczeni w sprzedanych miejscach
    ✔ VIP są ewakuowani jak inni
*/

static void sig_handler(int sig)
{
    (void)sig;
    if (SH) SH->przerwanie = 1;
}

/* ---------------------------------------------------------
   Dodanie zwykłego kibica do kolejki
   --------------------------------------------------------- */
static int queue_push_normal(int kid)
{
    if (SH->przerwanie || SH->ewakuacja)
        return 0;

    pthread_mutex_lock(&SH->mutex_kolejka);

    int ok = 0;

    if (SH->q_size < MAX_KOLEJKA) {
        SH->q_ids[SH->q_end] = kid;
        SH->q_end = (SH->q_end + 1) % MAX_KOLEJKA;
        SH->q_size++;
        ok = 1;
    }

    pthread_mutex_unlock(&SH->mutex_kolejka);

    if (ok)
        sem_post(&SH->items_sem);

    return ok;
}

/* ---------------------------------------------------------
   VIP kupuje bilet natychmiast (bez kolejki, bez kas)
   --------------------------------------------------------- */
static void vip_buy_ticket(Kibic *k)
{
    pthread_mutex_lock(&SH->mutex_bilety);

    /* sprawdzenie miejsc w sektorze VIP */
    if (SH->miejsca[SEKTOR_VIP] >= k->bilety) {

        SH->miejsca[SEKTOR_VIP] -= k->bilety;
        SH->sprzedane += k->bilety;
        SH->sold_per_sector[SEKTOR_VIP] += k->bilety;

        SH->vip_sold += k->bilety;

        k->sektor = SEKTOR_VIP;
        k->has_ticket = 1;

        loguj("Kibic %d (VIP): kupil bilet natychmiast, sektor VIP", k->id);
    }
    else {
        /* brak miejsc w sektorze VIP */
        k->sektor = -2;
        k->has_ticket = 0;
    }

    pthread_mutex_unlock(&SH->mutex_bilety);
}

/* ---------------------------------------------------------
   Przejście kontroli wejścia (VIP pomijają)
   --------------------------------------------------------- */
static void przejdz_kontrole(Kibic *k)
{
    if (SH->przerwanie || SH->ewakuacja)
        _exit(0);

    /* --- VIP wchodzą bez kontroli --- */
    if (k->vip) {
        pthread_mutex_lock(&SH->mutex_wejsc);

        SH->vip_entered += k->bilety;
        SH->stat_vip_wejsc += k->bilety;
        SH->osoby_w_sektorze[SEKTOR_VIP] += k->bilety;

        pthread_mutex_unlock(&SH->mutex_wejsc);

        loguj("Kibic %d (VIP): wszedl bez kontroli", k->id);
        return;
    }

    /* --- Zwykły kibic --- */

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

    /* --- VIP --- */
    if (k->vip) {

        /* VIP nie mogą być dziećmi */
        if (k->is_child)
            _exit(0);

        vip_buy_ticket(k);

        if (!k->has_ticket || k->sektor == -2)
            _exit(0);

        przejdz_kontrole(k);

        pthread_mutex_lock(&SH->mutex_global);
        SH->finished_kibice++;
        pthread_mutex_unlock(&SH->mutex_global);

        _exit(0);
    }

    /* --- Zwykły kibic --- */

    if (!queue_push_normal(kid))
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

    przejdz_kontrole(k);

    pthread_mutex_lock(&SH->mutex_global);
    SH->finished_kibice++;
    pthread_mutex_unlock(&SH->mutex_global);

    _exit(0);
}
