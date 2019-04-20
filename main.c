#include "main.h"

MPI_Datatype MPI_PAKIET_T;
pthread_t threadCom, threadM;

/* zamek do synchronizacji zmiennych współdzielonych */
pthread_mutex_t konto_mut = PTHREAD_MUTEX_INITIALIZER;
sem_t all_sem;

/* Ile każdy proces ma na początku pieniędzy */
int konto=STARTING_MONEY;


/* suma zbierana przez monitor */
int sum = 0;

/* globalny zegar */
int global_ts = 0;

/* kolejka */
std::vector <element_kolejki> kolejka;

/* end == TRUE oznacza wyjście z main_loop */
volatile char end = FALSE;
void mainLoop(void);

/* Deklaracje zapowiadające handlerów. */
void handleRequest(packet_t *pakiet, int numer_statusu);
void finishHandler(packet_t *pakiet, int numer_statusu);
void handleAnswer(packet_t *pakiet, int numer_statusu);
void handleRelease(packet_t *pakiet, int numer_statusu);
void addToQueue(packet_t *pakiet, int numer_statusu);

/* typ wskaźnik na funkcję zwracającej void i z argumentem packet_t* */
typedef void (*f_w)(packet_t *);
/* Lista handlerów dla otrzymanych pakietów
   Nowe typy wiadomości dodaj w main.h, a potem tutaj dodaj wskaźnik do 
     handlera.
   Funkcje handleróœ są na końcu pliku. Nie zapomnij dodać
     deklaracji zapowiadającej funkcji!
*/
/*f_w handlers[MAX_HANDLERS] = { [REQUEST]=handleRequest,
            [FINISH] = finishHandler,
            [ANSWER] = handleAnswer,
            [RELEASE] = handleRelease };*/

void inicjuj(int *argc, char ***argv);
extern void finalizuj(void);
extern char* returnTypeString(int type);
extern void sendPacket(packet_t *data, int dst, int type);

int main(int argc, char **argv)
{
    /* Tworzenie wątków, inicjalizacja itp */
    inicjuj(&argc,&argv);

    mainLoop();

    finalizuj();
    return 0;
}


/* Wątek główny - przesyła innym pieniądze */
void mainLoop(void)
{
    packet_t pakiet;
    for(int i = 0; i < size; i++) {
        if( i != rank) {
            sendPacket(&pakiet, i, REQUEST);
            //println("Rank %d, wyslalem REQUEST do %d\n", rank, i);
        }
    }
}

/* Wątek monitora - tylko u ROOTa */
void *monitorFunc(void *ptr)
{
    // packet_t data;
	// /* MONITOR; Jego zadaniem ma być wykrycie, ile kasy jest w systemie */

	// // 5 sekund, coby procesy zdążyły namieszać w stanie globalnym
    // sleep(3);
	// // TUTAJ WYKRYWANIE STANu        
    // int i;
    // sem_init(&all_sem,0,0);
    // println("MONITOR START \n");
    // for (i=0;i<size;i++)  {
	// sendPacket(&data, i, GIVE_YOUR_STATE);
    // }
    // sem_wait(&all_sem);

    // for (i=1;i<size;i++) {
	// sendPacket(&data, i, FINISH);
    // }
    // sendPacket(&data, 0, FINISH);
    // P_RED; printf("\n\tW systemie jest: [%d]\n\n", sum);P_CLR
     return 0;
}

int max(int a, int b) {
    if(a>b) {
        return a;
    } else {
        return b;
    }
}


/* Wątek komunikacyjny - dla każdej otrzymanej wiadomości wywołuje jej handler */
void *comFunc(void *ptr)
{

    MPI_Status status;
    packet_t pakiet;
    /* odbieranie wiadomości */
    while ( !end ) {
        MPI_Recv( &pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        pakiet.src = status.MPI_SOURCE;

	    pthread_mutex_lock(&konto_mut);
        println("Dostałem pakiet %s od procesu %d, zmieniam globalny zegar z %d na %d", returnTypeString((int)status.MPI_TAG), pakiet.rank, global_ts, max(global_ts, pakiet.ts) + 1);
        global_ts = max(global_ts, pakiet.ts) + 1;
	    pthread_mutex_unlock(&konto_mut);

        if (status.MPI_TAG == FINISH) end = TRUE;
        else {        	//handlers[(int)status.MPI_TAG](&pakiet, (int)status.MPI_TAG);
        	switch((int)status.MPI_TAG) {
        		case FINISH:
        			finishHandler(&pakiet, (int)status.MPI_TAG);
        			break;
        		case REQUEST:
        			handleRequest(&pakiet, (int)status.MPI_TAG);
        			break;
        		case ANSWER:
        			handleAnswer(&pakiet, (int)status.MPI_TAG);
        			break;
        		case RELEASE:
        			handleRelease(&pakiet, (int)status.MPI_TAG);
        			break;
        		default:
        			println("Function calling error");
        			break;
        	}
        }
        			

    }
    println(" Koniec! ");
    return 0;
}

/* Handlery */
void handleRelease(packet_t *pakiet, int numer_statusu)
{
    //println("Dostalem release\n");
    // static int statePacketsCnt = 0;

    // statePacketsCnt++;
    // sum += pakiet->kasa;
    // println("Suma otrzymana: %d, total: %d\n", pakiet->kasa, sum);
    // //println( "%d statePackets from %d\n", statePacketsCnt, pakiet->src);
    // if (statePacketsCnt == size ) {
    //     sem_post(&all_sem);
    // }
    
}

void finishHandler(packet_t *pakiet, int numer_statusu)
{
    /* właściwie nie wykorzystywane */
    //println("Otrzymałem FINISH" );
    end = TRUE; 
}

void handleRequest(packet_t *pakiet, int numer_statusu)
{
    /* monitor prosi, by mu podać stan kasy */
    /* tutaj odpowiadamy monitorowi, ile mamy kasy. Pamiętać o muteksach! */

    packet_t tmp;
    tmp.rank = rank;
    addToQueue(pakiet, numer_statusu);
    //println("Dostałem REQUEST od procesu %d, jego czas to %d, odsyłam ANSWER, tmp.rank = %d\n", pakiet->rank, pakiet->ts, tmp.rank);
    sendPacket(&tmp, pakiet->rank, ANSWER);
}

void queueChanged() {
	println("Wypisuje kolejke: ");
	for(unsigned int i = 0; i < kolejka.size(); i++) {
		println("%d %d %d", kolejka[i].numer_procesu, kolejka[i].zegar_procesu, kolejka[i].typ_komunikatu);
	}
	println("Koniec kolejki");
}

void addToQueue(packet_t *pakiet, int numer_statusu) {
	element_kolejki nowy_element;
	nowy_element.numer_procesu = pakiet->rank;
	nowy_element.zegar_procesu = pakiet->ts;
	nowy_element.typ_komunikatu = numer_statusu;
	kolejka.push_back(nowy_element);
	
	queueChanged();
}

void handleAnswer(packet_t *pakiet, int numer_statusu)
{
    /* ktoś przysłał mi przelew */
    //println("Dostalem answer od rank = %d\n", pakiet->rank);
    /*println("\tdostałem %d od %d\n", pakiet->kasa, pakiet->src);
    pthread_mutex_lock(&konto_mut);
	konto+=pakiet->kasa;
    println("Stan obecny: %d\n", konto);
    pthread_mutex_unlock(&konto_mut);*/
}
