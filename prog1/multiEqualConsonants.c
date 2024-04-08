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
#include "shared.h"
#include "wordUtils.h"

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
void *worker(void *id) {
    uint8_t workerId = *((uint8_t *)id);

    struct ChunkData chunkData;
    int ptr;
    char *currentChar = (char *) malloc(MAX_CHAR_LENGTH * sizeof(char));
    int consOcc[26];
    bool detMultCons;

    while (true) {
        chunkData.nWords = 0;
        chunkData.nWordsWMultCons = 0;
        chunkData.finished = true;
        chunkData.inWord = false;

        retrieveData(workerId, &chunkData);

        if (chunkData.finished) {
            break;
        }

        ptr = 0;
        detMultCons = false;
        memset(consOcc, 0, 26 * sizeof(int));

        char* word = (char *) malloc(MAX_CHAR_LENGTH * sizeof(char));

        while (extractCharFromChunk(chunkData.chunk, currentChar, &ptr) != -1) {
            processChar(word, currentChar, &chunkData.inWord, &chunkData.nWords, &chunkData.nWordsWMultCons, consOcc, &detMultCons);
        }

        // update shared data
        saveResults(&chunkData);

        memset(chunkData.chunk, 0, MAX_CHUNK_SIZE);
    }

    return (void*) EXIT_SUCCESS;
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
    int nThreads = N_WORKERS;
    int nFiles;
    char **fileNames;

    // process command line options
    int opt;
    do {
        opt = getopt(argc, argv, "n:");
        switch (opt) {
            case 'n':
                nThreads = atoi(optarg);
                if (nThreads < 1) {
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

    printf("Number of workers: %d\n\n", nThreads);

    initializeCharMeaning();
    pthread_t threads[nThreads];

    get_delta_time();

    initSharedData(nFiles, fileNames);

    // create nThreads threads
    for (int i = 0; i < nThreads; i++) {
        pthread_create(&threads[i], NULL, worker, &i);
    }

    // join nThreads threads
    for (int i = 0; i < nThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    printResults(nFiles);

    printf("Elapsed time: %f\n", get_delta_time());
    return EXIT_SUCCESS;
}
