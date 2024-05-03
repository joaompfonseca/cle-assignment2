# CLE Assignment 2

## Description

The assignment goal was to develop a mpi-based implementation of the two general problems given in the practical classes.

The first problem consisted of developing a program that counts the number of words and those with at least two equal consonants in several files containing text in Portuguese.

The second problem consisted of developing a sorting algorithm to sort an array of integers in a descending order. The chosen algorithm was bitonic sort since it provides good parallel decomposition properties.

**Course:** Large Scale Computing (2023/2024).

## 1. Multithreaded equal consonants

### Compile and execute

- Run `cd prog1` in root to change to the program's directory.
- Run `make` to compile the program.
- Run `mpiexec MPI_REQUIRED ./prog1 [file1_path] ... [fileN_path] OPTIONAL` to execute the program.

### MPI required arguments

- `-n`: number of processes (minimum is 2).

### Optional arguments

- `-h`: shows how to use the program.

### Example

`mpiexec -n 4 ./prog1 file1.txt file2.txt`

## 2. Multithreaded bitonic sort

### Compile and execute

- Run `cd prog2` in root to change to the program's directory.
- Run `make` to compile the program.
- Run `mpiexec MPI_REQUIRED ./prog2 REQUIRED OPTIONAL` to execute the program.

### MPI required arguments

- `-n`: number of processes (minimum is 1, must be power of 2).

### Required arguments

- `-f input_file_path`: path to the input file with numbers (string).

### Optional arguments

- `-h`: shows how to use the program.

### Example

`mpiexec -n 8 ./prog2 -f data/datSeq256K.bin`

## Authors

- João Fonseca, 103154
- Rafael Gonçalves, 102534
