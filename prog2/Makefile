.DEFAULT := compile

compile:
	@echo "Compiling..."
	mpicc -Wall -O3 -o prog2 mpiBitonic.c sortUtils.c

test: compile
	@echo "Testing..."
	mpiexec -n 8 ./prog2 -f data/datSeq256K.bin