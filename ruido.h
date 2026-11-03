/*
 * ruido.h - injeta "ruido de escalonamento" no sync_car.c SEM alterar o arquivo.
 *
 * Antes e depois de cada pthread_mutex_lock/unlock, pthread_cond_wait/signal/
 * broadcast, a thread faz sched_yield() ou dorme alguns microssegundos de forma
 * aleatoria. Isso forca trocas de thread em pontos "ruins" e ajuda a expor
 * condicoes de corrida que raramente aparecem numa maquina com poucos nucleos.
 *
 * Uso (ver Makefile, alvo testes-ruido):
 *   gcc -pthread -include ruido.h -c sync_car.c -o sync_car_ruido.o
 */
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

static __thread unsigned ruido_seed;

static inline void ruido_pt(void){
    if (!ruido_seed) ruido_seed = (unsigned)(size_t)&ruido_seed ^ (unsigned)time(NULL) ^ (unsigned)getpid();
    unsigned r = rand_r(&ruido_seed) % 10;
    if (r == 0) sched_yield();
    else if (r == 1) usleep(rand_r(&ruido_seed) % 150);
    else if (r == 2) { sched_yield(); sched_yield(); }
}

static inline int r_lock(pthread_mutex_t *m)   { ruido_pt(); int r = pthread_mutex_lock(m);   ruido_pt(); return r; }
static inline int r_unlock(pthread_mutex_t *m) { ruido_pt(); int r = pthread_mutex_unlock(m); ruido_pt(); return r; }
static inline int r_wait(pthread_cond_t *c, pthread_mutex_t *m) { ruido_pt(); int r = pthread_cond_wait(c, m); ruido_pt(); return r; }
static inline int r_signal(pthread_cond_t *c)  { ruido_pt(); int r = pthread_cond_signal(c);  ruido_pt(); return r; }
static inline int r_bcast(pthread_cond_t *c)   { ruido_pt(); int r = pthread_cond_broadcast(c); ruido_pt(); return r; }

#define pthread_mutex_lock      r_lock
#define pthread_mutex_unlock    r_unlock
#define pthread_cond_wait       r_wait
#define pthread_cond_signal     r_signal
#define pthread_cond_broadcast  r_bcast
