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
int max_licences = 4;

/* Maksymalna liczba zajęcy w parku */
int max_animals = 11;

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

bool are_animals_alive = true;
int* tablicaPoczatkowa = NULL;

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
	tablicaPoczatkowa = new int[size];  // Allocate n ints and save ptr in a.
	for (int i=0; i<size; i++) {
		tablicaPoczatkowa[i] = 0;    // Initialize all elements to zero.
	}
	tablicaPoczatkowa[rank] = to_hunt;
	
	usleep(100 * (rand() % 100 + 1)); //Żeby pomieszać im pozycje startowe
    packet_t pakiet;
    pakiet.rank = rank;
    pakiet.ts = global_ts;
	pakiet.to_hunt = to_hunt;
    global_ts_at_REQUEST = global_ts;
	sendToAllProces(&pakiet, REQUEST);
}

void sendRequest() {
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
        
        wypiszTablicePoczatkowa();
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
	
	//Odejmowanie
	if((int)kolejka_licencji.size() != size) {
		println("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Odejmowanie przy niepełnej kolejce");
		int do_odjecia = tablicaPoczatkowa[pakiet->rank] - pakiet->to_hunt;
		tablicaPoczatkowa[pakiet->rank] = pakiet->to_hunt;
		current_animals -= do_odjecia;
		if(current_animals <= 0) {
			current_animals = 0;
			are_animals_alive = false;
		}
	}
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
    
	przeliczLiczbeZwierzat();
	
	//Zaktualizuj tablicę początkową
	tablicaPoczatkowa[pakiet->rank] = pakiet->to_hunt;
    //println("Dostałem REQUEST od procesu %d, jego czas to %d, odsyłam ANSWER, tmp.rank = %d\n", pakiet->rank, pakiet->ts, tmp.rank);
    packet_t tmp;
	tmp.rank = rank;
	sendPacket(&tmp, pakiet->rank, ANSWER);
}

//Funkcja aktualizująca ilość zajęcy, wywoływana przy zmianach kolejki
void przeliczLiczbeZwierzat() {
    for(unsigned int i = 0; i < kolejka_licencji.size(); i++) {
		println("kolejka_licencji[%d].numer_procesu %d == %d", i, kolejka_licencji[i].numer_procesu, rank);
		if((kolejka_licencji[i].numer_procesu == rank) || (int)kolejka_licencji.size() != size)  {
			break;
		} else {
			//Patrzymy czy zsumowano, bo jeśli proces tylko przesunął się w górę kolejki, to nie chcemy tej ilości odejmować drugi raz
			if(!(kolejka_licencji[i].czy_zsumowano))
			{
				current_animals = current_animals - kolejka_licencji[i].to_hunt;
				kolejka_licencji[i].czy_zsumowano = true;
				println("Odejmuje");
				if(current_animals <= 0) {
					current_animals = 0;
					are_animals_alive = false;
				}
			}
		}
	}
}

void tryToEnterPark() {
	println("Próbuję wejść do parku");
	
	przeliczLiczbeZwierzat();
	
	if((int)kolejka_licencji.size() == size) {
		for(int i = 0; i < max_licences; i++) {
			if(kolejka_licencji[i].numer_procesu == rank) {
				if(are_animals_alive) {
					enterPark();
				} else {
					println("Nieudało się, bo nie starczyło zwierząt");
				}
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
	poluj();
	usleep(150000);
	leavePark();
}

void poluj() {
	println("Poluje, jest %d zwierzat", current_animals);
	if(current_animals > to_hunt) {
		println("If 1, current_animals %d, to_hunt %d", current_animals, to_hunt);
		current_animals -= to_hunt;
		to_hunt = 0;
	} else if(current_animals == to_hunt) {
		println("If 2, current_animals %d, to_hunt %d", current_animals, to_hunt);
		current_animals = 0;
		to_hunt = 0;
	} else {
		println("If 3, current_animals %d, to_hunt %d", current_animals, to_hunt);
		to_hunt -= current_animals;
		current_animals = 0;
	}
	tablicaPoczatkowa[rank] = to_hunt;
	println("After jest %d zwierzat", current_animals);
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

	if(chce_do_parku == true) {
		deleteFromQueue(kolejka_licencji, rank);
		sendRequest();
	}
	
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

void wypiszTablicePoczatkowa() {
	std::string res = "";
	for(int i = 0; i < size; i++) {
		res = res + " " + std::to_string(tablicaPoczatkowa[i]);
	}
	println("Tablica poczatkowa to: %s", res.c_str());
}
