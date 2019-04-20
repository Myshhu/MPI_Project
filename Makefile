all: bank

bank: main.o init.o
	mpic++ main.o init.o -o bank

init.o: init.cpp 
	mpic++ init.cpp -c -Wall

main.o: main.cpp main.hpp
	mpic++ main.cpp -c -Wall

clear: 
	rm *.o bank
