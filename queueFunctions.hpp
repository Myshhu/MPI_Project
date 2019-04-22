/* Operacje na kolejkach */
void addToQueue(std::vector <element_kolejki> &kolejka, packet_t *pakiet, int numer_statusu);
void sortQueue(std::vector <element_kolejki> &kolejka);
void queueChanged(std::vector <element_kolejki> &kolejka);
void printQueue(std::vector <element_kolejki> &kolejka);
void deleteFromQueue(std::vector <element_kolejki> &kolejka, int numer_procesu);
/*************************/

extern bool chce_do_parku;
