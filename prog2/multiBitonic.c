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
            "-n number_of_processes : number of processes (minimum is 2, must be in the form n=2^i+1, i>=0, meaning 2^i workers and 1 distributor)\n"
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
    // open the file
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        fprintf(stderr, "[DIST] Could not open file %s\n", file_path);
        return EXIT_FAILURE;
    }
    // read the size of the array
    int size;
    if (fread(&size, sizeof(int), 1, file) != 1) {
        fprintf(stderr, "[DIST] Could not read the size of the array\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    // size must be power of 2
    if ((size & (size - 1)) != 0) {
        fprintf(stderr, "[DIST] The size of the array must be a power of 2\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    fprintf(stdout, "[DIST] Array size: %d\n", size);
    // allocate memory for the array
    int *arr = (int *)malloc(size * sizeof(int));
    if (arr == NULL) {
        fprintf(stderr, "[DIST] Could not allocate memory for the array\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    // load array into memory
    int num, ni = 0;
    while (fread(&num, sizeof(int), 1, file) == 1 && ni < size) {
        arr[ni++] = num;
    }
    // close the file
    fclose(file);

    // START TIME
    get_delta_time();

    if (size > 1) {
        int type;
        int count = size / n_workers;

        // allocate memory for the sub-array
        int *sub_arr = (int *)malloc(count * sizeof(int));
        if (sub_arr == NULL) {
            fprintf(stderr, "[DIST] Could not allocate memory for the sub-array\n");
            free(arr);
            return EXIT_FAILURE;
        }
        // divide the array into n_workers parts
        // make each worker process bitonic sort one part
        for (int i = 0; i < n_workers; i++) {
            // set the type of task
            type = SORT_TASK;
            // copy the sub-array
            memcpy(sub_arr, arr + i * count, count * sizeof(int));
            // direction of the sub-sort
            int low_index = i * count;
            int sub_direction = (((low_index / count) % 2 == 0) != 0) == direction;
            // send the task to the worker process
            MPI_Send(&type, 1, MPI_INT, i + 1, 0, MPI_COMM_WORLD);
            MPI_Send(&sub_direction, 1, MPI_INT, i + 1, 0, MPI_COMM_WORLD);
            MPI_Send(&count, 1, MPI_INT, i + 1, 0, MPI_COMM_WORLD);
            MPI_Send(sub_arr, count, MPI_INT, i + 1, 0, MPI_COMM_WORLD);
        }
        fprintf(stdout, "[DIST] Bitonic sort of %d parts of size %d\n", n_workers, count);

        // save the sorted parts
        for (int i = 0; i < n_workers; i++) {
            MPI_Recv(arr + i * count, count, MPI_INT, i + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        MPI_Barrier(MPI_COMM_WORLD);

        // perform a bitonic merge of the sorted parts
        // make each worker process bitonic merge one part, discard unused processes
        for (count *= 2; count <= size; count *= 2) {
            // reallocate memory for the sub-array
            sub_arr = (int *)realloc(sub_arr, count * sizeof(int));
            if (sub_arr == NULL) {
                fprintf(stderr, "[DIST] Could not reallocate memory for the sub-array\n");
                free(arr);
                return EXIT_FAILURE;
            }
            int n_merge_tasks = size / count;
            // merge tasks
            for (int i = 0; i < n_merge_tasks; i++) {
                // set the type of task
                type = MERGE_TASK;
                // copy the sub-array
                memcpy(sub_arr, arr + i * count, count * sizeof(int));
                // direction of the sub-merge
                int low_index = i * count;
                int sub_direction = (((low_index / count) % 2 == 0) != 0) == direction;
                // send the task to the worker process
                MPI_Send(&type, 1, MPI_INT, i + 1, 0, MPI_COMM_WORLD);
                MPI_Send(&sub_direction, 1, MPI_INT, i + 1, 0, MPI_COMM_WORLD);
                MPI_Send(&count, 1, MPI_INT, i + 1, 0, MPI_COMM_WORLD);
                MPI_Send(sub_arr, count, MPI_INT, i + 1, 0, MPI_COMM_WORLD);
            }
            // wait tasks
            for (int i = n_merge_tasks; i < n_workers; i++) {
                // set the type of task
                type = WAIT_TASK;
                // send the task to the worker process
                MPI_Send(&type, 1, MPI_INT, i + 1, 0, MPI_COMM_WORLD);
            }
            fprintf(stdout, "[DIST] Bitonic merge of %d parts of size %d\n", n_merge_tasks, count);

            // save the merged sub-arrays
            for (int i = 0; i < n_merge_tasks; i++) {
                MPI_Recv(arr + i * count, count, MPI_INT, i + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }

            MPI_Barrier(MPI_COMM_WORLD);
        }

        free(sub_arr);

        // termination task to all worker processes
        for (int i = 0; i < n_workers; i++) {
            // set the type of task
            type = TERMINATION_TASK;
            // send the task to the worker process
            MPI_Send(&type, 1, MPI_INT, i + 1, 0, MPI_COMM_WORLD);
        }
    }

    // END TIME
    fprintf(stdout, "[TIME] Time elapsed: %.9f seconds\n", get_delta_time());

    *ret_array = arr;
    *ret_size = size;
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
    int n_workers = 0;

    // process program arguments
    int opt;
    do {
        switch ((opt = getopt(argc, argv, "f:h"))) {
            case 'f':
                file_path = optarg;
                if (file_path == NULL) {
                    fprintf(stderr, "[MAIN] Invalid file name\n");
                    printUsage(cmd_name);
                    return EXIT_FAILURE;
                }
                break;
            case 'h':
                printUsage(cmd_name);
                return EXIT_SUCCESS;
            case '?':
                fprintf(stderr, "[MAIN] Invalid option -%c\n", optopt);
                printUsage(cmd_name);
                return EXIT_FAILURE;
            case -1:
                break;
        }
    } while (opt != -1);
    if (file_path == NULL) {
        fprintf(stderr, "[MAIN] Input file not specified\n");
        printUsage(cmd_name);
        return EXIT_FAILURE;
    }

    // mpi arguments
    int mpi_rank, mpi_size;

    // initialize mpi
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    n_workers = mpi_size - 1;
    if (mpi_rank == 0 && (n_workers < 1 || (n_workers & (n_workers - 1)) != 0)) {
        fprintf(stderr, "[MAIN] Invalid number of processes\n");
        printUsage(cmd_name);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        return EXIT_FAILURE;
    }

    if (mpi_rank == 0) {
        // print program arguments
        fprintf(stdout, "[MAIN] Input file: %s\n", file_path);
        fprintf(stdout, "[MAIN] Workers: %d\n", n_workers);

        int *arr, size;

        // run distributor lifecycle
        if (bitonic_distributor(file_path, DESCENDING, n_workers, &arr, &size) != 0) {
            fprintf(stderr, "[MAIN] Error in distributor lifecycle\n");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            return EXIT_FAILURE;
        }

        // check if the array is sorted
        for (int i = 0; i < size - 1; i++) {
            if (arr[i] < arr[i + 1]) {
                fprintf(stderr, "[MAIN] Error in position %d between element %d and %d\n",
                        i, arr[i], arr[i + 1]);
                free(arr);
                return EXIT_FAILURE;
            }
        }
        fprintf(stdout, "[MAIN] The array is sorted, everything is OK! :)\n");
    } else {
        // run worker lifecycle
        if (bitonic_worker(mpi_rank) != 0) {
            fprintf(stderr, "[MAIN] Error in worker lifecycle\n");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            return EXIT_FAILURE;
        }
    }

    MPI_Finalize();

    return EXIT_SUCCESS;
}
