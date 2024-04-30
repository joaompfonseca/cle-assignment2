/**
 *  \file mpiBitonic.c (implementation file)
 *
 *  \brief Assignment 2.2: mpi-based bitonic sort.
 *
 *  This file contains the definition of the mpi-based bitonic sort algorithm.
 *
 *  \author João Fonseca - March 2024
 *  \author Rafael Gonçalves - March 2024
 */

#include <assert.h>
#include <getopt.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "const.h"
#include "sortUtils.h"

/**
 *  \brief Prints the usage of the program.
 *
 *  \param cmd_name name of the program file
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
 *  \brief Main function of the program.
 *
 *  Lifecycle:
 *  - initialize mpi variables
 *  - rank 0: process program arguments
 *  - rank 0: read the array from the file
 *  - inilialize mpi group
 *  - rank 0: broadcast the size of the array
 *  - rank 0: start time
 *  - mpi scatter/gather to bitonic sort each part of the array
 *  - group processes involved in merge tasks
 *  - terminate processes not involved
 *  - mpi scatter/gather to bitonic merge each part of the array until it's sorted
 *  - rank 0: stop time
 *  - rank 0: check if the array is sorted
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

    // mpi arguments
    int mpi_rank, mpi_size;

    // mpi comm and group
    MPI_Comm curr_comm = MPI_COMM_WORLD, next_comm;
    MPI_Group curr_group, next_group;
    int *group_members = NULL;

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
    int *arr = NULL, size;

    if (mpi_rank == 0) {
        // process program arguments
        int opt;
        do {
            switch ((opt = getopt(argc, argv, "f:h"))) {
                case 'f':
                    file_path = optarg;
                    if (file_path == NULL) {
                        fprintf(stderr, "Invalid file name\n");
                        printUsage(cmd_name);
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                    }
                    break;
                case 'h':
                    printUsage(cmd_name);
                    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                case '?':
                    fprintf(stderr, "Invalid option -%c\n", optopt);
                    printUsage(cmd_name);
                    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                case -1:
                    break;
            }
        } while (opt != -1);
        if (file_path == NULL) {
            fprintf(stderr, "Input file not specified\n");
            printUsage(cmd_name);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        // print program arguments
        fprintf(stdout, "%-16s : %s\n", "Input file", file_path);
        fprintf(stdout, "%-16s : %d\n", "Processes", mpi_size);

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
        fprintf(stdout, "%-16s : %d\n", "Array size", size);
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

    // intialize process group
    group_members = (int *)malloc(mpi_size * sizeof(int));
    if (group_members == NULL) {
        fprintf(stderr, "[PROC-%d] Could not allocate memory for the process group\n", mpi_rank);
        if (mpi_rank == 0) free(arr);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    for (int i = 0; i < mpi_size; i++) {
        group_members[i] = i;
    }

    // initialize mpi group and comm
    MPI_Comm_group(curr_comm, &curr_group);

    // broadcast the size of the array
    MPI_Bcast(&size, 1, MPI_INT, 0, curr_comm);

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
            if (mpi_rank == 0) free(arr);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        /* divide the array into mpi_size parts
           make each process bitonic sort one part */

        // fprintf(stdout, "[PROC-%d] Bitonic sort of %d parts of size %d\n", mpi_rank, mpi_size, count);

        // scatter the array into mpi_size parts
        MPI_Scatter(arr, count, MPI_INT, sub_arr, count, MPI_INT, 0, curr_comm);

        // direction of the sub-sort
        int sub_direction = (mpi_rank % 2 == 0) == direction;

        // make each process bitonic sort one part
        bitonic_sort(sub_arr, 0, count, sub_direction);

        // gather the sorted parts
        MPI_Gather(sub_arr, count, MPI_INT, arr, count, MPI_INT, 0, curr_comm);

        /* perform a bitonic merge of the sorted parts
           make each process bitonic merge one part */

        for (count *= 2; count <= size; count *= 2) {
            int n_merge_tasks = size / count;

            // reallocate memory for the sub-array
            sub_arr = (int *)realloc(sub_arr, count * sizeof(int));
            if (sub_arr == NULL) {
                fprintf(stderr, "[PROC-%d] Could not reallocate memory for the sub-array\n", mpi_rank);
                free(sub_arr);
                if (mpi_rank == 0) free(arr);
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }

            // group processes involved in merge tasks
            MPI_Group_incl(curr_group, n_merge_tasks, group_members, &next_group);
            MPI_Comm_create(curr_comm, next_group, &next_comm);
            curr_group = next_group;
            curr_comm = next_comm;
            
            // terminate processes not involved
            if (mpi_rank >= n_merge_tasks) {
                break;
            }

            // set communicator size
            MPI_Comm_size(curr_comm, &n_merge_tasks);

            // fprintf(stdout, "[PROC-%d] Bitonic merge of %d parts of size %d\n", mpi_rank, n_merge_tasks, count);

            // scatter the array into n_merge_tasks parts
            MPI_Scatter(arr, count, MPI_INT, sub_arr, count, MPI_INT, 0, curr_comm);

            // direction of the sub-merge
            int sub_direction = (mpi_rank % 2 == 0) == direction;

            // make each worker process bitonic merge one part
            bitonic_merge(sub_arr, 0, count, sub_direction);

            // gather the merged parts
            MPI_Gather(sub_arr, count, MPI_INT, arr, count, MPI_INT, 0, curr_comm);
        }

        free(sub_arr);
    }

    if (mpi_rank == 0) {
        // END TIME
        fprintf(stdout, "%-16s : %.9f seconds\n", "Time elapsed", get_delta_time());

        // check if the array is sorted
        for (int i = 0; i < size - 1; i++) {
            if (arr[i] < arr[i + 1]) {
                fprintf(stderr, "Error in position %d between element %d and %d\n", i, arr[i], arr[i + 1]);
                free(arr);
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }
        }
        fprintf(stdout, "The array is sorted, everything is OK! :)\n");

        free(arr);
    }

    // fprintf(stdout, "[PROC-%d] Finished\n", mpi_rank);

    MPI_Finalize();

    return EXIT_SUCCESS;
}
