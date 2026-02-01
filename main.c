#include "hala.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
void menu_testow();
void handler(int sig)
{
    przerwanie=1;
    ewakuacja=1;
    loguj("SYGNAL: wymuszenie ewakuacji");
    pthread_cond_broadcast(&cond_wejsc);
    pthread_cond_broadcast(&cond_kolejka);
}

int main()
{
	int tryb;
	printf("1 - Symulacja\n2 - Testy jednostkowe\nWybor: ");
	scanf("%d",&tryb);

	if(tryb == 2) {
		menu_testow();
    return 0;
}
	
	
	
    signal(SIGINT, handler);
    signal(SIGTERM, handler);
    signal(SIGQUIT, handler);

    srand(time(NULL));

    for(int i=0;i<SEKTORY;i++){
        miejsca[i]=K/SEKTORY;
        for(int j=0;j<2;j++){
            sem_init(&kontrola[i][j],0,3);
            stanowisko_druzyna[i][j]=-1;
        }
    }

    pthread_t kasy[MAX_KASY], kibice[120], t_kier, t_tech;

    pthread_create(&t_kier,NULL,kierownik,NULL);
    pthread_create(&t_tech,NULL,techniczny,NULL);

    for(int i=0;i<MAX_KASY;i++)
        pthread_create(&kasy[i],NULL,kasa,(void*)(size_t)i);

    for(int i=0;i<120;i++){
        Kibic* k=malloc(sizeof(Kibic));
        k->id=i;
        k->vip=(rand()%1000<3);
        k->wiek=rand()%40;
        k->druzyna=rand()%2;
        k->bilety=1+rand()%2;
        k->sektor=rand()%SEKTORY;
        pthread_create(&kibice[i],NULL,kibic,k);
    }

    for(int i=0;i<120;i++) pthread_join(kibice[i],NULL);
    for(int i=0;i<MAX_KASY;i++) pthread_join(kasy[i],NULL);
    pthread_join(t_kier,NULL);
    pthread_join(t_tech,NULL);

    printf("\n===== PODSUMOWANIE =====\n");
    printf("Sprzedane: %d\nVIP: %d\nNormalni: %d\nDzieci: %d\nWeszli: %d\n",
           sprzedane,stat_vip,stat_normalni,stat_dzieci,stat_wejsc);

    loguj("Koniec symulacji");
    return 0;
}
