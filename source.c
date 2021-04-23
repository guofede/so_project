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
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include "mappa.h"

#define CHECK 	if(errno)\
			switch(errno){\
				case EINVAL :\
				case EIDRM :\
					shmdt(map);\
					exit(0); \
				default :\
					fprintf(stderr,	\
					"%s:%d PID=%5d : Error %d(%s)"\
					,__FILE__,__LINE__,getpid(),errno,strerror(errno));\
					shmdt(map);\
					exit(EXIT_FAILURE);\
			}

void sigusr_handler(int signal);

int flag = 1;

int main(int argc, char *argv[]){
	int source_pos, shmap_id,sc_sem, sync_sem,msg_id/*pipe_fd*/, richiesta[2]={0,0};
	cella *map;
	struct timespec buf, time={3,0};
	struct sembuf ops[2];
	struct sigaction sa;
	struct messaggio msgbuf;
	sigset_t p_mask, h_mask;
	richieste_stats *r_stats;

	msgbuf.mtype = 1;
	if(argc<6)
		exit(EXIT_FAILURE);

	srand(getpid());
	msgbuf.richiesta[0] = source_pos = atoi(argv[1]);
	shmap_id = atoi(argv[2]);
	sync_sem = atoi(argv[3]);
	msg_id = atoi(argv[4]);
	sc_sem = atoi(argv[5]);

	sa.sa_handler = sigusr_handler;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&h_mask);
	sa.sa_mask = h_mask;
	sigemptyset(&p_mask);
	sigaddset(&p_mask,SIGUSR1);
	if(sigaction(SIGUSR1,&sa,NULL)==-1){
		perror("sigaction");
		exit(1);
	}
	
	map = shmat(shmap_id,NULL,0);
	if(map == NULL)
		CHECK

	r_stats = (richieste_stats *) (map + (SO_WIDTH*SO_HEIGHT));

	ops[0].sem_num = 0;
	ops[0].sem_op = 0;
	ops[0].sem_flg = 0;
	ops[1].sem_num = SO_WIDTH*SO_HEIGHT;
	ops[1].sem_flg = 0;
	semop(sync_sem, ops, 1);
	CHECK
	while(1){/*CICLO DI GENERAZIONE RICHIESTE DAL PUNTO SOURCE*/
		while(flag){
			
			while((msgbuf.richiesta[1] = random_transable_pos(map)) == source_pos);
			
			if(msgsnd(msg_id,&msgbuf,sizeof(msgbuf.richiesta),0)==-1)
				switch(errno){
					case EINTR :
						msgsnd(msg_id,&msgbuf,sizeof(msgbuf.richiesta),0);
						break;
					case EINVAL :
					case EIDRM :
						shmdt(map);
						exit(0);
					default :
						fprintf(stderr,
						"%s:%d PID=%5d : Error %d(%s)"
						,__FILE__,__LINE__,getpid(),errno,strerror(errno));
						shmdt(map);
						exit(EXIT_FAILURE);
					break;
				}

			ops[1].sem_op = -1;
			if(semop(sc_sem, ops+1, 1)==-1)
			CHECK

			r_stats->inevasi++;
			ops[1].sem_op = 1;

			if(semop(sc_sem, ops+1, 1)==-1)
			CHECK
			
			sigprocmask(SIG_BLOCK,&p_mask,NULL);
			flag--;
			sigprocmask(SIG_UNBLOCK,&p_mask,NULL);

		}

		if(nanosleep(&time,&buf)==-1 && errno == EINTR)
				while(1){
					if(nanosleep(&buf,&buf)==-1 && errno == EINTR)
						;
					else
						break;
				}

		sigprocmask(SIG_BLOCK,&p_mask,NULL);
		flag++;
		sigprocmask(SIG_UNBLOCK,&p_mask,NULL);
	}
}

void sigusr_handler(int signal){
	flag++;
}

