#define _XOPEN_SOURCE 700
#include <unistd.h>

#include "shared.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

Shared *SH = NULL;
static const char *SHM_NAME = "/hala_proc_shm";

/* ---------------------------------------------------------
   MASTER — tworzy segment SHM i inicjalizuje strukturę
   --------------------------------------------------------- */
void shared_init_master(int K)
{
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if(fd == -1){
        perror("shm_open");
        exit(1);
    }
    if(ftruncate(fd, sizeof(Shared)) == -1){
        perror("ftruncate");
        exit(1);
    }
    SH = mmap(NULL, sizeof(Shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(SH == MAP_FAILED){
        perror("mmap");
        exit(1);
    }
    close(fd);

    memset(SH, 0, sizeof(Shared));
    SH->K = K;
    SH->sprzedane = 0;
    SH->czynne_kasy = 2;
    SH->next_kibic_id = 0;
    SH->total_kibice = 0;
    SH->finished_kibice = 0;
    SH->ewakuacja = 0;
    SH->przerwanie = 0;
    SH->mecz_started = 0;

    /* rozdział miejsc */
    int base = K / SEKTORY;
    int rem  = K % SEKTORY;
    for(int i=0;i<SEKTORY;i++){
        SH->miejsca[i] = base + (i < rem ? 1 : 0);
        SH->sold_per_sector[i] = 0;
        SH->stop_sektor[i] = 0;
        SH->osoby_w_sektorze[i] = 0;
        for(int j=0;j<2;j++){
            SH->stanowisko_count[i][j] = 0;
            SH->stanowisko_druzyna[i][j] = -1;
        }
    }

    /* VIP limit */
    SH->max_vip = (int)(0.003 * K);
    if(SH->max_vip < 1) SH->max_vip = 1;
    SH->vip_reserved = 0;
    SH->vip_sold = 0;
    SH->vip_entered = 0;

    /* mutexy współdzielone */
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);

    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);

    pthread_mutex_init(&SH->mutex_log, &mattr);
    pthread_mutex_init(&SH->mutex_kolejka, &mattr);
    pthread_mutex_init(&SH->mutex_bilety, &mattr);
    pthread_mutex_init(&SH->mutex_wejsc, &mattr);
    pthread_mutex_init(&SH->mutex_global, &mattr);

    pthread_cond_init(&SH->cond_kolejka, &cattr);
    pthread_cond_init(&SH->cond_wejsc, &cattr);
    pthread_cond_init(&SH->cond_global, &cattr);

    /* semafor współdzielony */
    sem_init(&SH->items_sem, 1, 0);
}

/* ---------------------------------------------------------
   Dzieci — tylko attach
   --------------------------------------------------------- */
void shared_attach(void)
{
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if(fd == -1){
        perror("shm_open attach");
        exit(1);
    }
    SH = mmap(NULL, sizeof(Shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(SH == MAP_FAILED){
        perror("mmap attach");
        exit(1);
    }
    close(fd);
}

/* ---------------------------------------------------------
   Sprzątanie
   --------------------------------------------------------- */
void shared_cleanup(void)
{
    if(SH){
        munmap(SH, sizeof(Shared));
        shm_unlink(SHM_NAME);
        SH = NULL;
    }
}
