#include "hala.h"
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include "hala.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>   // usleep

int main()
{
    for (int i = 0; i < SEKTORY; i++) {
        miejsca[i] = K / SEKTORY;
        for (int j = 0; j < 2; j++)
            sem_init(&kontrola[i][j], 0, 3);
    }

    pthread_t kasy[MAX_KASY], kibice[50], t_kier, t_tech;

    pthread_create(&t_kier, NULL, kierownik, NULL);
    pthread_create(&t_tech, NULL, techniczny, NULL);

    for (int i = 0; i < MAX_KASY; i++)
        pthread_create(&kasy[i], NULL, kasa, NULL);

    for (int i = 0; i < 50; i++) {
        Kibic* k = malloc(sizeof(Kibic));
        k->id = i;
        k->vip = (i % 20 == 0);
        k->wiek = 20;
        k->druzyna = i % 2;
        k->bilety = 1 + rand() % 2;
        k->sektor = rand() % SEKTORY;
        pthread_create(&kibice[i], NULL, kibic, k);
        usleep(50000);
    }

    /* czekamy na wątki */
for (int i = 0; i < MAX_KASY; i++)
    pthread_join(kasy[i], NULL);

for (int i = 0; i < 50; i++)
    pthread_join(kibice[i], NULL);

pthread_join(t_kier, NULL);
pthread_join(t_tech, NULL);

loguj("Koniec symulacji");
return 0;

}
