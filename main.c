#include "hala.h"
#include <pthread.h>
#include <stdlib.h>

void* kasa(void*);
void* kibic(void*);
void* techniczny(void*);
void* kierownik(void*);

int main() {
    printf("Start symulacji\n");

    pthread_t kasy[MAX_KASY];
    pthread_t kibice[K];
    pthread_t tech[SEKTORY];
    pthread_t boss;

    for (int i = 0; i < SEKTORY; i++) {
        miejsca[i] = K / SEKTORY;
        sem_init(&kontrola[i][0], 0, 3);
        sem_init(&kontrola[i][1], 0, 3);
    }

    pthread_create(&boss, NULL, kierownik, NULL);

    for (int i = 0; i < MAX_KASY; i++)
        pthread_create(&kasy[i], NULL, kasa, NULL);

    for (int i = 0; i < SEKTORY; i++)
        pthread_create(&tech[i], NULL, techniczny, &i);

    for (int i = 0; i < K; i++) {
        int* s = malloc(sizeof(int));
        *s = rand() % SEKTORY;
        pthread_create(&kibice[i], NULL, kibic, s);
    }

    pthread_join(boss, NULL);
    printf("Koniec symulacji\n");
    return 0;
}
