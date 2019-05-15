#include "main.hpp"
#include "init.hpp"
#include "queueFunctions.hpp"

void addToQueue(std::vector <element_kolejki> &kolejka, packet_t *pakiet, int numer_statusu) {
	element_kolejki nowy_element;
	nowy_element.numer_procesu = pakiet->rank;
	nowy_element.zegar_procesu = pakiet->ts;
	nowy_element.typ_komunikatu = numer_statusu;
	nowy_element.to_hunt = pakiet->to_hunt;
	kolejka.push_back(nowy_element);
	queueChanged(kolejka);
}

void sortQueue(std::vector <element_kolejki> &kolejka) {
	std::sort(kolejka.begin(), kolejka.end(), normal_sort());
	std::sort(kolejka.begin(), kolejka.end(), type_sort());
}

void queueChanged(std::vector <element_kolejki> &kolejka) {
	sortQueue(kolejka);
	printQueue(kolejka);
	if(chce_do_parku) {
		tryToEnterPark();
	}
}

void printQueue(std::vector <element_kolejki> &kolejka) {
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
		queue_string += " Liczba zajecy do upolowania: ";
		queue_string += std::to_string(kolejka[i].to_hunt);
		queue_string += "\n";
		//println("---- Proces: %d zegar: %d typ %s", kolejka[i].numer_procesu, kolejka[i].zegar_procesu, returnTypeString(kolejka[i].typ_komunikatu).c_str());
	}
	queue_string += "---- Koniec kolejki \n";
	println("%s", queue_string.c_str());
	//println("---- Koniec kolejki");
}

void deleteFromQueue(std::vector <element_kolejki> &kolejka, int numer_procesu) {
	for(unsigned int i = 0; i < kolejka.size(); i++) {
		if(kolejka[i].numer_procesu == numer_procesu) {
			kolejka.erase(kolejka.begin() + i);
		}
	}
}
