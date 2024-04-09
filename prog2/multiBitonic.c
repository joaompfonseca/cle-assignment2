/**
 *  \file multiBitonic.c (implementation file)
 *
 *  \brief Assignment 1.2: multithreaded bitonic sort.
 *
 *  This file contains the definition of the multithreaded bitonic sort algorithm.
 *
 *  \author João Fonseca - March 2024
 *  \author Rafael Gonçalves - March 2024
 */

#include <getopt.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "const.h"
#include "shared.c"

/**
 *  \brief Prints the usage of the program.
 *
 *  \param cmd_name name of the command that started the program
 */
void printUsage(char *cmd_name) {
    fprintf(stderr,
            "Usage: mpiexec MPI_REQUIRED %s REQUIRED OPTIONAL\n"
            "MPI_REQUIRED\n"
            "-n number_of_processes : number of processes (minimum is 1, must be power of 2)\n"
            "REQUIRED\n"
            "-f input_file_path     : input file with numbers\n"
            "OPTIONAL\n"
            "-h                     : shows how to use the program\n",
            cmd_name);
}

/**
 *  \brief Gets the time elapsed since the last call to this function.
 *
 *  \return time elapsed in seconds
 */
static double get_delta_time(void) {
    static struct timespec t0, t1;

    // t0 is assigned the value of t1 from the previous call. if there is no previous call, t0 = t1 = 0
    t0 = t1;

    if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
        fprintf(stderr, "[TIME] Could not get the time\n");
        exit(EXIT_FAILURE);
    }
    return (double)(t1.tv_sec - t0.tv_sec) + 1.0e-9 * (double)(t1.tv_nsec - t0.tv_nsec);
}

/**
 * \brief Free memory from all pointers in the list.
 *
 * \param pointers list of pointers to be freed
 * \param size number of pointers in the list
 */
void free_all(void **pointers, int size) {
    for (int i = 0; i < size; i++) {
        free(pointers[i]);
    }
}

/**
 *  \brief Merges two halves of an integer array in the desired order.
 *
 *  \param arr array to be merged
 *  \param low_index index of the first element of the array
 *  \param count number of elements in the array
 *  \param direction 0 for descending order, 1 for ascending order
 */
void bitonic_merge(int *arr, int low_index, int count, int direction) {  // NOLINT(*-no-recursion)
    if (count <= 1) return;
    int half = count / 2;
    // move the numbers to the correct half
    for (int i = low_index; i < low_index + half; i++) {
        if (direction == (arr[i] > arr[i + half])) {
            int temp = arr[i];
            arr[i] = arr[i + half];
            arr[i + half] = temp;
        }
    }
    // merge left half
    bitonic_merge(arr, low_index, half, direction);
    // merge right half
    bitonic_merge(arr, low_index + half, half, direction);
}

/**
 *  \brief Sorts an integer array in the desired order.
 *
 *  \param arr array to be sorted
 *  \param low_index index of the first element of the array
 *  \param count number of elements in the array
 *  \param direction 0 for descending order, 1 for ascending order
 */
void bitonic_sort(int *arr, int low_index, int count, int direction) {  // NOLINT(*-no-recursion)
    if (count <= 1) return;
    int half = count / 2;
    // sort left half in ascending order
    bitonic_sort(arr, low_index, half, ASCENDING);
    // sort right half in descending order
    bitonic_sort(arr, low_index + half, half, DESCENDING);
    // merge the two halves
    bitonic_merge(arr, low_index, count, direction);
}

/**
 *  \brief Argument structure for the worker threads.
 */
typedef struct {
    int index;
    shared_t *shared;
} bitonic_worker_arg_t;

/**
 *  \brief Worker thread function that executes tasks.
 *
 *  Lifecycle loop:
 *  - get a task from the shared area
 *  - if the task is a sort task, sort the array
 *  - if the task is a merge task, merge the array
 *  - if the task is a termination task, finish the thread
 *
 *  \param arg pointer to the argument structure, that contains the index of the worker thread and the shared area
 */
