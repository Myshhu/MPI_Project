all: bank

bank: main.o init.o queueFunctions.o
	mpic++ main.o init.o queueFunctions.o -o bank

init.o: init.cpp 
	mpic++ init.cpp -c -Wall

queueFunctions.o: queueFunctions.cpp queueFunctions.hpp
	mpic++ queueFunctions.cpp -c -Wall

main.o: main.cpp main.hpp
	mpic++ main.cpp -c -Wall

clear: 
	rm *.o bank
