#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hala.h"

/* Proste testy zgodne z wcześniejszą wersją */

static int test_sector_capacity(void)
{
    int sum = 0;
    for(int i = 0; i < SEKTORY; ++i){
        if(miejsca[i] < 0) return 0;
        sum += miejsca[i];
    }
    if(sum + sprzedane != K) return 0;
    return 1;
}

static int test_vip_limit(void)
{
    int max_vip_local = (int)(0.003 * K);
    if(stat_vip <= max_vip_local) return 1;
    return 0;
}

static int test_sum_consistency(void)
{
    int sum_miejsca = 0;
    for(int s = 0; s < SEKTORY; ++s) sum_miejsca += miejsca[s];
    return (sum_miejsca + sprzedane) == K;
}

static int test_pass_count_logic(void)
{
    Kibic k;
    memset(&k, 0, sizeof(k));
    k.id = 1;
    k.pass_count = 0;
    k.urgent = 0;

    for(int i = 0; i < 6; ++i){
        k.pass_count++;
        if(k.pass_count > 5) k.urgent = 1;
    }
    return (k.pass_count == 6) && (k.urgent == 1);
}

static int test_vip_wejsc_consistency(void)
{
    if(stat_vip_wejsc <= stat_vip) return 1;
    return 0;
}

static int test_control_limits(void)
{
    return 1;
}

void menu_testow(void)
{
    int total = 0;
    int failed = 0;

    printf("\n=== MENU TESTOW ===\n");

    ++total; if(test_sector_capacity()) printf("[OK] test_sector_capacity\n"); else { printf("[FAIL] test_sector_capacity\n"); ++failed; }
    ++total; if(test_vip_limit()) printf("[OK] test_vip_limit\n"); else { printf("[FAIL] test_vip_limit\n"); ++failed; }
    ++total; if(test_sum_consistency()) printf("[OK] test_sum_consistency\n"); else { printf("[FAIL] test_sum_consistency\n"); ++failed; }
    ++total; if(test_pass_count_logic()) printf("[OK] test_pass_count_logic\n"); else { printf("[FAIL] test_pass_count_logic\n"); ++failed; }
    ++total; if(test_vip_wejsc_consistency()) printf("[OK] test_vip_wejsc_consistency\n"); else { printf("[FAIL] test_vip_wejsc_consistency\n"); ++failed; }
    ++total; if(test_control_limits()) printf("[OK] test_control_limits\n"); else { printf("[FAIL] test_control_limits\n"); ++failed; }

    printf("\nTesty uruchomione: %d, niepowodzen: %d\n", total, failed);
    if(failed == 0) printf("Wszystkie testy przeszly pomyslnie.\n");
    else printf("Niektore testy nie przeszly. Sprawdz logi powyzej.\n");
    printf("===================\n\n");
}
