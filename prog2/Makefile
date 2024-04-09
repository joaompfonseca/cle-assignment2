.DEFAULT := compile

compile:
	@echo "Compiling..."
	mpicc -Wall -O3 -o prog2 multiBitonic.c

test: compile
	@echo "Testing..."
	mpiexec -n 4 ./prog2 -f data/datSeq256K.bin