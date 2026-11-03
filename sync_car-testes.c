/*
 * sync_car-testes.c - bateria de testes para o sync_car.c (Implementacao 4)
 *
 * Complementa o sync_car-runner.c fornecido pelo professor. Testa casos
 * normais, casos limite, casos invalidos, carga, ausencia de busy-wait e
 * rodadas aleatorias com verificacao exata do resultado.
 *
 * Uso (depois do make testes):
 *   ./sync_car_testes               roda tudo
 *   ./sync_car_testes casos         so os cenarios deterministicos (C01 a C21)
 *   ./sync_car_testes aleatorios N  so os testes aleatorios, com N sementes
 *                                   (padrao: 30 por modo)
 *
 * Cada cenario roda num processo filho com limite de tempo: se o codigo
 * travar (deadlock), o teste e marcado como FALHOU (TRAVOU) em vez de
 * travar a bateria inteira. O programa termina com codigo 0 se tudo
 * passou e 1 se algum teste falhou.
 *
 * Observacao: os testes aleatorios leem os campos da struct station
 * (mutex, espera, livres, liberados) para conferir o estado interno.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include "sync_car.h"

#define AGET(x)   __atomic_load_n(&(x), __ATOMIC_SEQ_CST)
#define AINC(x)   __atomic_add_fetch(&(x), 1, __ATOMIC_SEQ_CST)
#define ASET(x,v) __atomic_store_n(&(x), (v), __ATOMIC_SEQ_CST)
#define FAIL(...) do { printf("        -> "); printf(__VA_ARGS__); printf("\n"); return 1; } while (0)

/* ======================================================================
 * PARTE 1 - infraestrutura dos cenarios deterministicos
 * ====================================================================== */

static struct station st;
static int returned;   /* passageiros que sairam de station_wait_for_car     */
static int onb;        /* passageiros que chamaram station_on_board          */
static int delay_us;   /* tempo que cada passageiro demora para "sentar"     */

