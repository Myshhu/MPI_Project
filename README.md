#MPI_Project

export OMPI_CXX=g++-7
make
mpirun --allow-run-as-root --hostfile hostfile -np 4 ./bank
