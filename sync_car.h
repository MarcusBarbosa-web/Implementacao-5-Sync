#ifndef SYNC_CAR_H
#define SYNC_CAR_H
   
#include <pthread.h>

typedef struct station{
    int espera;
    int liberados;
    int livres;
    pthread_mutex_t mutex;
    pthread_cond_t passageiros;
    pthread_cond_t vagao;
}station;

void station_init(struct station *station);
void station_wait_for_car(struct station *station);
void station_on_board(struct station *station);
void station_load_car(struct station *station, int count);

#endif