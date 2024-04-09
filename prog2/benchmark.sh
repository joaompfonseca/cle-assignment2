# Usage: ./benchmark.sh
# Description: Compiles the source code, runs the mpi-based bitonic sort program, and outputs the results in a
#              "results" file, for each configuration of array sizes (32, 256K, 1M, 16M) and processes (1, 2, 4, 8).
# Example: ./benchmark.sh

OUTPUT_FILE="results.txt"
FOLDER_NUMBERS="data"
FILE_NUMBERS="datSeq32.bin datSeq256K.bin datSeq1M.bin datSeq16M.bin"
N_PROCS="1 2 4 8"

# Create the output file
rm -f $OUTPUT_FILE
touch $OUTPUT_FILE

# Compile the source code
mpicc -O3 -o bmprog2 mpiBitonic.c

# Run the program for each configuration of processes and array sizes
for file in $FILE_NUMBERS; do
  for procs in $N_PROCS; do
    echo "Running program with file $file and $procs processes..."
    echo "File: $file; Processes: $procs" >> $OUTPUT_FILE
    mpiexec -n $procs ./bmprog2 -f $FOLDER_NUMBERS/$file | grep '^\[TIME\]' >> $OUTPUT_FILE
    echo "" >> $OUTPUT_FILE
  done
done

# Clean-up
rm -f bmprog2
