/**
 * \file multiEqualConsonants.c (implementation file)
 * 
 * \brief Assignment 1.1: count words with multiple equal consonants.
 * 
 * This file contains the implementation of a program that reads several files containing Portuguese texts, and for each one, counts the number of words and those with at least two equal consonants.
 * 
 *  \author João Fonseca - March 2024
 *  \author Rafael Gonçalves - March 2024
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mpi.h>
#include <stdint.h>
#include <getopt.h>
#include "wordUtils.h"
#include "fileDataHandler.h"

#define N_WORKERS 2 // default number of workers
#define CLOCK_MONOTONIC 1 // for clock_gettime

/**
 *  \brief Gets the time elapsed since the last call to this function.
 *
 *  \return time elapsed in seconds
 */
static double get_delta_time(void) {
    static struct timespec t0, t1;
    
    // t0 is assigned the value of t1 from the previous call. if there is no previous call, t0 = t1 = 0
    t0 = t1;

    if (clock_gettime (CLOCK_MONOTONIC, &t1) != 0) {
        perror ("clock_gettime");
        exit(1);
    }
    return (double) (t1.tv_sec - t0.tv_sec) + 1.0e-9 * (double) (t1.tv_nsec - t0.tv_nsec);
}

/**
 *  \brief Worker thread function that executes tasks.
 *
 *  Lifecycle loop:
 * - retrieve a chunk of data
 * - process the chunk
 * - save the partial results
 * 
 * \param id pointer to the worker id
 */
void distributeChunks(int nProcesses) {
    chunk_data chunkData;
    int nWords, nWordsWMultCons, fileIndex;
    bool finished;
    int nextWorker = 0; // first nextWorker is 1 (0 is the main thread)

    while (true) {
        if (nextWorker == nProcesses - 1) {
            nextWorker = 1;
            
            // receive results from workers
            for (int i = 1; i < nProcesses; i++) {
                MPI_Recv(&nWords, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&nWordsWMultCons, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&fileIndex, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                // update final results
                saveResults(nWords, nWordsWMultCons, fileIndex);
            }

        } else {
            nextWorker++;
        }

        chunkData.nWords = 0;
        chunkData.nWordsWMultCons = 0;
        chunkData.finished = true;
        chunkData.inWord = false;

        retrieveData(&chunkData);
        finished = chunkData.finished;

        if (finished) {
            break;
        }

        // signal worker to start receiving the rest of the object
        // printf("Main: waking up worker %d\n", nextWorker);
        MPI_Send(&finished, 1, MPI_INT, nextWorker, 0, MPI_COMM_WORLD);

        // printf("Main: sending chunk to worker %d\n", nextWorker);
        MPI_Send(&chunkData.chunkSize, 1, MPI_INT, nextWorker, 0, MPI_COMM_WORLD);
        MPI_Send(&chunkData.fileIndex, 1, MPI_INT, nextWorker, 0, MPI_COMM_WORLD);
        MPI_Send(chunkData.chunk, chunkData.chunkSize, MPI_CHAR, nextWorker, 0, MPI_COMM_WORLD);

        memset(chunkData.chunk, 0, MAX_CHUNK_SIZE);
    }

    // signal workers to stop receiving
    for (int i = 1; i < nProcesses; i++) {
        // printf("Main: sending signal to stop to worker %d\n", i);
        MPI_Send(&finished, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
    }
}


void workerRoutine(int rank) {
    // chunk_data chunkMetaData;
    int nWords, nWordsWMultCons, fileIndex, chunkSize;
    char* chunk;
    int ptr;
    char *currentChar = (char *) malloc(MAX_CHAR_LENGTH * sizeof(char));

    int consOcc[26];
    bool detMultCons, finished, inWord;

    while (true) {
        MPI_Recv(&finished, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        // printf("Worker %d: received signal to start\n", rank);

        if (finished) {
            // printf("Worker %d: received signal to stop\n", rank);
            break;
        }

        // MPI_Recv(&chunkMetaData, sizeof(chunkMetaData), MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        // printf("Worker %d: received chunk metada\n", rank);

        // MPI_Recv(chunk, chunkMetaData.chunkSize, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        // printf("Worker %d: received chunk\n", rank);

        MPI_Recv(&chunkSize, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&fileIndex, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        chunk = (char *) malloc((chunkSize + 1) * sizeof(char));
        MPI_Recv(chunk, chunkSize, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        nWords = 0;
        nWordsWMultCons = 0;
        inWord = false;

        ptr = 0;
        detMultCons = false;
        memset(consOcc, 0, 26 * sizeof(int));

        // printf("Worker %d: processing chunk\n", rank);

        chunk[chunkSize] = '\0';

        while (extractCharFromChunk(chunk, currentChar, &ptr) != -1) {
            processChar(currentChar, &inWord, &nWords, &nWordsWMultCons, consOcc, &detMultCons);
        }

        // send back partial results
        // printf("Worker %d: sending partial results\n", rank);
        MPI_Send(&nWords, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        MPI_Send(&nWordsWMultCons, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        MPI_Send(&fileIndex, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }
    
}

/**
 *  \brief Main function.
 *
 *  Lifecycle:
 * - process command line options
 * - allocate memory for the shared area
 * - create worker threads
 * - wait for threads to finish
 * - print the final results
 *
 *  \param argc number of arguments
 *  \param argv array of arguments
 *  \return EXIT_SUCCESS if the program runs successfully, EXIT_FAILURE otherwise
 */
int main(int argc, char *argv[]) {
    // program arguments
    char *cmd_name = argv[0];
    int nFiles, rank, size, nProcesses = N_WORKERS;
    char **fileNames;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 2) {
        fprintf(stderr, "Error: This program requires at least 2 processes\n");
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    initializeCharMeaning(); // initialize the charMeaning array in all processes

    if (rank == 0) {
        // process command line options
        int opt;
        do {
            opt = getopt(argc, argv, "n:");
            switch (opt) {
                case 'n':
                    nProcesses = atoi(optarg);
                    if (nProcesses < 1) {
                        fprintf(stderr, "[MAIN] Invalid number of worker threads\n");
                        fprintf(stderr, "Usage: %s [-n n_workers] file1.txt file2.txt ...\n", cmd_name);
                        return EXIT_FAILURE;
                    }
                    break;
                case -1:
                    if (optind < argc) {
                        // process remaining arguments
                        nFiles = argc - optind;
                        printf("Number of files: %d\n", nFiles);
                        fileNames = (char **)malloc((nFiles + 1) * sizeof(char *));
                        for (int i = optind; i < argc; i++) {
                            fileNames[i - optind] = argv[i];
                        }
                    }
                    else {
                        fprintf(stderr, "Usage: %s [-n n_workers] file1.txt file2.txt ...\n", cmd_name);
                        exit(EXIT_FAILURE);
                    }
                    break;
                default:
                    fprintf(stderr, "Usage: %s [-n n_workers] file1.txt file2.txt ...\n", cmd_name);
                    exit(EXIT_FAILURE);
            }
        } while (opt != -1);

        printf("Number of processes: %d\n\n", nProcesses);

        get_delta_time();

        initFinalResults(nFiles, fileNames);
        distributeChunks(nProcesses);

        printResults(nFiles);
        printf("Elapsed time: %f\n", get_delta_time());
    }
    else {
        workerRoutine(rank);
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}
