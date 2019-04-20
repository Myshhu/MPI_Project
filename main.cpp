#include "main.hpp"

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
//extern char* returnTypeString(int type);
extern void sendPacket(packet_t *data, int dst, int type);

/**********************
	init.cpp
*/


int rank;
int size;

pthread_t threadDelay;
//GQueue *delayStack;
pthread_mutex_t packetMut = PTHREAD_MUTEX_INITIALIZER;

void check_thread_support(int provided)
{
    printf("THREAD SUPPORT: %d\n", provided);
    switch (provided) {
        case MPI_THREAD_SINGLE: 
            printf("Brak wsparcia dla wątków, kończę\n");
            /* Nie ma co, trzeba wychodzić */
	    fprintf(stderr, "Brak wystarczającego wsparcia dla wątków - wychodzę!\n");
	    MPI_Finalize();
	    exit(-1);
	    break;
        case MPI_THREAD_FUNNELED: 
            printf("tylko te wątki, ktore wykonaly mpi_init_thread mogą wykonać wołania do biblioteki mpi\n");
	    break;
        case MPI_THREAD_SERIALIZED: 
            /* Potrzebne zamki wokół wywołań biblioteki MPI */
            printf("tylko jeden watek naraz może wykonać wołania do biblioteki MPI\n");
	    break;
        case MPI_THREAD_MULTIPLE: printf("Pełne wsparcie dla wątków\n");
	    break;
        default: printf("Nikt nic nie wie\n");
    }
}

/* Nie ruszać, do użytku wewnętrznego przez wątek komunikacyjny */
typedef struct {
    packet_t *newP;
    int type;
    int dst;
    } stackEl_t;

void inicjuj(int *argc, char ***argv)
{
    int provided;
    //delayStack = g_queue_new();
    MPI_Init_thread(argc, argv,MPI_THREAD_MULTIPLE, &provided);
    check_thread_support(provided);


    /* Stworzenie typu */
    /* Poniższe (aż do MPI_Type_commit) potrzebne tylko, jeżeli
       brzydzimy się czymś w rodzaju MPI_Send(&typ, sizeof(pakiet_t), MPI_BYTE....
    */
    /* sklejone z stackoverflow */
    const int nitems=FIELDNO; // Struktura ma FIELDNO elementów - przy dodaniu pola zwiększ FIELDNO w main.h !
    int       blocklengths[FIELDNO] = {1,1,1,1}; /* tu zwiększyć na [4] = {1,1,1,1} gdy dodamy nowe pole */
    MPI_Datatype typy[FIELDNO] = {MPI_INT, MPI_INT,MPI_INT,MPI_INT}; /* tu dodać typ nowego pola (np MPI_BYTE, MPI_INT) */
    MPI_Aint     offsets[FIELDNO];

    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, rank);
    offsets[2] = offsetof(packet_t, dst);
    offsets[3] = offsetof(packet_t, src);
    /* tutaj dodać offset nowego pola (offsets[2] = ... */

    MPI_Type_create_struct(nitems, blocklengths, offsets, typy, &MPI_PAKIET_T);
    MPI_Type_commit(&MPI_PAKIET_T);


    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    srand(rank);

    pthread_create( &threadCom, NULL, comFunc, 0);
    //pthread_create( &threadDelay, NULL, delayFunc, 0);
    if (rank==ROOT) {
	    //pthread_create( &threadM, NULL, monitorFunc, 0);
    } 
}

void finalizuj(void)
{
    pthread_mutex_destroy( &konto_mut);
    /* Czekamy, aż wątek potomny się zakończy */
    //println("czekam na wątek \"komunikacyjny\"\n" );
    pthread_join(threadCom,NULL);
    //println("czekam na wątek \"opóźniający\"\n" );
    //pthread_join(threadDelay,NULL);
    //if (rank==0) pthread_join(threadM,NULL);
    MPI_Type_free(&MPI_PAKIET_T);
    MPI_Finalize();
    //g_queue_free(delayStack);
}

std::string returnTypeString(int type) {
    switch(type) {
        case 1:
            return "FINISH";
        case 2:
            return "REQUEST";
        case 3:
            return "ANSWER";
        case 4:
            return "RELEASE";
        default:
            return "ERROR";
    }
}

void sendPacket(packet_t *data, int dst, int type)
{
    data->ts = global_ts;
    data->rank = rank;
    global_ts++;
    println("Wysylam pakiet typu %s do procesu %d, zwiekszam swoj zegar z %d na %d\n", returnTypeString(type).c_str(), dst, global_ts - 1, global_ts);
    MPI_Send(data, 1, MPI_PAKIET_T, dst, type, MPI_COMM_WORLD);
}


///////////////////*/
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
        println("Dostałem pakiet %s od procesu %d, zmieniam globalny zegar z %d na %d", returnTypeString((int)status.MPI_TAG).c_str(), pakiet.rank, global_ts, max(global_ts, pakiet.ts) + 1);
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
    
}

void finishHandler(packet_t *pakiet, int numer_statusu)
{
    /* właściwie nie wykorzystywane */
    //println("Otrzymałem FINISH" );
    end = TRUE; 
}

void handleRequest(packet_t *pakiet, int numer_statusu)
{
    packet_t tmp;
    tmp.rank = rank;
    addToQueue(pakiet, numer_statusu);
    //println("Dostałem REQUEST od procesu %d, jego czas to %d, odsyłam ANSWER, tmp.rank = %d\n", pakiet->rank, pakiet->ts, tmp.rank);
    sendPacket(&tmp, pakiet->rank, ANSWER);
}

void queueChanged() {
	std::string queue_string = "";
	queue_string += "\n---- Zmieniono kolejkę, wypisuje kolejke: \n";
	//println("---- Wypisuje kolejke: ");
	for(unsigned int i = 0; i < kolejka.size(); i++) {
		queue_string += "Proces: ";
		queue_string += std::to_string(kolejka[i].numer_procesu);
		queue_string += " Zegar: ";
		queue_string += std::to_string(kolejka[i].zegar_procesu);
		queue_string += " Typ: ";
		queue_string += returnTypeString(kolejka[i].typ_komunikatu).c_str();
		queue_string += "\n";
		//println("---- Proces: %d zegar: %d typ %s", kolejka[i].numer_procesu, kolejka[i].zegar_procesu, returnTypeString(kolejka[i].typ_komunikatu).c_str());
	}
	queue_string += "---- Koniec kolejki \n";
	println("%s", queue_string.c_str());
	//println("---- Koniec kolejki");
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

}
