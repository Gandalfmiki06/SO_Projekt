#ifndef LOG_H
#define LOG_H

/* Zapis pojedynczej linii do raportu */
void loguj(const char *fmt, ...);

/* Zapis końcowego podsumowania symulacji */
void zapisz_podsumowanie(void);

#endif
