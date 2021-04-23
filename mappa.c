#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "mappa.h"

/*ritorna 0 se la generazione della mappa è riuscita -1 altrimenti*/
int map_gen(cella *map,const int celle_sem,const int SO_HOLES,const long SO_CAPTMIN,const long SO_CAPTMAX,const int SO_CAPMIN,const int SO_CAPMAX){

	int i,candidate,accepted,count=0;
	int seminit_val;

	/*inizializzo celle */
	for(i = 0; i<SO_WIDTH*SO_HEIGHT; i++){
		map[i].tempo = ((rand() % (SO_CAPTMAX-SO_CAPTMIN+1)) + SO_CAPTMIN);
		seminit_val = map[i].cap = ((rand() % (SO_CAPMAX-SO_CAPMIN+1)) + SO_CAPMIN);
		if(semctl(celle_sem,i,SETVAL,seminit_val)==-1)
			switch(errno){
				case ERANGE :
					perror("Parametri SO_CAPMIN e SO_CAPMAX troppo grandi(limite 0 - SEMVMX)");
					return -1;
				case EINVAL :
					perror("semaforo o parametro non valido");
					return -1;
				case EIDRM :
					perror("celle_sem è stato rimosso");
					return -1;
			}
	}
	/*genero holes*/
	for(i = 0; i < SO_HOLES && count < 10000;){
		candidate = rand()%(SO_WIDTH*SO_HEIGHT);

		if(map[candidate].is_hole)
			accepted = 0;
		else if((candidate%SO_WIDTH == 0)&&(candidate<SO_WIDTH)) /*check angolo ALTO SINSITRA*/
			accepted = !(map[candidate+1].is_hole||map[candidate+SO_WIDTH].is_hole||map[candidate+SO_WIDTH+1].is_hole);
		else if((candidate%SO_WIDTH == SO_WIDTH-1)&&(candidate<SO_WIDTH))/*check angolo ALTO DESTRA*/
			accepted = !(map[candidate-1].is_hole||map[candidate+SO_WIDTH].is_hole||map[candidate+SO_WIDTH-1].is_hole);
		else if((candidate%SO_WIDTH == 0)&&(candidate>(SO_WIDTH*SO_HEIGHT-SO_WIDTH)))/*check angolo BASSO SINISTRA*/
			accepted = !(map[candidate+1].is_hole||map[candidate-SO_WIDTH].is_hole||map[candidate-SO_WIDTH+1].is_hole);
		else if((candidate>(SO_WIDTH*SO_HEIGHT-SO_WIDTH))&&(candidate%SO_WIDTH == SO_WIDTH-1))/*check angolo BASS0 DESTRA*/
			accepted = !(map[candidate-1].is_hole||map[candidate+SO_WIDTH].is_hole||map[candidate+SO_WIDTH-1].is_hole);
		else if(candidate<SO_WIDTH)/*caso SU*/
			accepted = !(map[candidate-1].is_hole||map[candidate+1].is_hole||map[candidate+SO_WIDTH-1].is_hole
					||map[candidate+SO_WIDTH].is_hole||map[candidate+SO_WIDTH+1].is_hole);
		else if((candidate%SO_WIDTH == SO_WIDTH-1))/*caso GIU*/
			accepted = !(map[candidate-1].is_hole||map[candidate+1].is_hole||map[candidate-SO_WIDTH-1].is_hole
					||map[candidate-SO_WIDTH].is_hole||map[candidate-SO_WIDTH+1].is_hole);
		else if(candidate%SO_WIDTH == 0)/*caso SINISTRA*/
			accepted = !(map[candidate-SO_WIDTH].is_hole||map[candidate-SO_WIDTH+1].is_hole||map[candidate+1].is_hole
					||map[candidate+SO_WIDTH].is_hole||map[candidate+SO_WIDTH+1].is_hole);
		else if(candidate%SO_WIDTH == SO_WIDTH-1)/*caso DESTRA*/
			accepted = !(map[candidate-SO_WIDTH].is_hole||map[candidate-SO_WIDTH-1].is_hole||map[candidate-1].is_hole
					||map[candidate+SO_WIDTH-1].is_hole||map[candidate+SO_WIDTH].is_hole);
		else
			accepted = !(map[candidate-SO_WIDTH].is_hole||map[candidate-SO_WIDTH+1].is_hole||map[candidate+1].is_hole
					||map[candidate+SO_WIDTH+1].is_hole||map[candidate+SO_WIDTH].is_hole||map[candidate+SO_WIDTH-1].is_hole
					||map[candidate-1].is_hole||map[candidate-SO_WIDTH-1].is_hole);


		if(accepted){
			map[candidate].is_hole = 1;
			i++;
		}else{
			count++;
		}
    	}
	if(count==10000)
		printf("generazione incompleta N_HOLES : %d\n",i+1);
	return 0;
}

int getX(cella *map, int pos){
	return pos%SO_WIDTH;
}
int getY(cella *map, int pos){
	return pos/SO_WIDTH;
}

void map_print(cella *map, int celle_sem){
	int i;
	unsigned short resourceleft[SO_WIDTH*SO_HEIGHT];

	if(semctl(celle_sem, 0, GETALL, resourceleft)==-1){
		perror("print map celle_sem");
		return;
	}

	printf("\n\n\n");
	for(i=0; i<SO_WIDTH*SO_HEIGHT; i++){
		if(map[i].is_hole)
			printf("%c	", 'H');
		else
			printf("%u	", map[i].cap - resourceleft[i]);

		if(i%SO_WIDTH == SO_WIDTH-1)
			printf("\n\n");
	}
}



int traversable(cella *map, const int pos){
	if(pos>=(SO_WIDTH*SO_HEIGHT || map == NULL))
		return 0;
	else return
		!map[pos].is_hole;
}


int random_transable_pos(cella *map){
	int pos;
	pos = rand()%(SO_WIDTH*SO_HEIGHT);
	if(traversable(map,pos))
		return pos;
	else if(traversable(map,pos+1))
		return pos+1;
	else
		return pos-1;
}


