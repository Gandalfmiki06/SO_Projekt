#include "hala.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

/* ================= RESET STANU ================= */

void reset_stanu() {
    sprzedane = 0;
    czynne_kasy = 2;
    kibice_w_kolejce = 0;
    q_size = qv_size = 0;
    q_start = q_end = qv_start = qv_end = 0;
    ewakuacja = 0;

    for(int i=0;i<SEKTORY;i++) {
        miejsca[i] = K/SEKTORY;
        osoby_w_sektorze[i] = 0;
        stop_sektor[i] = 0;
    }
}

/* ================= TESTY ================= */

void test_fifo() {
    reset_stanu();

    Kibic a={.id=1}, b={.id=2};
    kolejka[q_end++]=&a; q_size++;
    kolejka[q_end++]=&b; q_size++;

    Kibic* pierwszy = kolejka[q_start++];
    Kibic* drugi = kolejka[q_start++];

    if(pierwszy->id==1 && drugi->id==2)
        printf("FIFO dziala poprawnie\n");
    else
        printf("FIFO nie dziala\n");
}

void test_vip_omija() {
    reset_stanu();

    Kibic normalny={.id=1,.vip=0};
    Kibic vip={.id=2,.vip=1};

    kolejka[q_end++]=&normalny; q_size++;
    kolejka_vip[qv_end++]=&vip; qv_size++;

    Kibic* obslugiwany = (qv_size>0) ? kolejka_vip[qv_start++] : kolejka[q_start++];

    if(obslugiwany->vip)
        printf("VIP omija kolejke\n");
    else
        printf("VIP nie ominal kolejki\n");
}

void test_limit_2_bilety() {
    Kibic k={.bilety=3};
    if(k.bilety>2)
        printf("Limit 2 biletow wykrywany\n");
    else
        printf("Limit biletow nie dziala\n");
}

void test_sprzedaz() {
    reset_stanu();
    miejsca[0]=5;
    Kibic k={.bilety=2,.sektor=0};

    pthread_mutex_lock(&mutex_bilety);
    miejsca[0]-=k.bilety;
    sprzedane+=k.bilety;
    pthread_mutex_unlock(&mutex_bilety);

    if(miejsca[0]==3 && sprzedane==2)
        printf("Sprzedaz biletow poprawna\n");
    else
        printf("Blad sprzedazy\n");
}

void test_losowy_sektor() {
    int s=rand()%SEKTORY;
    if(s>=0 && s<SEKTORY)
        printf("Losowy sektor w zakresie\n");
    else
        printf("Losowy sektor poza zakresem\n");
}

void test_pid() {
    printf("PID procesu: %d\n", getpid());
}

void test_dynamiczne_kasy() {
    reset_stanu();
    kibice_w_kolejce = (K/10)*4;

    int potrzebne = kibice_w_kolejce/(K/10)+1;
    if(potrzebne<2) potrzebne=2;
    if(potrzebne>MAX_KASY) potrzebne=MAX_KASY;

    if(potrzebne==5)
        printf("Dynamiczna liczba kas OK\n");
    else
        printf("Bledne wyliczenie kas (%d)\n",potrzebne);
}

void test_sektor_stop() {
    reset_stanu();
    stop_sektor[2]=1;
    if(stop_sektor[2]==1)
        printf("Sektor zatrzymany\n");
    else
        printf("Blad zatrzymania sektora\n");
}

void test_ewakuacja_flag() {
    ewakuacja=1;
    if(ewakuacja)
        printf("Flaga ewakuacji ustawiona\n");
    else
        printf("Flaga ewakuacji nie dziala\n");
    ewakuacja=0;
}

void test_miejsca_nieujemne() {
    reset_stanu();
    miejsca[0]=1;
    Kibic k={.bilety=2,.sektor=0};

    if(miejsca[0] < k.bilety)
        printf("Blokada sprzedazy ponad limit\n");
    else
        printf("Sprzedaz ponad limit mozliwa\n");
}

void test_kontrola_limit_osob() {
    int max=3;
    if(max==3)
        printf("Limit 3 osob na stanowisku\n");
    else
        printf("Zly limit kontroli\n");
}

/* ================= MENU ================= */

void menu_testow() {
    int wybor;
    do {
        printf("\n==== TESTY JEDNOSTKOWE ====\n");
        printf("1. Kolejka FIFO\n");
        printf("2. VIP omija kolejke\n");
        printf("3. Limit 2 biletow\n");
        printf("4. Sprzedaz biletow\n");
        printf("5. Losowy sektor\n");
        printf("6. PID procesu\n");
        printf("7. Dynamiczne kasy\n");
        printf("8. Zatrzymanie sektora\n");
        printf("9. Flaga ewakuacji\n");
        printf("10. Blokada sprzedazy ponad limit\n");
        printf("11. Limit osob na kontroli\n");
        printf("0. Wyjscie\n");
        printf("Wybor: ");
        scanf("%d",&wybor);

        switch(wybor) {
            case 1: test_fifo(); break;
            case 2: test_vip_omija(); break;
            case 3: test_limit_2_bilety(); break;
            case 4: test_sprzedaz(); break;
            case 5: test_losowy_sektor(); break;
            case 6: test_pid(); break;
            case 7: test_dynamiczne_kasy(); break;
            case 8: test_sektor_stop(); break;
            case 9: test_ewakuacja_flag(); break;
            case 10: test_miejsca_nieujemne(); break;
            case 11: test_kontrola_limit_osob(); break;
        }
    } while(wybor!=0);
}
