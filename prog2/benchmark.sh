# Usage: ./benchmark.sh
# Description: Compiles the source code, runs the mpi-based bitonic sort program, and outputs the results in a
#              "results" folders, for each configuration of array sizes (32, 256K, 1M, 16M) and processes (1, 2, 4, 8).
# Example: ./benchmark.sh

OUTPUT_FOLDER="results"
NUMBERS_FOLDER="data"
NUMBERS_SIZES="32 256K 1M 16M"
N_PROCS="1 2 4 8"
N_ITERATIONS=15

# Create the output folder
mkdir -p $OUTPUT_FOLDER

# Compile the source code
mpicc -Wall -O3 -o bmprog2 mpiBitonic.c sortUtils.c

# Run the program for each configuration of processes and array sizes
for size in $NUMBERS_SIZES; do
  for procs in $N_PROCS; do
    echo "Running program $N_ITERATIONS times for $size numbers and $procs processes..."
    # Create the output file
    OUTPUT_FILE="$OUTPUT_FOLDER/n$size-p$procs.txt"
    rm -f $OUTPUT_FILE
    touch $OUTPUT_FILE
    # Run the program and save the results
    for i in $(seq 1 $N_ITERATIONS); do
      mpiexec -n $procs ./bmprog2 -f $NUMBERS_FOLDER/datSeq$size.bin | grep -Po 'Time elapsed\s*:\s*\K[0-9.]+' >> $OUTPUT_FILE
    done
  done
done

# Clean-up
rm -f bmprog2
