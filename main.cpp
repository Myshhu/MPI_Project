#include "main.hpp"
#include "init.hpp"
#include "queueFunctions.hpp"

MPI_Datatype MPI_PAKIET_T;
pthread_t threadCom, threadM;

/* zamek do synchronizacji zmiennych współdzielonych */
pthread_mutex_t konto_mut = PTHREAD_MUTEX_INITIALIZER;
sem_t all_sem;

int rank;
int size;

pthread_t threadDelay;
//GQueue *delayStack;
pthread_mutex_t packetMut = PTHREAD_MUTEX_INITIALIZER;

/* Ile każdy proces ma na początku pieniędzy */
int konto = STARTING_MONEY;

/* Maksymalna lość licencji */
int max_licences = 2;

/* Maksymalna liczba zajęcy w parku */
int max_animals = 10;

/* Aktualna liczba zajęcy w parku */
int current_animals = max_animals;

/* Ile chce upolowac */
int to_hunt = 3;

/* Ilość uzyskanych zgód */
int answers = 0;

/* suma zbierana przez monitor */
int sum = 0;

/* globalny zegar */
int global_ts = 0;

/* globalny zegar podczas wysyłania żądania */
int global_ts_at_REQUEST = -1;

/* czy chce wejsc do parku */
bool chce_do_parku = true;

/* kolejka_licencji */
std::vector <element_kolejki> kolejka_licencji;

/* end == TRUE oznacza wyjście z main_loop */
volatile char end = FALSE;

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
	usleep(100 * (rand() % 100 + 1)); //Żeby pomieszać im pozycje startowe
    packet_t pakiet;
    pakiet.rank = rank;
    pakiet.ts = global_ts;
	pakiet.to_hunt = to_hunt;
    global_ts_at_REQUEST = global_ts;
	sendToAllProces(&pakiet, REQUEST);
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
	deleteFromQueue(kolejka_licencji, pakiet->rank);
	addToQueue(kolejka_licencji, pakiet, numer_statusu);
	//queueChanged("Release");
}

void finishHandler(packet_t *pakiet, int numer_statusu)
{
    /* właściwie nie wykorzystywane */
    //println("Otrzymałem FINISH" );
    end = TRUE; 
}

void handleRequest(packet_t *pakiet, int numer_statusu)
{   
	deleteFromQueue(kolejka_licencji, pakiet->rank);
    addToQueue(kolejka_licencji, pakiet, numer_statusu);
    //println("Dostałem REQUEST od procesu %d, jego czas to %d, odsyłam ANSWER, tmp.rank = %d\n", pakiet->rank, pakiet->ts, tmp.rank);
    packet_t tmp;
	tmp.rank = rank;
	sendPacket(&tmp, pakiet->rank, ANSWER); //-1, bo dla RELEASE ostatni argument nie jest uzywany
}

void tryToEnterPark() {
	println("Próbuję wejść do parku");
	if((int)kolejka_licencji.size() == size) {
		for(int i = 0; i < max_licences; i++) {
			if(kolejka_licencji[i].numer_procesu == rank) {
				enterPark();
				return;
			}
		}
		println("Nieudało się, park zajęty");
	} else {
		println("Nieudało się, bo kolejka_licencji.size(): %d != size: %d", (int)kolejka_licencji.size(), size);
	}
}

void enterPark() {
	chce_do_parku = false;
	println("Uzyskałem licencję, wszedłem do parku");
	usleep(150000);
	leavePark();
}

void leavePark() {
	packet_t pakiet;
	pakiet.rank = rank;
	pakiet.ts = global_ts;
	pakiet.to_hunt = to_hunt;
	println("Wychodzę z parku, wysyłam release");
	deleteFromQueue(kolejka_licencji, rank);
	addToQueue(kolejka_licencji, &pakiet, RELEASE);
	sendToAllProces(&pakiet, RELEASE);
	
}

void handleAnswer(packet_t *pakiet, int numer_statusu)
{
	answers++;
	if(answers == size - 1) {
		println("Uzyskalem odpowiedzi od wszystkich, dodaje siebie do kolejki");
		packet_t tmppacket;
		tmppacket.rank = rank;
		tmppacket.ts = global_ts_at_REQUEST;
		tmppacket.to_hunt = to_hunt;
	  	addToQueue(kolejka_licencji, &tmppacket, REQUEST);
	  	answers = 0;
	}
}
