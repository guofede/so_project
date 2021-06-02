#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include "mappa.h"
#include "taxi.h"

#define CHECK if (errno) {fprintf(stderr,	\
				"%s:%d PID=%5d : Error %d(%s)"\
				,__FILE__,__LINE__,getpid(),errno,strerror(errno));\
				exit(EXIT_FAILURE);}

void print_term(cella *map, int SO_TOPCELLS, int SO_TAXI, int SO_SOURCES, taxi_stats *stats, richieste_stats *r_stats);

void handler(int signal);

void dealloc(char * arg[]);

struct source{
	int pid;
	int pos;
};

int mappa_shm,celle_sem, sc_sem, sync_sem, msg_id;
struct source *sourcearr;
int terminate_flag=1;

int main(int argc, char *argv[]){
	int SO_TAXI, SO_HOLES, SO_CAPMIN, SO_CAPMAX, SO_SOURCES, SO_DURATION, SO_TIMEOUT, SO_TOPCELLS;
	int i, j,  MAX_HOLES;
	/*int pipe_fd[2];*/
	long SO_TIMENSEC_MIN,SO_TIMENSEC_MAX;
	struct sembuf ops[1];
	char *taxi_argv[9], *source_argv[7];
	cella * shmap;
	struct timespec secondo = {1,0},starting_sleep = {4,0};
	struct sigaction sa;
	sigset_t mask;
	taxi_stats *stats;
	richieste_stats *r_stats;
	
	
	MAX_HOLES = ((SO_WIDTH>>1)+(SO_WIDTH&1))*((SO_HEIGHT>>1)+(SO_HEIGHT&1));
	/*numero massimo generabile, non è detto che vengano generate anche se soddisfa il criterio*/

			/*INSERIMENTO INPUTS*/
	printf("numero taxi :");
	scanf("%d",&SO_TAXI);
	
	printf("numero buchi :");
	scanf("%d", &SO_HOLES);
	if(SO_HOLES>MAX_HOLES){
		fprintf(stderr, "SO_HOLES non valido");
		exit(EXIT_FAILURE);
	}
	
	printf("limite minimo capacità taxi :");
	scanf("%d", &SO_CAPMIN);
	if(SO_CAPMIN<1){
		fprintf(stderr, "SO_HOLES non valido");
		exit(EXIT_FAILURE);
	}
	
	printf("limite massimo capacità taxi :");
	scanf("%d", &SO_CAPMAX);
	if(SO_CAPMAX<SO_CAPMIN){
		fprintf(stderr, "SO_CAPMAX non valido");
		exit(EXIT_FAILURE);
	}
	
	printf("limite minimo attraversamento (nsec):");
	scanf("%ld", &SO_TIMENSEC_MIN);
	
	printf("limite massimo attraversamento (nsec):");
	scanf("%ld", &SO_TIMENSEC_MAX);
	if(SO_TIMENSEC_MAX<SO_TIMENSEC_MIN){
		printf("il tempo di attraversamenmto massimo deve essere maggiore di quella minimo");
		exit(EXIT_FAILURE);
	}
	
	
	printf("numero source :");
	scanf("%d",&SO_SOURCES);
	if(SO_SOURCES>SO_WIDTH*SO_HEIGHT-SO_HOLES){
		fprintf(stderr, "SO_SOURCES non valido");
		exit(EXIT_FAILURE);
	}
	
	printf("timeout taxi (sec):");
	scanf("%d",&SO_TIMEOUT);
	
	printf("tempo simulazione (sec):");
	scanf("%d",&SO_DURATION);
	
	printf("numero top cells:");
	scanf("%d",&SO_TOPCELLS);
	if(SO_TOPCELLS > SO_WIDTH*SO_HEIGHT){
		fprintf(stderr, "TOP CELLS PUÒ ESSERE AL MASSIMO LA DIMENSIONE DELLA MAPPA");
		exit(EXIT_FAILURE);
	}
			/*FINE INPUTS*/

	/*IMPOSTO HANDLER SIGINT*/
	sa.sa_handler = handler;
	sa.sa_flags = 0;
	sigemptyset(&mask);
	sa.sa_mask = mask;
	sigaction(SIGINT,&sa,NULL);


	celle_sem = semget(IPC_PRIVATE, SO_WIDTH*SO_HEIGHT, IPC_CREAT|0600);
	if(errno == EINVAL){
		perror("Dimensioni mappa fuori range(0-SEMMSL)");
		exit(EXIT_FAILURE);
	}

	mappa_shm = shmget(IPC_PRIVATE, SO_WIDTH*SO_HEIGHT*sizeof(cella) + sizeof(richieste_stats) + sizeof(taxi_stats)*SO_TAXI ,IPC_CREAT|0600);
	shmap = shmat(mappa_shm, NULL, 0);
	if(shmap == NULL){
		CHECK	
	}
	bzero(shmap ,SO_WIDTH*SO_HEIGHT*sizeof(cella) + sizeof(richieste_stats) + sizeof(taxi_stats)*SO_TAXI);
	r_stats = (richieste_stats *) (shmap+SO_WIDTH*SO_HEIGHT);

	/*STATS ARRAY È ALLOCATA DOPO LA MAPPA*/
	stats = (taxi_stats *) (r_stats+1);
	srand(time(NULL));
	if(map_gen(shmap,celle_sem,SO_HOLES,SO_TIMENSEC_MIN,SO_TIMENSEC_MAX,SO_CAPMIN,SO_CAPMAX)==-1)
		terminate_flag = 0;

	/*semaforo per gestire scrittura sulle varie celle della mappa e tabella richieste(sezione critica)*/
	sc_sem = semget(IPC_PRIVATE, SO_WIDTH*SO_HEIGHT+1, IPC_CREAT|0600);
		CHECK
	/*imposto semafori per scrittura su sezione critica una per ogni cella*/
	for(i=0;i< SO_WIDTH*SO_HEIGHT+1;i++){
		semctl(sc_sem,i,SETVAL,1);
		CHECK
	}
	
	/*ottengo semaforo per sincronizzare la partenza della simulazione*/
	sync_sem = semget(IPC_PRIVATE, 1, IPC_CREAT|0600);
	semctl(sync_sem, 0, SETVAL, 1);
	msg_id = msgget(IPC_PRIVATE, IPC_CREAT|0600);
	
	for(i=0;i<8;i++){
		taxi_argv[i] = malloc(256);
	}
	
	sprintf(taxi_argv[0],"./taxi");
	sprintf(taxi_argv[1],"%d",mappa_shm);
	sprintf(taxi_argv[2],"%d",celle_sem);
	sprintf(taxi_argv[3],"%d",sc_sem);
	sprintf(taxi_argv[4],"%d",sync_sem);
	sprintf(taxi_argv[5],"%d",msg_id);
	sprintf(taxi_argv[6],"%d",SO_TIMEOUT);
	taxi_argv[8] = NULL;


	for(i=0;terminate_flag && i<SO_TAXI; i++){
		switch(fork()){
			case -1:
				fprintf(stderr, "fork taxi error");
				terminate_flag = 0;
				break;
			case 0 :
				sprintf(taxi_argv[7],"%d",i);
				execvp("./taxi", taxi_argv);
				CHECK
			default : 
				break;
		}
	}


	for(i=0;i<6;i++){
		source_argv[i] = malloc(256);
	}
	sourcearr = malloc(sizeof(struct source)*SO_SOURCES);

	for(i=0;i<SO_SOURCES;){
		sourcearr[i].pos = random_transable_pos(shmap);
		for(j=0;j<i;j++){
				if(sourcearr[i].pos == sourcearr[j].pos)
					break;
		}
		if(j==i)
			i++;
	}
	sprintf(source_argv[0],"./source");
	sprintf(source_argv[2],"%d",mappa_shm);
	sprintf(source_argv[3],"%d",sync_sem);
	sprintf(source_argv[4],"%d",msg_id);
	sprintf(source_argv[5],"%d",sc_sem);
	source_argv[6] = NULL;
	for(i=0;terminate_flag && i<SO_SOURCES;i++){
		switch(sourcearr[i].pid=fork()){
			case -1:
				switch(errno){
					case EAGAIN :
						perror("LIMITE DI THREADS MASSIMI RAGGIUNTO \n");
						terminate_flag = 0;
						break;
					default :
						CHECK
				}
			case 0 : 
				srand(time(NULL) - i*2);
				sprintf(source_argv[1],"%d",sourcearr[i].pos);
				execvp("./source", source_argv);
				CHECK
			default :
				break;
		}
	}
	
	dealloc(taxi_argv);
	dealloc(source_argv);
	/*IMPOSTO SIGHANDLER PER SIGALRM*/
	
	sigaction(SIGALRM,&sa,NULL);
	alarm(SO_DURATION);

	for(i=0; terminate_flag && i<SO_SOURCES;i++){
		printf("SOURCE pid:%d	pos:%d\n",sourcearr[i].pid,sourcearr[i].pos);
	}

	ops[0].sem_num = 0;
	ops[0].sem_op = -1;
	ops[0].sem_flg = 0;
	nanosleep(&starting_sleep,NULL);
	semop(sync_sem,ops,1);		/*PARTENZA*/
	
	while(terminate_flag){
		map_print(shmap, celle_sem);
		nanosleep(&secondo, NULL);
	}

	/*STAMPA DI TERMINAZIONE*/

	printf("in terminazione...\n");
	msgctl(msg_id,IPC_RMID, NULL);
	semctl(sync_sem, 0, IPC_RMID);
	semctl(celle_sem,0,IPC_RMID);

	while(wait(NULL)!=-1){	/*wait fino a quando tutti i figli terminano o timer*/}
	semctl(sc_sem,0,IPC_RMID);
	print_term(shmap, SO_TOPCELLS, SO_TAXI, SO_SOURCES, stats, r_stats);

	shmdt(shmap);
	shmctl(mappa_shm,IPC_RMID,NULL);
	printf("terminato\n");
}

