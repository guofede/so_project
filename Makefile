
CC = gcc
CFLAGS = -std=c89 -Wpedantic 


main : mappa.o main.c
	

taxi : mappa.o taxi.o
	

source : mappa.o source.o
	

run :
	./main

taxigame : main.c mappa.c taxi.c source.c
	$(CC) $(CFLAGS) -DSO_WIDTH=$(WIDTH) -DSO_HEIGHT=$(HEIGHT) -c mappa.c
	$(CC) mappa.o main.c $(CFLAGS) -DSO_WIDTH=$(WIDTH) -DSO_HEIGHT=$(HEIGHT) -o main
	$(CC) mappa.o taxi.c $(CFLAGS) -DSO_WIDTH=$(WIDTH) -DSO_HEIGHT=$(HEIGHT) -o taxi
	$(CC) mappa.o source.c $(CFLAGS) -DSO_WIDTH=$(WIDTH) -DSO_HEIGHT=$(HEIGHT) -o source

clean :
	rm main
	rm taxi
	rm source
