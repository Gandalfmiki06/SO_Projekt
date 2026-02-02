/* sig_helpers.c - bezpieczny handler sygnałów i monitor sygnałów w wątku */

#define _POSIX_C_SOURCE 200809L
#include "hala.h"
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

/* async-safe handler: tylko ustawienie flagi, write do pipe i sem_post */
static void signal_handler(int sig)
{
    (void)sig;
    przerwanie = 1;
    const char c = 'x';
    if(sig_pipe[1] != -1){
        ssize_t r = write(sig_pipe[1], &c, 1);
        (void)r;
    }
    sem_post(&items_sem);
}

/* wątek monitorujący pipe - wykonuje bezpieczne operacje poza handlerem */
static void* sig_monitor(void* arg)
{
    (void)arg;
    char buf;
    while(1){
        ssize_t r = read(sig_pipe[0], &buf, 1);
        if(r <= 0){
            if(errno == EINTR) continue;
            break;
        }
        loguj("SYGNAL: odebrano powiadomienie w monitorze sygnalow");
        przerwanie = 1;

        pthread_cond_broadcast(&cond_wejsc);
        pthread_cond_broadcast(&cond_kolejka);

        for(int i=0;i<MAX_KASY;i++){
            sem_post(&items_sem);
        }

        break;
    }
    return NULL;
}

/* instalacja handlerów i konfiguracja pipe */
void install_signal_handlers(void)
{
    if(pipe(sig_pipe) == -1){
        perror("pipe");
        sig_pipe[0] = sig_pipe[1] = -1;
        return;
    }
    int flags = fcntl(sig_pipe[1], F_GETFL, 0);
    fcntl(sig_pipe[1], F_SETFL, flags | O_NONBLOCK);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
}

/* uruchom wątek monitorujący sygnały i zwróć jego pthread_t */
pthread_t start_signal_monitor(void)
{
    pthread_t t_sig;
    if(sig_pipe[0] == -1){
        return 0;
    }
    if(pthread_create(&t_sig, NULL, sig_monitor, NULL) != 0){
        perror("pthread_create sig_monitor");
        return 0;
    }
    return t_sig;
}
