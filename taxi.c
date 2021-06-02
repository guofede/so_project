#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include "mappa.h"
#include "taxi.h"

#define TERMINATE {shmdt(map); exit(0);}

#define PRINT_TERMINATE	 		{fprintf(stderr, \
					"%s:%d PID=%5d : Error %d(%s)"\
					,__FILE__,__LINE__,getpid(),errno,strerror(errno));\
					shmdt(map); exit(0);\
					}


int moveHoriz(cella *map,const int celle_sem,const int sc_sem,const int dest);

int moveVert(cella *map,const int celle_sem,const int sc_sem,const int dest);

int moveTo(const char *dest_name,cella *map,const int celle_sem,const int sc_sem, const int dest);

int getresource(int semid, int semnum);

int releaseresource(int semid, int semnum);

int transition(int celle_sem, int pos, int newpos);


int SO_TIMEOUT;

taxi_stats *taxi;


int main(int argc, char *argv[]){
	int msg_id,sync_sem, celle_sem, sc_sem, shmap_id;
	struct messaggio msgbuf;
	cella *map;
	struct sembuf start, stats;
	struct timespec test={1,0};
	richieste_stats *r_stats;


	if(argc<8)
		exit(EXIT_FAILURE);

	shmap_id = atoi(argv[1]);
	celle_sem = atoi(argv[2]);
	sc_sem = atoi(argv[3]);
	sync_sem = atoi(argv[4]);
	msg_id = atoi(argv[5]);
	SO_TIMEOUT = atoi(argv[6]);

	map = shmat(shmap_id,NULL,0);
	r_stats = (richieste_stats *) (map + (SO_WIDTH*SO_HEIGHT));
	taxi = ((taxi_stats *) (r_stats+1)) + atoi(argv[7]);
	taxi->pid = getpid();
	srand(getpid());
	taxi->pos = random_transable_pos(map);
	taxi->celle_percorse = 0;
	taxi->tnow = 0;
	taxi->n_richieste = 0;	
	
	/*ASPETTO MAIN*/
	start.sem_num=0;
	start.sem_op=0;
	start.sem_flg=0;
	if(semop(sync_sem, &start, 1)==-1)
		TERMINATE

	/*MI POSIZIONO IN POS*/

	if(getresource(celle_sem, taxi->pos)==-1)
		TERMINATE

	if(getresource(sc_sem, taxi->pos)==-1)
		PRINT_TERMINATE

	map[taxi->pos].n_attr++;

	if(releaseresource(sc_sem, taxi->pos)==-1)
		PRINT_TERMINATE

	while(1){
		if(msgrcv(msg_id,&msgbuf,sizeof(msgbuf.richiesta),0,0)==-1)
			switch(errno){
				case EINVAL :
				case EIDRM :
					TERMINATE
				default :
					PRINT_TERMINATE
			}
		taxi->n_richieste++;
		if(taxi->n_richieste > taxi->n_richiestemax)
			taxi->n_richiestemax = taxi->n_richieste;
		
		if(moveTo("SOURCE", map, celle_sem, sc_sem, msgbuf.richiesta[0])==-1)
			switch(errno){
				case EAGAIN :
					shmdt(map);
					execvp("./taxi",argv);
				case EINVAL :
				case EIDRM :
					TERMINATE
				default :
					PRINT_TERMINATE
			}
			/*SONO IN SOURCE POSSO EVADERE IL VIAGGIO*/
			
		if(getresource(sc_sem, SO_WIDTH*SO_HEIGHT)==-1)
				PRINT_TERMINATE

		r_stats->inevasi--;

		if(releaseresource(sc_sem, SO_WIDTH*SO_HEIGHT)==-1)
				PRINT_TERMINATE
		

		taxi->tnow = 0;

		if(moveTo("DESTINAZIONE", map, celle_sem, sc_sem, msgbuf.richiesta[1])==-1){/*VIAGGIO FALLISCE ->ABORTED*/
			switch(errno){
				case EAGAIN :
					getresource(sc_sem, SO_WIDTH*SO_HEIGHT);
					r_stats->aborted++;
					releaseresource(sc_sem, SO_WIDTH*SO_HEIGHT);
					shmdt(map);
					execvp("./taxi",argv);
				case EINVAL :
				case EIDRM :
					if(getresource(sc_sem, SO_WIDTH*SO_HEIGHT)==-1)
						TERMINATE
					r_stats->aborted++;
					releaseresource(sc_sem, SO_WIDTH*SO_HEIGHT);
					TERMINATE
				default :
					PRINT_TERMINATE
			}
		}else{	/*SONO IN SOURCE POSSO EVADERE IL VIAGGIO*/
			
			if(getresource(sc_sem, SO_WIDTH*SO_HEIGHT)==-1)
				PRINT_TERMINATE
			r_stats->completed++;

			if(releaseresource(sc_sem, SO_WIDTH*SO_HEIGHT)==-1)
				PRINT_TERMINATE
			if(taxi->tnow>taxi->tmax)
			taxi->tmax = taxi->tnow;
		}	
	}
}







