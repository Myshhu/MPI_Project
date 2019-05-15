#include "main.hpp"
#include "init.hpp"



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
    const int nitems = FIELDNO; // Struktura ma FIELDNO elementów - przy dodaniu pola zwiększ FIELDNO w main.h !
    int blocklengths[FIELDNO] = {1,1,1,1,1}; /* tu zwiększyć na [4] = {1,1,1,1} gdy dodamy nowe pole */
    MPI_Datatype typy[FIELDNO] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT}; /* tu dodać typ nowego pola (np MPI_BYTE, MPI_INT) */
    MPI_Aint offsets[FIELDNO];

    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, rank);
    offsets[2] = offsetof(packet_t, dst);
    offsets[3] = offsetof(packet_t, src);
    offsets[4] = offsetof(packet_t, ile_chce_upolowac);
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
    pthread_mutex_destroy(&konto_mut);
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

void sendPacket(packet_t *data, int dst, int type, int REQUEST_ts) //TODO: Zmienic na argument domyslny
{
    data->ts = global_ts; //TODO: Czy przy broadcastcie tez zmieniamy zegar??? czy zostawiamy
    // na zegarze który był przy wysyłaniu pierwszego pakietu z broadcasta
    // bo w przeciwnym wypadku proces któremu pierwszemu wysyłamy release dostanie on wiadomość z
    // naszym zegarem jako np. 10, a proces któremu jako drugiemu wysyłamy release dostanie 
    //wiadomość z zegarem już jako 11. //Czy to wpływa na program??
    //przy requestach jest to zabezpieczone warunkiem niżej
    if(type == REQUEST) { 
    //sendPacket zmienia globalny zegar, a podczas requestu chcemy wysłać do wszystkich pakiet
    // z zegarem, który był przed wysłaniem pierwszego requesta
    	data->ts = REQUEST_ts;
    } 
    data->rank = rank;
    global_ts++;
    println("Wysylam pakiet typu %s do procesu %d, zwiekszam swoj zegar z %d na %d\n", 	returnTypeString(type).c_str(), dst, global_ts - 1, global_ts);
    MPI_Send(data, 1, MPI_PAKIET_T, dst, type, MPI_COMM_WORLD);
}


