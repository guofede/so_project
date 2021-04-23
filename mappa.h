#ifndef _MAPPA_H
#define _MAPPA_H

typedef struct{
	long tempo;
	int cap;
	char is_hole;
	int n_attr;
}cella;

struct messaggio{
	long mtype;
	int richiesta[2];
};

typedef struct{
	int completed;
	int aborted;
	int inevasi;
}richieste_stats;

		/*genera mappa con holes e ritorna puntatore alla mappa*/
int map_gen(cella *map,const int celle_sem,const int SO_HOLES,const long SO_CAPTMIN,const long SO_CAPTMAX,const int SO_CAPMIN,const int SO_CAPMAX);
		/*stampa su terminale la mappa*/

void map_print(cella *map, int celle_sem);

int getX(cella *map,const int pos);

int getY(cella *map,const int pos);

int traversable(cella *map,const int pos);

/*genera una posizione attraversabile,
//la mappa deve essere generata con il map_gen
//che garantisce la generazione di una mappa con holes
//distaccati,altrimenti può generare risultati sbagliati
//inizializzare il generatore di numeri random prima di invocare
*/
int random_transable_pos(cella *map);

#endif