int bitonic_worker(int id) {
    int type, direction, count;
    int *arr = (int *)malloc(sizeof(int));

    fprintf(stdout, "[WORKER-%d] started\n", id);

    while (1) {
        MPI_Recv(&type, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        fprintf(stdout, "[WORKER-%d] received task type=%d\n", id, type);

        if (type == SORT_TASK) {
            MPI_Recv(&direction, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            fprintf(stdout, "[WORKER-%d] received task direction=%d\n", id, direction);

            MPI_Recv(&count, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            fprintf(stdout, "[WORKER-%d] received task count=%d\n", id, count);

            arr = (int *)realloc(arr, count * sizeof(int));
            if (arr == NULL) {
                fprintf(stderr, "[WORKER-%d] Could not allocate memory for the array\n", id);
                free(arr);
                return EXIT_FAILURE;
            }
            MPI_Recv(arr, count, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            fprintf(stdout, "[WORKER-%d] received task array\n", id);

            bitonic_sort(arr, 0, count, direction);

            MPI_Send(arr, count, MPI_INT, 0, 0, MPI_COMM_WORLD);
            MPI_Barrier(MPI_COMM_WORLD);

        } else if (type == MERGE_TASK) {
            MPI_Recv(&direction, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            fprintf(stdout, "[WORKER-%d] received task direction=%d\n", id, direction);

            MPI_Recv(&count, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            fprintf(stdout, "[WORKER-%d] received task count=%d\n", id, count);

            arr = (int *)realloc(arr, count * sizeof(int));
            if (arr == NULL) {
                fprintf(stderr, "[WORKER-%d] Could not allocate memory for the array\n", id);
                free(arr);
                return EXIT_FAILURE;
            }
            MPI_Recv(arr, count, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            fprintf(stdout, "[WORKER-%d] received task array\n", id);

            bitonic_merge(arr, 0, count, direction);

            MPI_Send(arr, count, MPI_INT, 0, 0, MPI_COMM_WORLD);
            MPI_Barrier(MPI_COMM_WORLD);
        } else if (type == WAIT_TASK) {
            MPI_Barrier(MPI_COMM_WORLD);
        } else {
            // termination task
            break;
        }
    }

    free(arr);
    return EXIT_SUCCESS;
}

/**
 *  \brief Distributor thread function that assigns tasks to worker threads.
 *
 *  Lifecycle:
 *  - read the array from the file
 *  - divide the array into n_workers parts and assign a sort task to each worker thread
 *  - perform a bitonic merge of the sorted parts and assign a merge task to each worker thread
 *  - terminate worker threads that are not needed anymore
 *
 *  \param arg pointer to the shared area
 */
int bitonic_distributor(char *file_path, int direction, int n_workers, int **ret_array, int *ret_size) {
    return EXIT_SUCCESS;
}

/**
 *  \brief Main function of the program.
 *
 *  Lifecycle:
 *  - process command line options
 *  - allocate memory for the shared area, configuration, tasks, list of tasks and list of threads done
 *  - initialize the configuration, tasks and shared area
 *  - create distributor thread
 *  - create worker threads
 *  - wait for threads to finish
 *  - check if the array is sorted
 *
 *  \param argc number of command line arguments
 *  \param argv array of command line arguments
 *
 *  \return EXIT_SUCCESS if the array is sorted, EXIT_FAILURE otherwise
 */
int main(int argc, char *argv[]) {
    // program arguments
    char *cmd_name = argv[0];
    char *file_path = NULL;

    // process program arguments
    int opt;
    do {
        switch ((opt = getopt(argc, argv, "f:h"))) {
            case 'f':
                file_path = optarg;
                if (file_path == NULL) {
                    fprintf(stderr, "Invalid file name\n");
                    printUsage(cmd_name);
                    return EXIT_FAILURE;
                }
                break;
            case 'h':
                printUsage(cmd_name);
                return EXIT_SUCCESS;
            case '?':
                fprintf(stderr, "Invalid option -%c\n", optopt);
                printUsage(cmd_name);
                return EXIT_FAILURE;
            case -1:
                break;
        }
    } while (opt != -1);
    if (file_path == NULL) {
        fprintf(stderr, "Input file not specified\n");
        printUsage(cmd_name);
        return EXIT_FAILURE;
    }

    // mpi arguments
    int mpi_rank, mpi_size;

    // initialize mpi
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    if (mpi_rank == 0 && (mpi_size & (mpi_size - 1)) != 0) {
        fprintf(stderr, "Invalid number of processes\n");
        printUsage(cmd_name);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    int direction = DESCENDING;
    int *arr, size;

    if (mpi_rank == 0) {
        // print program arguments
        fprintf(stdout, "Input file: %s\n", file_path);
        fprintf(stdout, "Processes: %d\n", mpi_size);

        // open the file
        FILE *file = fopen(file_path, "rb");
        if (file == NULL) {
            fprintf(stderr, "Could not open file %s\n", file_path);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        // read the size of the array
        int size;
        if (fread(&size, sizeof(int), 1, file) != 1) {
            fprintf(stderr, "Could not read the size of the array\n");
            fclose(file);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        // size must be power of 2
        if ((size & (size - 1)) != 0) {
            fprintf(stderr, "The size of the array must be a power of 2\n");
            fclose(file);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        fprintf(stdout, "Array size: %d\n", size);
        // allocate memory for the array
        arr = (int *)malloc(size * sizeof(int));
        if (arr == NULL) {
            fprintf(stderr, "Could not allocate memory for the array\n");
            fclose(file);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        // load array into memory
        int num, ni = 0;
        while (fread(&num, sizeof(int), 1, file) == 1 && ni < size) {
            arr[ni++] = num;
        }
        // close the file
        fclose(file);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Bcast(&size, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (mpi_rank == 0) {
        // START TIME
        get_delta_time();
    }

    if (size > 1) {
        int count = size / mpi_size;

        // allocate memory for the sub-array
        int *sub_arr = (int *)malloc(count * sizeof(int));
        if (sub_arr == NULL) {
            fprintf(stderr, "[PROC-%d] Could not allocate memory for the sub-array\n", mpi_rank);
            free(arr);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        // divide the array into mpi_size parts
        // make each process bitonic sort one part
        fprintf(stdout, "[PROC-%d] Bitonic sort of %d parts of size %d\n", mpi_rank, mpi_size, count);

        // scatter the array into mpi_size parts
        MPI_Scatter(arr, count, MPI_INT, sub_arr, count, MPI_INT, 0, MPI_COMM_WORLD);

        // direction of the sub-sort
        int low_index = mpi_rank * count;
        int sub_direction = (((low_index / count) % 2 == 0) != 0) == direction;

        // make each process bitonic sort one part
        bitonic_sort(sub_arr, 0, count, sub_direction);
        fprintf(stdout, "[PROC-%d] Direction %d\n", mpi_rank, sub_direction);

        // gather the sorted parts
        MPI_Gather(sub_arr, count, MPI_INT, arr, count, MPI_INT, 0, MPI_COMM_WORLD);

        // perform a bitonic merge of the sorted parts
        // make each process bitonic merge one part, ignore unused processes
        MPI_Comm merge_comm;

        for (count *= 2; count <= size; count *= 2) {
            int n_merge_tasks = size / count;

            // reallocate memory for the sub-array
            sub_arr = (int *)realloc(sub_arr, count * sizeof(int));
            if (sub_arr == NULL) {
                fprintf(stderr, "[PROC-%d] Could not reallocate memory for the sub-array\n", mpi_rank);
                free(arr);
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }

            if (mpi_rank < n_merge_tasks) {
                MPI_Comm_split(MPI_COMM_WORLD, 0, mpi_rank, &merge_comm);
            } else {
                MPI_Comm_split(MPI_COMM_WORLD, MPI_UNDEFINED, mpi_rank, &merge_comm);
            }

            if (merge_comm != MPI_COMM_NULL) {
                fprintf(stdout, "[PROC-%d] Bitonic merge of %d parts of size %d\n", mpi_rank, n_merge_tasks, count);

                // scatter the array into n_merge_tasks parts
                MPI_Scatter(arr, count, MPI_INT, sub_arr, count, MPI_INT, 0, merge_comm);

                // direction of the sub-merge
                int low_index = mpi_rank * count;
                int sub_direction = (((low_index / count) % 2 == 0) != 0) == direction;

                // make each worker process bitonic merge one part
                bitonic_merge(sub_arr, 0, count, sub_direction);
                fprintf(stdout, "[PROC-%d] Direction %d\n", mpi_rank, sub_direction);

                // gather the merged parts
                MPI_Gather(sub_arr, count, MPI_INT, arr, count, MPI_INT, 0, merge_comm);

                MPI_Comm_free(&merge_comm);
            }
        }

        free(sub_arr);
    }

    if (mpi_rank == 0) {
        // END TIME
        fprintf(stdout, "[TIME] Time elapsed: %.9f seconds\n", get_delta_time());
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (mpi_rank == 0) {
        // check if the array is sorted
        for (int i = 0; i < size - 1; i++) {
            if (arr[i] < arr[i + 1]) {
                fprintf(stderr, "Error in position %d between element %d and %d\n", i, arr[i], arr[i + 1]);
                free(arr);
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }
        }
        fprintf(stdout, "The array is sorted, everything is OK! :)\n");
    }

    if (mpi_rank == 0) {
        free(arr);
    }

    MPI_Finalize();

    return EXIT_SUCCESS;
}
