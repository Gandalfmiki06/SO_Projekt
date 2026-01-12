#include "hala.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

int main() {
    srand(time(NULL));

    /* inicjalizacja sektorów */
    for (int i = 0; i < SEKTORY; i++) {
        miejsca[i] = K / SEKTORY;
    }

    pthread_t kasy[MAX_KASY];
    int kasa_id[MAX_KASY];

    pthread_t kibice[K];
    Kibic dane[K];

    /* start z 2 kasami */
    for (int i = 0; i < 2; i++) {
        kasa_id[i] = i;
        pthread_create(&kasy[i], NULL, kasa, &kasa_id[i]);
    }

    /* tworzenie kibiców */
    for (int i = 0; i < K; i++) {
        dane[i].id = i;
        dane[i].vip = (rand() % 1000 < 3); // <0.3%
        dane[i].bilety = 0;
        pthread_create(&kibice[i], NULL, kibic, &dane[i]);
        usleep(100000);
    }

    /* dynamiczne sterowanie kasami */
    while (sprzedane < K) {
        pthread_mutex_lock(&mutex_kasy);

        int wymagane = kibice_w_kolejce / (K / 10) + 1;
        if (wymagane < 2) wymagane = 2;
        if (wymagane > MAX_KASY) wymagane = MAX_KASY;

        while (czynne_kasy < wymagane) {
            kasa_id[czynne_kasy] = czynne_kasy;
            pthread_create(&kasy[czynne_kasy], NULL, kasa,
                           &kasa_id[czynne_kasy]);
            czynne_kasy++;
            loguj("Otwarto nową kase");
        }

        pthread_mutex_unlock(&mutex_kasy);
        sleep(1);
    }

    sleep(2);
    loguj("Sprzedano wszystkie bilety – koniec");

    return 0;
}