static double now_ms(void){
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

static void *pass(void *a){
    (void)a;
    station_wait_for_car(&st);
    AINC(returned);
    usleep(AGET(delay_us));
    AINC(onb);                 /* conta antes de chamar, para o carro poder conferir */
    station_on_board(&st);
    return NULL;
}

static void spawn(int n){
    for (int i = 0; i < n; i++) {
        pthread_t t;
        pthread_attr_t at;
        pthread_attr_init(&at);
        pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
        if (pthread_create(&t, &at, pass, NULL)) { perror("pthread_create"); exit(2); }
    }
}

struct car { int count; double elapsed; int ret; };

static void *car_t(void *a){
    struct car *c = a;
    double t = now_ms();
    station_load_car(&st, c->count);
    c->elapsed = now_ms() - t;
    ASET(c->ret, 1);
    return NULL;
}

static void start_car(struct car *c, int count){
    pthread_t t;
    c->count = count; c->ret = 0; c->elapsed = 0;
    pthread_create(&t, NULL, car_t, c);
    pthread_detach(t);
}

static int wait_car(struct car *c, int ms){
    double t = now_ms();
    while (now_ms() - t < ms) {
        if (AGET(c->ret)) return 1;
        usleep(1000);
    }
    return AGET(c->ret);
}

static void settle(void){ usleep(250000); }   /* deixa as threads chegarem a espera */

static double cpu(void){
    struct rusage r;
    getrusage(RUSAGE_SELF, &r);
    return r.ru_utime.tv_sec + r.ru_stime.tv_sec +
           (r.ru_utime.tv_usec + r.ru_stime.tv_usec) / 1e6;
}

/* ======================================================================
 * PARTE 2 - cenarios deterministicos (C01 a C21)
 * ====================================================================== */

/* C01 */
static int c01(void){
    double t = now_ms();
    station_load_car(&st, 0);
    station_load_car(&st, 10);
    station_load_car(&st, 1000000);
    if (now_ms() - t > 100) FAIL("load_car demorou %.0f ms sem passageiros", now_ms() - t);
    return 0;
}

/* C02 */
static int c02(void){
    spawn(5); settle();
    double t = now_ms();
    station_load_car(&st, 0);
    if (now_ms() - t > 100) FAIL("load_car(0) nao retornou na hora (%.0f ms)", now_ms() - t);
    settle();
    if (AGET(returned) != 0) FAIL("%d passageiros embarcaram num carro com 0 assentos", AGET(returned));
    struct car c; start_car(&c, 5);
    if (!wait_car(&c, 3000)) FAIL("carro seguinte (5 assentos, 5 na fila) nao retornou");
    if (AGET(returned) != 5 || AGET(onb) != 5) FAIL("esperava 5 embarcados, veio %d", AGET(returned));
    return 0;
}

/* C03 */
static int c03(void){
    spawn(3); settle();
    struct car c; start_car(&c, 10);
    if (!wait_car(&c, 3000)) FAIL("load_car nao retornou (10 assentos, 3 na fila)");
    if (AGET(returned) != 3) FAIL("embarcaram %d, esperava 3", AGET(returned));
    if (AGET(onb) != 3) FAIL("load_car retornou antes de todos chamarem on_board (onb=%d)", AGET(onb));
    spawn(1); settle();
    if (AGET(returned) != 3) FAIL("passageiro que chegou depois da partida embarcou num carro que ja saiu");
    struct car c2; start_car(&c2, 1);
    if (!wait_car(&c2, 3000) || AGET(returned) != 4) FAIL("proximo carro nao levou o passageiro retardatario");
    return 0;
}

/* C04 */
static int c04(void){
    spawn(10); settle();
    struct car c; start_car(&c, 4);
    if (!wait_car(&c, 3000)) FAIL("load_car nao retornou (4 assentos, 10 na fila)");
    if (AGET(returned) != 4) FAIL("embarcaram %d, esperava exatamente 4", AGET(returned));
    if (AGET(onb) != 4) FAIL("retornou antes dos 4 chamarem on_board");
    settle();
    if (AGET(returned) != 4) FAIL("passageiros extras entraram depois da partida (%d)", AGET(returned));
    struct car c2; start_car(&c2, 6);
    if (!wait_car(&c2, 3000) || AGET(returned) != 10) FAIL("segundo carro nao levou os 6 restantes (returned=%d)", AGET(returned));
    return 0;
}

/* C05 */
static int c05(void){
    ASET(delay_us, 400000); spawn(3); settle();
    struct car c; start_car(&c, 3);
    if (!wait_car(&c, 5000)) FAIL("load_car nao retornou");
    if (c.elapsed < 350) FAIL("load_car retornou em %.0f ms, antes dos passageiros sentarem", c.elapsed);
    return 0;
}

/* C06 */
static int c06(void){
    ASET(delay_us, 600000); spawn(2); settle();
    struct car c; start_car(&c, 5);
    usleep(150000); spawn(1);
    if (!wait_car(&c, 5000)) FAIL("load_car travou com passageiro chegando durante o embarque");
    int r = AGET(returned);
    if (r > 5) FAIL("mais passageiros que assentos: %d", r);
    if (AGET(onb) != r) FAIL("load_car retornou sem todos os embarcados chamarem on_board");
    printf("        (info) passageiro que chegou com o carro na estacao e assentos livres: %s\n",
           r == 3 ? "embarcou" : "esperou o proximo");
    return 0;
}

/* C07 - INVALIDO */
static int c07(void){
    spawn(3); settle();
    struct car c; start_car(&c, -1);
    int ok = wait_car(&c, 2000);
    int r = AGET(returned);
    if (r > 0) FAIL("%d passageiros 'embarcaram' num carro com -1 assentos", r);
    if (!ok) FAIL("load_car(-1) travou (deadlock) com passageiros na fila");
    return 0;
}

/* C08 - INVALIDO */
static int c08(void){
    double t = now_ms();
    station_load_car(&st, -5);
    if (now_ms() - t > 100) FAIL("demorou %.0f ms", now_ms() - t);
    spawn(1); settle();
    if (AGET(returned) != 0) FAIL("passageiro embarcou depois de carro com assentos negativos");
    return 0;
}

/* C09 */
static int c09(void){
    spawn(100); settle();
    struct car c; start_car(&c, INT_MAX);
    if (!wait_car(&c, 5000) || AGET(returned) != 100) FAIL("INT_MAX assentos: returned=%d", AGET(returned));
    return 0;
}

/* C10 */
static int c10(void){
    spawn(2000); usleep(800000);
    struct car c; start_car(&c, 2000);
    if (!wait_car(&c, 15000)) FAIL("travou com 2000 passageiros");
    if (AGET(returned) != 2000 || AGET(onb) != 2000) FAIL("returned=%d onb=%d", AGET(returned), AGET(onb));
    return 0;
}

/* C11 */
static int c11(void){
    double c0 = cpu();
    spawn(50); usleep(2000000);
    double c1 = cpu() - c0;
    if (c1 > 0.3) FAIL("50 passageiros bloqueados por 2s gastaram %.2fs de CPU (busy-wait?)", c1);
    ASET(delay_us, 1500000);
    struct car c; start_car(&c, 50); usleep(2200000);
    double c2 = cpu() - c0 - c1;
    if (c2 > 0.3) FAIL("carro esperando on_board por 2s gastou %.2fs de CPU", c2);
    printf("        (info) CPU: %.3fs com passageiros bloqueados, %.3fs com o carro esperando\n", c1, c2);
    return 0;
}

/* C12 */
static int c12(void){
    ASET(delay_us, 0);
    int total = 600, boarded = 0;
    spawn(total);
    double t = now_ms();
    while (boarded < total && now_ms() - t < 20000) {
        struct car c; start_car(&c, 1 + rand() % 7);
        if (!wait_car(&c, 3000)) FAIL("carro travou (boarded=%d)", boarded);
        boarded = AGET(returned);
    }
    if (boarded != total) FAIL("so %d de %d embarcaram", boarded, total);
    return 0;
}

/* C13 - INVALIDO (violacao do contrato: on_board sem passageiro liberado) */
static int c13(void){
    spawn(2); settle();
    station_on_board(&st);
    struct car c; start_car(&c, 2);
    if (!wait_car(&c, 2000)) FAIL("load_car travou apos on_board chamado a mais");
    return 0;
}

/* C14 - INVALIDO */
static int c14(void){
    spawn(5); settle();
    struct car c; start_car(&c, INT_MIN);
    if (!wait_car(&c, 2000)) FAIL("load_car(INT_MIN) travou");
    settle();
    if (AGET(returned) != 0) FAIL("%d embarcaram com INT_MIN assentos", AGET(returned));
    struct car c2; start_car(&c2, 5);
    if (!wait_car(&c2, 3000) || AGET(returned) != 5) FAIL("carro normal depois do invalido nao levou os 5 (returned=%d)", AGET(returned));
    return 0;
}

/* C15 - INVALIDO */
static int c15(void){
    spawn(20); settle();
    int vals[] = {0, -1, -100, INT_MIN, 0, -7};
    double t0 = now_ms();
    for (int i = 0; i < 1500; i++) station_load_car(&st, vals[i % 6]);
    double el = now_ms() - t0;
    if (el > 3000) FAIL("1500 carros invalidos demoraram %.0f ms", el);
    if (AGET(returned) != 0) FAIL("alguem embarcou num carro invalido (returned=%d)", AGET(returned));
    struct car c; start_car(&c, 20);
    if (!wait_car(&c, 3000) || AGET(returned) != 20) FAIL("fila de 20 nao embarcou no carro valido (returned=%d)", AGET(returned));
    return 0;
}

/* C16 */
static int c16(void){
    spawn(50); settle();
    for (int i = 1; i <= 50; i++) {
        struct car c; start_car(&c, 1);
        if (!wait_car(&c, 3000)) FAIL("carro %d travou", i);
        if (AGET(returned) != i) FAIL("apos carro %d embarcaram %d (esperado %d)", i, AGET(returned), i);
    }
    return 0;
}

/* C17 */
static void *stream(void *a){
    (void)a;
    for (int i = 0; i < 150; i++) { spawn(1); usleep(2000); }
    return NULL;
}
static int c17(void){
    pthread_t t;
    struct car c;
    ASET(delay_us, 450000);
    spawn(1); settle();                  /* 1 passageiro segura o carro na estacao */
    start_car(&c, 1000);
    pthread_create(&t, NULL, stream, NULL);
    pthread_join(t, NULL);
    if (!wait_car(&c, 4000)) FAIL("carro nao saiu apos o fim do fluxo de chegadas");
    int rt = AGET(returned);
    if (AGET(onb) != rt) FAIL("saiu sem on_board de todos (returned=%d onb=%d)", rt, AGET(onb));
    printf("        (info) %d de 150 passageiros que chegaram durante o embarque entraram nesse carro\n", rt - 1);
    struct car c2; start_car(&c2, 1000);
    if (!wait_car(&c2, 4000)) FAIL("segundo carro travou");
    if (AGET(returned) != 151) FAIL("no total embarcaram %d de 151", AGET(returned));
    return 0;
}

/* C18 */
static int c18(void){
    spawn(5000); usleep(1500000);
    struct car c; start_car(&c, 5000);
    if (!wait_car(&c, 20000)) FAIL("travou com 5000 passageiros");
    if (AGET(returned) != 5000 || AGET(onb) != 5000) FAIL("returned=%d onb=%d", AGET(returned), AGET(onb));
    return 0;
}

/* C19 - INVALIDO */
static int c19(void){
    ASET(delay_us, 300000); spawn(3); settle();
    for (int i = 0; i < 1000; i++) station_on_board(&st);
    struct car c; start_car(&c, 3);
    if (!wait_car(&c, 5000)) FAIL("travou");
    if (AGET(returned) != 3) FAIL("embarcaram %d", AGET(returned));
    if (c.elapsed < 250) FAIL("carro saiu em %.0f ms: on_board extras adiantaram a partida", c.elapsed);
    return 0;
}

/* C20 */
static int c20(void){
    spawn(100); settle();
    int seq[] = {30, 30, 30, 10}, acc = 0;
    for (int i = 0; i < 4; i++) {
        struct car c; start_car(&c, seq[i]);
        if (!wait_car(&c, 3000)) FAIL("carro %d travou", i);
        acc += seq[i];
        if (AGET(returned) != acc) FAIL("apos carro %d: %d (esperado %d)", i, AGET(returned), acc);
    }
    return 0;
}

/* C21 */
static int c21(void){
    int exp = 0;
    for (int i = 0; i < 300; i++) {
        station_load_car(&st, 3);          /* ninguem na fila: sai na hora */
        spawn(2); usleep(3000);
        struct car c; start_car(&c, 3);
        if (!wait_car(&c, 3000)) FAIL("volta %d travou", i);
        exp += 2;
        if (AGET(returned) != exp) {
            usleep(20000);
            if (AGET(returned) != exp) FAIL("volta %d: returned=%d esperado=%d", i, AGET(returned), exp);
        }
    }
    return 0;
}

struct cenario { const char *id; const char *nome; int (*fn)(void); };

static const struct cenario cenarios[] = {
    {"C01", "sem passageiros: load_car retorna na hora", c01},
    {"C02", "carro com 0 assentos e fila: retorna e ninguem embarca", c02},
    {"C03", "mais assentos que fila: leva todos, espera on_board, retardatario espera", c03},
    {"C04", "menos assentos que fila: leva exatamente count, o resto fica", c04},
    {"C05", "load_car so retorna depois dos on_board", c05},
    {"C06", "passageiro chega durante o embarque", c06},
    {"C07", "INVALIDO: count negativo com fila", c07},
    {"C08", "INVALIDO: count negativo sem fila", c08},
    {"C09", "limite: INT_MAX assentos", c09},
    {"C10", "carga: 2000 passageiros", c10},
    {"C11", "sem busy-wait (consumo de CPU)", c11},
    {"C12", "600 passageiros, carros aleatorios em sequencia", c12},
    {"C13", "INVALIDO: on_board chamado a mais (violacao de contrato)", c13},
    {"C14", "INVALIDO: count = INT_MIN", c14},
    {"C15", "INVALIDO: 1500 carros com count 0/negativo e fila de 20", c15},
    {"C16", "50 carros de 1 assento, 50 passageiros: 1 por carro", c16},
    {"C17", "fluxo continuo de chegadas durante o embarque", c17},
    {"C18", "carga: 5000 passageiros", c18},
    {"C19", "INVALIDO: on_board extra x1000 nao adianta a partida", c19},
    {"C20", "sequencia exata 30,30,30,10 com 100 na fila", c20},
    {"C21", "300 idas e voltas (carro vazio / 2 passageiros)", c21},
};

static int roda_cenarios(void){
    int n = (int)(sizeof cenarios / sizeof cenarios[0]), falhas = 0;
    printf("=== CENARIOS DETERMINISTICOS (%d) ===\n", n);
    for (int i = 0; i < n; i++) {
        printf("[%s] %s\n", cenarios[i].id, cenarios[i].nome);
        pid_t p = fork();
        if (p == 0) {
            alarm(40);
            memset(&st, 0, sizeof st);
            station_init(&st);
            srand(7);
            _exit(cenarios[i].fn());
        }
        int s;
        waitpid(p, &s, 0);
        if (WIFEXITED(s) && WEXITSTATUS(s) == 0) {
            printf("        PASSOU\n");
        } else if (WIFSIGNALED(s) && WTERMSIG(s) == SIGALRM) {
            printf("        FALHOU (TRAVOU / timeout)\n"); falhas++;
        } else {
            printf("        FALHOU\n"); falhas++;
        }
    }
    printf("--- cenarios: %d de %d passaram\n\n", n - falhas, n);
    return falhas;
}

/* ======================================================================
 * PARTE 3 - testes aleatorios com verificacao exata
 *   modo 0 (lote):  passageiros chegam em lotes entre um carro e outro;
 *                   confere exatamente min(assentos, fila) embarcados.
 *   modo 1 (caos):  passageiros chegam enquanto os carros entram e saem;
 *                   confere as invariantes de seguranca.
 * ====================================================================== */

static struct station ast;
static int al_car_active, al_cur_seats, al_cur_boarded, al_cur_onb, al_total_returned, al_errors;
static unsigned al_seed_g;

#define ERR(...) do { AINC(al_errors); fprintf(stderr, "ERRO: "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while (0)

static int rnd(unsigned *s, int n){ return rand_r(s) % n; }

static void *al_pass(void *a){
    unsigned s = (unsigned)(size_t)a;
    if (rnd(&s, 3) == 0) usleep(rnd(&s, 300));
    station_wait_for_car(&ast);
    if (!AGET(al_car_active)) ERR("passageiro saiu de wait_for_car sem carro na estacao");
    int b = AINC(al_cur_boarded);
    if (b > AGET(al_cur_seats)) ERR("mais passageiros (%d) que assentos (%d)", b, AGET(al_cur_seats));
    AINC(al_total_returned);
    if (rnd(&s, 2)) usleep(rnd(&s, 200));
    AINC(al_cur_onb);
    station_on_board(&ast);
    return NULL;
}

static void al_spawn(int n, unsigned *s){
    for (int i = 0; i < n; i++) {
        pthread_t t;
        pthread_attr_t at;
        pthread_attr_init(&at);
        pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
        if (pthread_create(&t, &at, al_pass, (void *)(size_t)rand_r(s))) { perror("pthread_create"); exit(2); }
    }
}

static int al_waiting_now(void){
    pthread_mutex_lock(&ast.mutex);
    int e = ast.espera;
    pthread_mutex_unlock(&ast.mutex);
    return e;
}

static void al_alarm(int x){
    (void)x;
    fprintf(stderr, "TRAVOU (deadlock/timeout) semente=%u\n", al_seed_g);
    _exit(3);
}

static void al_run_car(int count, int *boarded){
    ASET(al_cur_seats, count); ASET(al_cur_boarded, 0); ASET(al_cur_onb, 0); ASET(al_car_active, 1);
    station_load_car(&ast, count);
    int b = AGET(al_cur_boarded), o = AGET(al_cur_onb);
    ASET(al_car_active, 0);
    if (o != b) ERR("carro saiu com %d embarcados mas so %d chamaram on_board", b, o);
    if (count >= 0 && b > count) ERR("embarcaram %d > %d assentos", b, count);
    *boarded = b;
}

static int al_batch(unsigned seed, int rounds){
    unsigned s = seed;
    int waiting = 0;
    for (int r = 0; r < rounds; r++) {
        int n = rnd(&s, 25);
        al_spawn(n, &s); waiting += n;
        for (int w = 0; w < 5000 && al_waiting_now() != waiting; w++) usleep(200);
        if (al_waiting_now() != waiting) { ERR("fila esperada %d, real %d", waiting, al_waiting_now()); return 1; }
        int count = rnd(&s, 4) == 0 ? 0 : rnd(&s, 40);
        int b;
        al_run_car(count, &b);
        int expect = count < waiting ? count : waiting;
        if (b != expect) ERR("rodada %d: fila=%d assentos=%d -> embarcaram %d (esperado %d)", r, waiting, count, b, expect);
        waiting -= b;
        if (al_waiting_now() != waiting) ERR("rodada %d: fila depois=%d esperado=%d", r, al_waiting_now(), waiting);
        pthread_mutex_lock(&ast.mutex);
        if (ast.livres != 0 || ast.liberados != 0) ERR("estado sujo apos carro: livres=%d liberados=%d", ast.livres, ast.liberados);
        pthread_mutex_unlock(&ast.mutex);
    }
    while (waiting > 0) {
        int b;
        al_run_car(waiting, &b);
        waiting -= b;
        if (!b) { ERR("carro com assentos suficientes nao levou ninguem"); break; }
    }
    return AGET(al_errors);
}

static int al_chaos(unsigned seed, int passengers){
    unsigned s = seed;
    al_spawn(passengers, &s);
    int guard = 0;
    while (AGET(al_total_returned) < passengers && guard++ < 200000) {
        int b;
        al_run_car(rnd(&s, 4) == 0 ? 0 : 1 + rnd(&s, 30), &b);
        if (rnd(&s, 2)) usleep(rnd(&s, 300));
    }
    if (AGET(al_total_returned) != passengers) ERR("so %d de %d embarcaram", AGET(al_total_returned), passengers);
    usleep(20000);
    return AGET(al_errors);
}

static int roda_aleatorios(int nseeds){
    const char *nome[2] = {"lote (chegadas em lotes, verificacao exata)", "caos (chegadas simultaneas, invariantes)"};
    int falhas_total = 0;
    printf("=== TESTES ALEATORIOS (%d sementes por modo) ===\n", nseeds);
    for (int modo = 0; modo < 2; modo++) {
        int falhas = 0;
        for (int seed = 1; seed <= nseeds; seed++) {
            pid_t p = fork();
            if (p == 0) {
                al_seed_g = (unsigned)seed;
                signal(SIGALRM, al_alarm);
                alarm(40);
                memset(&ast, 0, sizeof ast);
                station_init(&ast);
                int r = (modo == 0) ? al_batch((unsigned)seed, 60) : al_chaos((unsigned)seed, 400);
                _exit(r ? 1 : 0);
            }
            int s;
            waitpid(p, &s, 0);
            if (!(WIFEXITED(s) && WEXITSTATUS(s) == 0)) {
                printf("        FALHOU: modo %s, semente %d\n", modo == 0 ? "lote" : "caos", seed);
                falhas++;
            }
        }
        printf("[A%d] %s: %d de %d sementes passaram\n", modo + 1, nome[modo], nseeds - falhas, nseeds);
        falhas_total += falhas;
    }
    printf("--- aleatorios: %d falha(s)\n\n", falhas_total);
    return falhas_total;
}

/* ====================================================================== */

int main(int argc, char **argv){
    setvbuf(stdout, NULL, _IONBF, 0);
    int fazer_casos = 1, fazer_aleatorios = 1, nseeds = 30;
    if (argc > 1) {
        if (strcmp(argv[1], "casos") == 0) fazer_aleatorios = 0;
        else if (strcmp(argv[1], "aleatorios") == 0) { fazer_casos = 0; if (argc > 2) nseeds = atoi(argv[2]); }
        else { fprintf(stderr, "uso: %s [casos | aleatorios [N]]\n", argv[0]); return 2; }
    }
    if (nseeds < 1) nseeds = 1;

    int falhas = 0;
    if (fazer_casos) falhas += roda_cenarios();
    if (fazer_aleatorios) falhas += roda_aleatorios(nseeds);

    if (falhas) printf("RESULTADO FINAL: FALHOU (%d problema(s))\n", falhas);
    else        printf("RESULTADO FINAL: TODOS OS TESTES PASSARAM\n");
    return falhas != 0;
}
