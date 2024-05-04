# Usage: ./benchmark.sh

OUTPUT_FILE="results.txt"
N_PROCS="2 3 5 9"
N_ITERATIONS=15

for procs in $N_PROCS; do
  echo "Number of workers: $(($procs - 1))" >> $OUTPUT_FILE
  for i in $(seq 1 $N_ITERATIONS); do
    mpiexec -n $procs ./prog1 data/text*.txt | grep -Po 'Elapsed time *: *\K[0-9.]+' >> $OUTPUT_FILE
  done
  echo "\n" >> $OUTPUT_FILE
done