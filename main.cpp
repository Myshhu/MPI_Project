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

/* Maksymalna ilość licencji */
int max_licences = 1;

/* Maksymalna ilość zajęcy w parku */
const int max_animals = 5;

/* Aktualna ilość zajęcy w parku */
int current_animals = max_animals;

/* Ile chce upolowac */
int do_upolowania = 3;

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

	//do_upolowania = rand() % 10 + 1;
	
    mainLoop();

    finalizuj();
    return 0;
}


/* Wątek główny - przesyła innym pieniądze */
void mainLoop(void)
{
	usleep(100 * (rand() % 100 + 1)); //Żeby pomieszać im pozycje startowe
    sendRequest();
    /*for(int i = 0; i < size; i++) {
        if( i != rank) {
            sendPacket(&pakiet, i, REQUEST);
            //println("Rank %d, wyslalem REQUEST do %d\n", rank, i);
        }
    }*/
}

void sendRequest() {
	packet_t pakiet;
    pakiet.rank = rank;
    pakiet.ts = global_ts;
    pakiet.ile_chce_upolowac = do_upolowania;
    global_ts_at_REQUEST = global_ts;
  	//addToQueue(&pakiet, REQUEST);
    broadcastMessage(&pakiet, REQUEST, global_ts_at_REQUEST);
}

void broadcastMessage(packet_t *pakiet, int typ, int REQUEST_ts) { //TODO: Zmienic na argument domyslny
	for(int i = 0; i < size; i++) {
        if( i != rank) {
            sendPacket(pakiet, i, typ, REQUEST_ts);
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
    packet_t tmp;
    tmp.rank = rank;
	deleteFromQueue(kolejka_licencji, pakiet->rank);
    addToQueue(kolejka_licencji, pakiet, numer_statusu);
    //println("Dostałem REQUEST od procesu %d, jego czas to %d, odsyłam ANSWER, tmp.rank = %d\n", pakiet->rank, pakiet->ts, tmp.rank);
    sendPacket(&tmp, pakiet->rank, ANSWER, -1); //-1, bo dla RELEASE ostatni argument nie jest uzywany
}

void tryToEnterPark() {
	println("Próbuję wejść do parku");
	are_animals_alive(); //Przelicz zwierzeta
	/* Sprawdzanie czy należy się licencja */
	if((int)kolejka_licencji.size() == size) {
		for(int i = 0; i < max_licences; i++) {
			if(kolejka_licencji[i].numer_procesu == rank) {
				/* Licencja dostępna */
				if(are_animals_alive()) { // Czy są zwierzęta w parku
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

bool are_animals_alive() {
	int moja_pozycja_w_kolejce = 0;
		for(unsigned int i = 0; i < kolejka_licencji.size(); i++) {
			if(kolejka_licencji[i].numer_procesu == rank) {
				moja_pozycja_w_kolejce = i;
				println("moja_pozycja_w_kolejce to %d", moja_pozycja_w_kolejce);
				break;
			}
		}
	
	int iterator = 0;
	if(moja_pozycja_w_kolejce > max_licences) {
		iterator = max_licences;
	} else {
		iterator = moja_pozycja_w_kolejce;
	}
	
	println("iterator = %d", iterator);
	for(int i = 0; i < iterator; i++) {
		println("Jestem w petli");
		if(!(kolejka_licencji[i].czy_zsumowano)) {
			println("Zmniejszam current_animals o %d", kolejka_licencji[i].ile_chce_upolowac);
			current_animals -= kolejka_licencji[i].ile_chce_upolowac;
			kolejka_licencji[i].czy_zsumowano = true;
		}
	}
	
	if(current_animals > 0) {
		return true;
	} else {
		return false;
	}
}
	

void enterPark() {
	chce_do_parku = false;
	println("Uzyskałem licencję, wszedłem do parku");
	poluj();
	if(do_upolowania > 0) {
		chce_do_parku = true;
	}
	
	usleep(150000);
	leavePark();
}

void poluj() {
	println("Poluje, jest %d zwierzat", current_animals);
	if(current_animals > do_upolowania) {
		println("If 1, current_animals %d, do_upolowania %d", current_animals, do_upolowania);
		current_animals -= do_upolowania;
		do_upolowania = 0;
	} else if(current_animals == do_upolowania) {
		println("If 2, current_animals %d, do_upolowania %d", current_animals, do_upolowania);
		current_animals = 0;
		do_upolowania = 0;
	} else {
		println("If 3, current_animals %d, do_upolowania %d", current_animals, do_upolowania);
		do_upolowania -= current_animals;
		current_animals = 0;
	}
	println("After jest %d zwierzat", current_animals);
}

void leavePark() {
	packet_t tmppacket; //tmppacket ponieważ broadcast message zmienia jego parametry i się psuje synchronizacja
	packet_t pakiet;
	tmppacket.ile_chce_upolowac = do_upolowania;
	pakiet.ile_chce_upolowac = do_upolowania;
	pakiet.rank = rank;
	pakiet.ts = global_ts;
	println("Wychodzę z parku, wysyłam release");
	broadcastMessage(&tmppacket, RELEASE, -1); //-1, bo dla RELEASE ostatni argument nie jest uzywany
	deleteFromQueue(kolejka_licencji, rank);
	addToQueue(kolejka_licencji, &pakiet, RELEASE);
	
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
		tmppacket.ile_chce_upolowac = do_upolowania;
	  	addToQueue(kolejka_licencji, &tmppacket, REQUEST);
	  	answers = 0;
	}
}
