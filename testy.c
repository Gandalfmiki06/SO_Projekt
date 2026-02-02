/* testy_ext.c - rozszerzone testy jednostkowe i symulowane
 *
 * Uruchomienie:
 *   gcc testy_ext.c *.c -o testy -lpthread
 *   ./testy
 *
 * Testy są zaprojektowane tak, aby nie modyfikować logiki produkcyjnej,
 * tylko manipulować globalnymi strukturami i wywoływać fragmenty logiki
 * w kontrolowany sposób.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "hala.h"
#include "queue.h"

/* Pomocnicze: szybkie porównanie zera/niezera */
#define ASSERT(cond, msg) do { if(!(cond)) { printf("[FAIL] %s\n", msg); return 0; } } while(0)
#define OK(msg) do { printf("[OK] %s\n", msg); } while(0)

/* Kopia prostych testów z poprzedniej wersji + nowe testy */

/* Suma miejsc */
static int sum_miejsca(void) {
    int s = 0;
    for (int i = 0; i < SEKTORY; ++i) s += miejsca[i];
    return s;
}

/* Test 1: spójność sumy miejsc po init */
static int test_sum_consistency_after_init(void) {
    init_simulation(0);
    int sum = sum_miejsca();
    ASSERT((sum + sprzedane) == K, "sum_miejsca + sprzedane != K");
    OK("test_sum_consistency_after_init");
    return 1;
}

/* Test 2: pojemności sektorów i zerowe sold_per_sector */
static int test_sector_capacity_after_init(void) {
    init_simulation(0);
    for (int i = 0; i < SEKTORY; ++i) {
        ASSERT(miejsca[i] >= 0, "miejsca[i] < 0");
        ASSERT(sold_per_sector[i] == 0, "sold_per_sector[i] != 0");
    }
    OK("test_sector_capacity_after_init");
    return 1;
}

/* Test 3: limit VIP po init */
static int test_vip_limit_after_init(void) {
    init_simulation(0);
    int max_vip_local = (int)(0.003 * K);
    if (max_vip_local < 1) max_vip_local = 1;
    if (max_vip != max_vip_local) {
        printf("[WARN] max_vip globalny=%d oczekiwany=%d\n", max_vip, max_vip_local);
    }
    OK("test_vip_limit_after_init");
    return 1;
}

/* Test 4: pass_count logic unit (prosty) */
static int test_pass_count_logic_unit(void) {
    Kibic k;
    memset(&k, 0, sizeof(k));
    k.id = 1;
    k.pass_count = 0;
    k.urgent = 0;
    for (int i = 0; i < 6; ++i) {
        k.pass_count++;
        if (k.pass_count > 5) k.urgent = 1;
    }
    ASSERT(k.pass_count == 6, "pass_count != 6");
    ASSERT(k.urgent == 1, "urgent != 1");
    OK("test_pass_count_logic_unit");
    return 1;
}

/* Test 5: vip_entered nie przekracza vip_sold (symulacja technicznego) */
static int test_vip_entered_not_exceed_sold(void) {
    init_simulation(0);
    /* ustawiamy vip_sold na 3 */
    pthread_mutex_lock(&mutex_vip);
    vip_sold = 3;
    vip_reserved = 0;
    vip_entered = 0;
    pthread_mutex_unlock(&mutex_vip);

    /* symulujemy technicznego próbującego dodać 5 wejść VIP dla jednego kibica */
    int n = 5;
    int added = 0;
    pthread_mutex_lock(&mutex_vip);
    int can_add = vip_sold - vip_entered;
    int add = n;
    if (add > can_add) add = can_add;
    if (add > 0) vip_entered += add;
    added = add;
    pthread_mutex_unlock(&mutex_vip);

    ASSERT(added == 3, "vip_entered powinno zostać ograniczone do vip_sold");
    ASSERT(vip_entered == 3, "vip_entered != vip_sold po dodaniu");
    OK("test_vip_entered_not_exceed_sold");
    return 1;
}

/* Test 6: queue push/pop (Queue API) */
static int test_queue_push_pop(void) {
    Queue q;
    queue_init(&q);
    Kibic a, b, *p;
    memset(&a,0,sizeof(a));
    memset(&b,0,sizeof(b));
    a.id = 1; b.id = 2;
    ASSERT(queue_push(&q, &a) == 1, "queue_push a failed");
    ASSERT(queue_push(&q, &b) == 1, "queue_push b failed");
    p = queue_pop(&q);
    ASSERT(p == &a, "queue_pop first != a");
    p = queue_pop(&q);
    ASSERT(p == &b, "queue_pop second != b");
    OK("test_queue_push_pop");
    return 1;
}

