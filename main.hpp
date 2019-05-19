#ifndef MAINH
#define MAINH

/* boolean */
#define TRUE 1
#define FALSE 0

const int ROOT = 0;
const int TECHNIK = ROOT;

#define FINISH 1
#define REQUEST 2
#define ANSWER 3
#define RELEASE 4
#define NEWANIMALS 5
/* MAX_HANDLERS musi się równać wartości ostatniego typu pakietu + 1 */
#define MAX_HANDLERS 6 

#define STARTING_MONEY 1000

#include <mpi.h>
#include <stdlib.h>
#include <stdio.h> 
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <string>

/* FIELDNO: liczba pól w strukturze packet_t */
#define FIELDNO 5
typedef struct {
    int ts; /* zegar lamporta */
    int rank; 

    int dst; /* pole ustawiane w sendPacket */
    int src; /* pole ustawiane w wątku komunikacyjnym na rank nadawcy */
	int to_hunt;
    /* przy dodaniu nowych pól zwiększy FIELDNO i zmodyfikuj 
       plik init.c od linijki 76
    */
} packet_t;

typedef struct {

	int numer_procesu;
	int zegar_procesu;
	int typ_komunikatu;
	bool czy_zsumowano;
	int to_hunt;
	
} element_kolejki;

struct normal_sort
{
	inline bool operator() (const element_kolejki& element1, const element_kolejki& element2)
	{
		if(element1.zegar_procesu != element2.zegar_procesu) {
			return(element1.zegar_procesu < element2.zegar_procesu);
		} else {
			return(element1.numer_procesu < element2.numer_procesu);
		}
	}
};

struct type_sort
{
	inline bool operator() (const element_kolejki& element1, const element_kolejki& element2)
	{
		return(element1.typ_komunikatu < element2.typ_komunikatu);
	}
};

extern int rank,size;
extern int global_ts;
extern volatile char end;
extern MPI_Datatype MPI_PAKIET_T;
extern pthread_t threadCom, threadM, threadDelay;

/* Deklaracje zapowiadające handlerów. */
void handleRequest(packet_t *pakiet, int numer_statusu);
void finishHandler(packet_t *pakiet, int numer_statusu);
void handleAnswer(packet_t *pakiet, int numer_statusu);
void handleRelease(packet_t *pakiet, int numer_statusu);
void handleNewAnimals(packet_t *pakiet, int numer_statusu);
/**************************/

void mainLoop(void);
void broadcastMessage(packet_t *pakiet, int typ, int REQUEST_ts);
int max(int a, int b);
void tryToEnterPark();
void enterPark();
void leavePark();
void poluj();
void przeliczLiczbeZwierzat();
void wypiszTabliceIleChcaUpolowac();
void sprawdzCzyKtosChcePolowac();
void sendFinish();

/* synchro do zmiennej konto */
extern pthread_mutex_t konto_mut;

/* argument musi być, bo wymaga tego pthreads. Wątek komunikacyjny */
void *comFunc(void *);

#define PROB_OF_SENDING 35
#define PROB_OF_PASSIVE 5
#define PROB_OF_SENDING_DECREASE 1
#define PROB_SENDING_LOWER_LIMIT 1
#define PROB_OF_PASSIVE_INCREASE 1

/* makra do wypisywania inina ekranie */
#define P_WHITE printf("ini%c[%d;%dm",27,1,37);
#define P_BLACK printf("ini%c[%d;%dm",27,1,30);
#define P_RED printf("%cini[%d;%dm",27,1,31);
#define P_GREEN printf("ini%c[%d;%dm",27,1,33);
#define P_BLUE printf("%inic[%d;%dm",27,1,34);
#define P_MAGENTA printfini("%c[%d;%dm",27,1,35);
#define P_CYAN printf("%inic[%d;%d;%dm",27,1,36);
#define P_SET(X) printf(ini"%c[%d;%dm",27,1,31+(6+X)%7);
#define P_CLR printf("%cini[%d;%dm",27,0,37);

/* Tutaj dodaj odwołanieini do zegara lamporta */
#define println(FORMAT, ...) printf("%c[%d;%dm global_ts: [%d] rank: [%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, global_ts, rank, ##__VA_ARGS__, 27,0,37);

/* macro debug - działa jak printf, kiedy zdefiniowano
   DEBUG, kiedy DEBUG niezdefiniowane działa jak instrukcja pusta 
   
   używa się dokładnie jak printfa, tyle, że dodaje kolorków i automatycznie
   wyświetla rank

   w związku z tym, zmienna "rank" musi istnieć.
*/
#ifdef DEBUG
#define debug(...) printf("%c[%d;%dm [%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, ##__VA_ARGS__, 27,0,37);

#else
#define debug(...) ;
#endif
#endif
