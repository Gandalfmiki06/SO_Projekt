#include "hala.h"
#include <unistd.h>

void* kierownik(void* arg)
{
    sleep(2);
    stop_wejsc = 1;
    loguj("Kierownik: stop wejsc");

    sleep(2);
    stop_wejsc = 0;
    pthread_cond_broadcast(&cond_wejsc);
    loguj("Kierownik: wznowienie wejsc");

    sleep(5);
    ewakuacja = 1;
    loguj("Kierownik: ewakuacja");

    return NULL;
}
