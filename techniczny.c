#include "hala.h"
#include <unistd.h>

void* techniczny(void* arg)
{
    while (!ewakuacja)
        sleep(1);

    loguj("Techniczny: sektor pusty");
    return NULL;
}