void dealloc(char * arg[]){
	int i;
	for(i=1;arg[i]!=NULL;i++){
		free(arg[i]);
		CHECK
	}
}

/*SIGNAL HANDLER*/
void handler(int signal){
	switch(signal){
		case SIGALRM:
		case SIGINT:
			terminate_flag=0;
			break;
		default :
			break;
	}
}
int compare_sourcepos(const void*a, const void*b){
	struct source *sa, *sb;
	sa = (struct source *) a;
	sb = (struct source *) b;
	return sa->pos-sb->pos;
}

int compare_celle(const void *a,const void*b){
	int value_a = (*((cella **) a))->n_attr;
	int value_b = (*((cella **) b))->n_attr;
	if(value_a<value_b)
		return value_a-value_b;
	
}

void print_term(cella *map, int SO_TOPCELLS, int SO_TAXI, int SO_SOURCES,taxi_stats *stats, richieste_stats *r_stats){
	int i, j, output[3]={0,0,0};	/*output[0]->taxi che ha percorso più celle,
					/ output[1]->taxi che ha servito la richiesta più lunga(nsec time),
					/ output[2]->taxi che ha servito più richieste.
					*/
	cella **cella_ptr_ptr, *cella_ptr;
	cella_ptr = map;
	for(i=0;i<SO_TAXI;i++){
			if(stats[i].celle_percorse > stats[output[0]].celle_percorse)
				output[0] = i;
			if(stats[i].tmax > stats[output[1]].tmax)
				output[1] = i;
			if(stats[i].n_richieste > stats[output[2]].n_richieste)
					output[2] = i;
		}
		printf("\n\n\n\n\n");
		printf("TAXI CON MAGGIOR NUMERO DI CELLE PERCORSE : %d N CELLE : %d\n",stats[output[0]].pid,stats[output[0]].celle_percorsemax);
		printf("TAXI CHE HA EFFETTUATO IL TRAGITTO PIÙ LUNGO : %d DURATA : %ld\n",stats[output[1]].pid,stats[output[1]].tmax);
		printf("TAXI CON IL NUMERO MAGGIORE DI RICHIESTE RACCOLTE : %d N RICHIESTE : %d\n",stats[output[2]].pid,stats[output[2]].n_richiestemax);
		printf("RICHIESTE COMPLETATE : %d \n",r_stats->completed);
		printf("RICHIESTE ABORTITE : %d \n", r_stats->aborted);
		printf("RICHIESTE INEVASE : %d \n",r_stats->inevasi);
		
	cella_ptr_ptr = malloc(sizeof(cella *)*SO_WIDTH*SO_HEIGHT);
	for(i=0; i<SO_WIDTH*SO_HEIGHT;  i++){
		cella_ptr_ptr[i] = &(cella_ptr[i]);
	}

	qsort(cella_ptr_ptr, SO_WIDTH*SO_HEIGHT, sizeof(cella *), compare_celle);

	for(i=0;i < SO_WIDTH*SO_HEIGHT-SO_TOPCELLS; i++){
		(cella_ptr_ptr[i])->n_attr = 0;
	}
	free(cella_ptr_ptr);

	qsort(sourcearr, SO_SOURCES, sizeof(struct source), compare_sourcepos);

	for(i=0, j=0; i < SO_WIDTH*SO_HEIGHT; i++){
		if(cella_ptr[i].n_attr){
			printf("%d", cella_ptr[i].n_attr);
			if(j<SO_SOURCES && sourcearr[j].pos == i){
			printf("so");
			j++;
			}
		}else{
			if(j<SO_SOURCES && sourcearr[j].pos == i){
			printf("so");
			j++;
			}else{
			printf("*");
			}
		}
		
		printf("	");
		if(i%SO_WIDTH == SO_WIDTH-1)
			printf("\n\n");
	}
}




		