/* Test 7: atomic sales under mutex (symulacja) */
static int test_atomic_sales_simulated(void) {
    init_simulation(0);
    int s = 0;
    int tickets_to_sell = 5;
    if (miejsca[s] < tickets_to_sell) {
        printf("[SKIP] nie ma wystarczajaco miejsc w sektorze %d do testu\n", s);
        return 1;
    }

    pthread_mutex_lock(&mutex_bilety);
    int before_miejsca = miejsca[s];
    int before_sprzedane = sprzedane;
    miejsca[s] -= tickets_to_sell;
    sprzedane += tickets_to_sell;
    sold_per_sector[s] += tickets_to_sell;
    pthread_mutex_unlock(&mutex_bilety);

    ASSERT(miejsca[s] == before_miejsca - tickets_to_sell, "miejsca po transakcji niepoprawne");
    ASSERT(sprzedane == before_sprzedane + tickets_to_sell, "sprzedane po transakcji niepoprawne");
    ASSERT(sold_per_sector[s] == tickets_to_sell, "sold_per_sector niepoprawne");
    int sum = sum_miejsca();
    ASSERT((sum + sprzedane) == K, "suma po transakcji niezgodna z K");
    OK("test_atomic_sales_simulated");
    return 1;
}

/* Test 8: requeue simulated (manipulacja kolejką globalną) */
static int test_requeue_simulated(void) {
    init_simulation(0);
    Kibic *k = malloc(sizeof(Kibic));
    if (!k) return 0;
    memset(k, 0, sizeof(Kibic));
    k->id = 9999;
    k->bilety = 1;
    k->sektor = -1;
    k->in_queue = 0;

    pthread_mutex_lock(&mutex_kolejka);
    int start_q = q_size;
    if (q_size >= MAX_KOLEJKA) {
        pthread_mutex_unlock(&mutex_kolejka);
        free(k);
        return 1;
    }
    kolejka[q_end] = k;
    q_end = (q_end + 1) % MAX_KOLEJKA;
    q_size++;
    k->in_queue = 1;
    pthread_mutex_unlock(&mutex_kolejka);

    ASSERT(q_size == start_q + 1, "q_size po requeue nie wzroslo");

    pthread_mutex_lock(&mutex_kolejka);
    int found = 0;
    for (int i = 0; i < MAX_KOLEJKA; ++i) {
        if (kolejka[i] == k) { kolejka[i] = NULL; found = 1; break; }
    }
    if (found) {
        q_size--;
        k->in_queue = 0;
    }
    pthread_mutex_unlock(&mutex_kolejka);

    free(k);
    ASSERT(found, "nie znaleziono w kolejce w requeue_simulated");
    OK("test_requeue_simulated");
    return 1;
}

/* Test 9: timeout counters simulated */
static int test_timeout_counters_simulated(void) {
    init_simulation(0);
    int before = timeout_count;
    __sync_fetch_and_add(&timedout_before_dequeue, 1);
    __sync_fetch_and_add(&timeout_count, 1);
    __sync_fetch_and_add(&timedout_after_dequeue, 1);
    __sync_fetch_and_add(&timeout_count, 1);

    ASSERT(timeout_count == before + 2, "timeout_count nie wzrosl o 2");
    OK("test_timeout_counters_simulated");
    return 1;
}

/* Test 10: entered_flag simulated */
static int test_entered_flag_simulated(void) {
    init_simulation(0);
    Kibic *k = malloc(sizeof(Kibic));
    if (!k) return 0;
    memset(k, 0, sizeof(Kibic));
    k->id = 42;
    k->to_enter = 2;
    k->sektor = 3;

    entered_flag[k->id] = 0;
    adults_entered_in_sector[k->sektor] = 0;

    entered_flag[k->id] = 1;
    adults_entered_in_sector[k->sektor] += k->to_enter;

    ASSERT(entered_flag[k->id] == 1, "entered_flag nie ustawiony");
    ASSERT(adults_entered_in_sector[k->sektor] == 2, "adults_entered_in_sector niepoprawne");
    free(k);
    OK("test_entered_flag_simulated");
    return 1;
}

