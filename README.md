#MPI_Project

make
mpirun --allow-run-as-root --hostfile hostfile -np 4 ./bank