int moveHoriz(cella *map,const int celle_sem,const int sc_sem,const int dest){

	int *pos,posX,posY,destX,destY,newpos;
	struct timespec attr_time;

	pos = &(taxi->pos);
	posX = getX(map,*pos);
	posY = getY(map,*pos);
	destX = getX(map,dest);
	destY = getY(map,dest);

	while(posX != destX){

		attr_time.tv_sec = (map[taxi->pos].tempo)/1000000000;
		attr_time.tv_nsec = (map[taxi->pos].tempo)%1000000000;

		nanosleep(&attr_time, NULL);
		taxi->tnow += map[*pos].tempo;
		/*trovo newpos MAPPA MAPPATA A PARTIRE DA ALTO A SINISTRA*/
		if(!map[posX<destX ? (*pos)+1 : (*pos)-1].is_hole)
			newpos = posX<destX ? (*pos)+1 : (*pos)-1;
		else if(posY!=destY)	
			newpos = posY<destY ? (*pos)+SO_WIDTH : (*pos)-SO_HEIGHT;
		else
			newpos = posY < SO_HEIGHT-1 ? (*pos)+SO_WIDTH : (*pos)-SO_WIDTH;

		taxi->tnow += map[*pos].tempo;

		if(transition(celle_sem,*pos, newpos)==-1)
			return -1;

		*pos = newpos;

		if(getresource(sc_sem, *pos)==-1)
			return -1;

		map[newpos].n_attr++;

		if(releaseresource(sc_sem, *pos)==-1)
			return -1;

		taxi->celle_percorse++;
		if(taxi->celle_percorse > taxi->celle_percorsemax)
			taxi->celle_percorsemax = taxi->celle_percorse;
		posX = getX(map,*pos);
		posY = getY(map,*pos);
	}
	return 0;
}

int moveVert(cella *map,const int celle_sem,const int sc_sem,const  int dest){
	int *pos, posX, posY, destX, destY, newpos;
	struct timespec attr_time;

	pos = &(taxi->pos);
	posX = getX(map,*pos);
	posY = getY(map,*pos);
	destX = getX(map,dest);
	destY = getY(map,dest);

	while(posY != destY){
		/*ATTRAVERSO TRAMITE NANOSLEEP*/
		attr_time.tv_sec = map[*pos].tempo/1000000000;
		attr_time.tv_nsec = map[*pos].tempo%1000000000;

		nanosleep(&attr_time, NULL);
		taxi->tnow += map[*pos].tempo;

		/*trovo newpos*/
		if(!(map[posY<destY ? (*pos)+SO_WIDTH : (*pos)-SO_WIDTH].is_hole))
			newpos = posY<destY ? (*pos)+SO_WIDTH : (*pos)-SO_WIDTH;/*basso o alto*/
		else if(posX!=destX)/*mappato a partire da alto sinistra*/
			newpos = posX<destX ? (*pos)+1 : (*pos)-1;
		else
			newpos = posX < SO_WIDTH-1 ? (*pos)+1 : (*pos)-1;

		/*INIZIO TRANSIZIONE CELLA*/
		
			/*MI TOLGO DA POS*/

		if(transition(celle_sem, *pos, newpos)==-1)
			return -1;

		*pos = newpos;	/*sono in newpos*/

		if(getresource(sc_sem, *pos)==-1)
			return -1;

		map[*pos].n_attr++;

		if(releaseresource(sc_sem, *pos)==-1)
			return -1;
		/*FINE TRANSIZIONE CELLA*/

		taxi->celle_percorse++;
		if(taxi->celle_percorse > taxi->celle_percorsemax)
			taxi->celle_percorsemax = taxi->celle_percorse;
		posX = getX(map,*pos);
		posY = getY(map,*pos);
	}
	return 0;
}

int moveTo(const char *dest_name,cella *map,const int celle_sem,const int sc_sem,const int dest){
	if(moveHoriz(map,celle_sem,  sc_sem, dest)==-1)
		return -1;
	if(moveVert(map, celle_sem,  sc_sem, dest)==-1)
		return -1;
	if(taxi->pos != dest && !map[dest].is_hole)
		if(moveHoriz(map, celle_sem,  sc_sem, dest)==-1)
			return -1;
	return 0;
}

int getresource(int semid, int semnum){
	struct sembuf op;
	op.sem_op = -1;
	op.sem_num = semnum;
	op.sem_flg = 0;
	if(semop(semid, &op, 1) == -1)
		return -1;
	else
		return 0;
}

int releaseresource(int semid, int semnum){
	struct sembuf op;
	op.sem_op = 1;
	op.sem_num = semnum;
	op.sem_flg = 0;
	if(semop(semid, &op, 1) == -1)
		return -1;
	else
		return 0;
}

int transition(int celle_sem, int pos, int newpos){
	struct sembuf ops[2];
	struct timespec timeout = {0,0};
	timeout.tv_sec = SO_TIMEOUT;
	ops[0].sem_num = pos;
	ops[0].sem_op = 1;
	ops[0].sem_flg = 0;
	ops[1].sem_num = newpos;
	ops[1].sem_op = -1;
	ops[1].sem_flg = 0;
	if(semtimedop(celle_sem, ops, 2, &timeout) == -1){
		switch(errno){
			case EAGAIN :
				if(semop(celle_sem, ops, 1)==-1)
					fprintf(stderr,	
					"%s:%d PID=%5d : Error %d(%s)\n"
					,__FILE__,__LINE__,getpid(),errno,strerror(errno));
				return -1;
			case EINVAL :
			case EIDRM :
				return -1;
			default :
				fprintf(stderr,	
					"%s:%d PID=%5d : Error %d(%s)\n"
					,__FILE__,__LINE__,getpid(),errno,strerror(errno));
				return -1;
		}
	}else{
		return 0;	
	}
}
