CC = gcc
CFLAGS = -Wall -pthread

all: sync_car

sync_car: sync_car.c sync_car-runner.c sync_car.h
	$(CC) $(CFLAGS) -o sync_car sync_car.c sync_car-runner.c

test: sync_car
	./sync_car

sync_car_testes: sync_car-testes.c sync_car.c sync_car.h
	$(CC) $(CFLAGS) -o sync_car_testes sync_car-testes.c sync_car.c

testes: sync_car_testes
	./sync_car_testes

sync_car_ruido.o: sync_car.c sync_car.h ruido.h
	$(CC) $(CFLAGS) -include ruido.h -c sync_car.c -o sync_car_ruido.o

testes_ruido: sync_car-testes.c sync_car_ruido.o sync_car.h
	$(CC) $(CFLAGS) -o testes_ruido sync_car-testes.c sync_car_ruido.o

testes-ruido: testes_ruido
	./testes_ruido

clean:
	rm -f sync_car sync_car_testes testes_ruido sync_car_ruido.o

.PHONY: all test testes testes-ruido clean