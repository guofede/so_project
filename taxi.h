#ifndef _TAXI_H
#define _TAXI_H
typedef struct{
	int pid;
	int pos;
	int celle_percorse;
	int celle_percorsemax;
	long tnow;
	long tmax;
	int n_richieste;
	int n_richiestemax;
}taxi_stats;



#endif