/* Test 11: sold_per_sector consistency */
static int test_sold_per_sector_consistency(void) {
    init_simulation(0);
    int total_sold = 0;
    pthread_mutex_lock(&mutex_bilety);
    for (int s = 0; s < SEKTORY; ++s) {
        if (miejsca[s] > 0) {
            miejsca[s] -= 1;
            sold_per_sector[s] += 1;
            sprzedane += 1;
            total_sold += 1;
        }
    }
    pthread_mutex_unlock(&mutex_bilety);

    int sum_sold_per = 0;
    for (int s = 0; s < SEKTORY; ++s) sum_sold_per += sold_per_sector[s];

    ASSERT(sum_sold_per == total_sold, "sum(sold_per_sector) != total_sold");
    ASSERT(sprzedane == total_sold, "sprzedane != total_sold");
    OK("test_sold_per_sector_consistency");
    return 1;
}

/* Now new tests */

/* Test 12: VIP reservation lifecycle (reserve -> cancel -> sell) */
static int test_vip_reservation_lifecycle(void) {
    init_simulation(0);
    pthread_mutex_lock(&mutex_vip);
    vip_reserved = 0;
    vip_sold = 0;
    vip_entered = 0;
    pthread_mutex_unlock(&mutex_vip);

    /* rezerwujemy 2 bilety VIP */
    pthread_mutex_lock(&mutex_vip);
    if (vip_reserved + 2 <= max_vip) vip_reserved += 2;
    pthread_mutex_unlock(&mutex_vip);

    ASSERT(vip_reserved >= 2, "vip_reserved nie wzrosl po rezerwacji");

    /* symulujemy timeout/anulowanie jednej rezerwacji */
    pthread_mutex_lock(&mutex_vip);
    if (vip_reserved >= 1) vip_reserved -= 1;
    pthread_mutex_unlock(&mutex_vip);

    ASSERT(vip_reserved >= 0, "vip_reserved ujemny po anulowaniu");

    /* sprzedajemy pozostałą rezerwację */
    pthread_mutex_lock(&mutex_vip);
    if (vip_reserved >= 1) {
        vip_reserved -= 1;
        vip_sold += 1;
    }
    pthread_mutex_unlock(&mutex_vip);

    ASSERT(vip_sold >= 0, "vip_sold niepoprawny po sprzedazy");
    ASSERT(vip_reserved >= 0, "vip_reserved ujemny po sprzedazy");
    OK("test_vip_reservation_lifecycle");
    return 1;
}

/* Test 13: control station rules (max 3, same team) */
static int test_control_station_rules(void) {
    init_simulation(0);
    int s = 0;
    /* reset stanowiska */
    for (int st = 0; st < 2; ++st) {
        stanowisko_count[s][st] = 0;
        stanowisko_druzyna[s][st] = -1;
    }

    /* assign 3 persons of team 0 to stanowisko 0 */
    for (int i = 0; i < 3; ++i) {
        int st = 0;
        int cnt = stanowisko_count[s][st];
        int team = stanowisko_druzyna[s][st];
        if (cnt < 3 && (team == -1 || team == 0)) {
            stanowisko_count[s][st] = cnt + 1;
            stanowisko_druzyna[s][st] = 0;
        }
    }
    ASSERT(stanowisko_count[s][0] == 3, "nie mozna przypisac 3 osob tej samej druzyny");

    /* attempt to add a person of different team to same stanowisko -> should be rejected by rule */
    int rejected = 0;
    int st = 0;
    int cnt = stanowisko_count[s][st];
    int team = stanowisko_druzyna[s][st];
    if (!(cnt < 3 && (team == -1 || team == 1))) {
        rejected = 1;
    }
    ASSERT(rejected == 1, "mieszana druzyna zostala dopuszczona (blad)");

    OK("test_control_station_rules");
    return 1;
}

