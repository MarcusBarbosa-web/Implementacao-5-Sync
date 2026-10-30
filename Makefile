CC = gcc
CFLAGS = -Wall -pthread

all: sync_car

sync_car: sync_car.c sync_car-runner.c sync_car.h
	$(CC) $(CFLAGS) -o sync_car sync_car.c sync_car-runner.c

clean:
	rm -f sync_car

test: sync_car
	./sync_car

.PHONY: all clean test