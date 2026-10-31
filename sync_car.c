#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "sync_car.h"

void station_init(struct station *station){
    station->espera = 0;
    station->liberados = 0;
    station->livres = 0;
    pthread_mutex_init(&station->mutex, NULL);
    pthread_cond_init(&station->passageiros, NULL);
    pthread_cond_init(&station->vagao, NULL);
}

void station_wait_for_car(struct station *station){

    pthread_mutex_lock(&station->mutex);
    station->espera++;

    while(station->livres <= 0){
        pthread_cond_wait(&station->passageiros, &station->mutex);
    }

    station->espera--;
    station->livres--;
    station->liberados++;

    pthread_mutex_unlock(&station->mutex);
}

void station_on_board(struct station *station){

    pthread_mutex_lock(&station->mutex);
    if(station->liberados > 0){
        station->liberados--;
    }

    pthread_cond_signal(&station->vagao);

    pthread_mutex_unlock(&station->mutex);
}

void station_load_car(struct station *station, int count){

    pthread_mutex_lock(&station->mutex);
    station->livres = count;

    pthread_cond_broadcast(&station->passageiros);

    while(station->liberados > 0 || (station->livres > 0 && station->espera > 0)){
        pthread_cond_wait(&station->vagao, &station->mutex);
    }

    station->livres = 0;

    pthread_mutex_unlock(&station->mutex);
}