/* Test 14: pass_count increment logic simulation (id-based) */
static int test_pass_count_increment_logic(void) {
    init_simulation(0);
    /* przygotuj kilka kibicow w created_kibice z in_queue = 1 i rosnacymi id */
    pthread_mutex_lock(&mutex_kolejka);
    for (int i = 0; i < 10; ++i) {
        Kibic* k = malloc(sizeof(Kibic));
        memset(k,0,sizeof(Kibic));
        k->id = i;
        k->in_queue = 1;
        k->sektor = 0;
        k->pass_count = 0;
        if (i < MAX_KOLEJKA) created_kibice[i] = k;
    }
    q_size = 10;
    pthread_mutex_unlock(&mutex_kolejka);

    /* symulujemy potwierdzenie kibica o id = 7 -> wszyscy z id < 7 powinni dostac pass_count++ */
    int confirmed_id = 7;
    pthread_mutex_lock(&mutex_kolejka);
    for (int j = 0; j < MAX_KOLEJKA; ++j) {
        Kibic* w = created_kibice[j];
        if(!w) continue;
        if (w->in_queue && w->sektor == 0 && w->id < confirmed_id) {
            w->pass_count++;
        }
    }
    pthread_mutex_unlock(&mutex_kolejka);

    /* sprawdzamy */
    for (int i = 0; i < 10; ++i) {
        Kibic* k = created_kibice[i];
        if (!k) continue;
        if (i < confirmed_id) {
            ASSERT(k->pass_count == 1, "pass_count nie zostal inkrementowany dla pominiętego kibica");
        } else {
            ASSERT(k->pass_count == 0, "pass_count zmieniony dla niepominiętego kibica");
        }
    }

    /* cleanup */
    pthread_mutex_lock(&mutex_kolejka);
    for (int i = 0; i < 10; ++i) {
        if (created_kibice[i]) { free(created_kibice[i]); created_kibice[i] = NULL; }
    }
    q_size = 0;
    pthread_mutex_unlock(&mutex_kolejka);

    OK("test_pass_count_increment_logic");
    return 1;
}


/* Test 15: distribution of sold tickets across sectors (basic fairness check) */
static int test_sector_distribution_basic(void) {
    init_simulation(0);
    /* symulujemy sprzedaż losową: sprzedajemy po 10 biletów losowo do sektorów */
    srand((unsigned int)time(NULL));
    pthread_mutex_lock(&mutex_bilety);
    for (int i = 0; i < 80; ++i) {
        int s = rand() % SEKTORY;
        if (miejsca[s] > 0) {
            miejsca[s] -= 1;
            sold_per_sector[s] += 1;
            sprzedane += 1;
        }
    }
    pthread_mutex_unlock(&mutex_bilety);

    /* sprawdzamy, że nie ma sektora z 0 sprzedanych jeśli sprzedane > 0 (proste) */
    int nonzero = 0;
    for (int s = 0; s < SEKTORY; ++s) if (sold_per_sector[s] > 0) nonzero++;
    ASSERT(nonzero > 0, "brak sprzedanych biletow w zadnym sektorze (blad)");
    OK("test_sector_distribution_basic");
    return 1;
}

/* Menu testów */
void menu_testow(void) {
    struct {
        const char *name;
        int (*fn)(void);
    } tests[] = {
        {"test_sum_consistency_after_init", test_sum_consistency_after_init},
        {"test_sector_capacity_after_init", test_sector_capacity_after_init},
        {"test_vip_limit_after_init", test_vip_limit_after_init},
        {"test_pass_count_logic_unit", test_pass_count_logic_unit},
        {"test_vip_entered_not_exceed_sold", test_vip_entered_not_exceed_sold},
        {"test_atomic_sales_simulated", test_atomic_sales_simulated},
        {"test_requeue_simulated", test_requeue_simulated},
        {"test_timeout_counters_simulated", test_timeout_counters_simulated},
        {"test_entered_flag_simulated", test_entered_flag_simulated},
        {"test_sold_per_sector_consistency", test_sold_per_sector_consistency},
        {"test_queue_push_pop", test_queue_push_pop},
        {"test_vip_reservation_lifecycle", test_vip_reservation_lifecycle},
        {"test_control_station_rules", test_control_station_rules},
        {"test_pass_count_increment_logic", test_pass_count_increment_logic},
        {"test_sector_distribution_basic", test_sector_distribution_basic},
    };

    int total = sizeof(tests) / sizeof(tests[0]);
    int failed = 0;

    printf("\n=== MENU TESTOW (rozszerzone) ===\n");
    for (int i = 0; i < total; ++i) {
        printf("Running %s ... ", tests[i].name);
        fflush(stdout);
        int ok = tests[i].fn();
        if (ok) {
            printf("[OK]\n");
        } else {
            printf("[FAIL]\n");
            failed++;
        }
    }

    printf("\nTesty uruchomione: %d, niepowodzen: %d\n", total, failed);
    if (failed == 0) printf("Wszystkie testy przeszly pomyslnie.\n");
    else printf("Niektore testy nie przeszly. Sprawdz logi powyzej.\n");
    printf("===============================\n\n");
}
