/**
 *  \file mpiEqualConsonants.c (implementation file)
 *
 *  \brief Assignment 2.1: mpi-based equal consonants.
 *
 *  This file contains the definition of the mpi-based equal consonants algorithm.
 *
 *  \author João Fonseca
 *  \author Rafael Gonçalves
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mpi.h>
#include <stdint.h>
#include <getopt.h>
#include <stddef.h>
#include "wordUtils.h"

#define CLOCK_MONOTONIC 1 // for clock_gettime


/** \brief Structure that represents the final results of each file */
typedef struct {
    char *fileName;
    int nWords;
    int nWordsWMultCons;
    FILE *fp;
} final_file_results;

/** \brief Structure that represents the results of a processed chunk */
typedef struct {
    int nWords;
    int nWordsWMultCons;
} partial_results;

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
 * \brief Dispatcher lifecycle:
 * - Receive work requests from workers
 * - Send chunks to workers
 * - Receive chunk results from workers
 * - Update final results of each file
 * 
 * \param finalFileData array with final results of each file
 * \param nProcesses number of processes (including the dispatcher)
 * \param nFiles number of files
 */
void distributeChunks(final_file_results *finalFileData, int nProcesses, int nFiles) {
    int size = nProcesses - 1; // number of worker processes
    int currentFile = 0;
    int numFinishedWorkers = 0;

    int workerRank;
    chunk_data chunkData; // chunk data to be sent to workers
    partial_results recvData[size]; // partial results received from workers
    bool allMsgRec, recVal, msgRec[size], finished[size]; // flags to control message reception and worker status
    MPI_Request reqAskForWork[size], reqSendLength[size], reqSendChunk[size], reqRecvResults[size]; // MPI requests
    int workerCurrentFile[size]; // array to store the current file being processed by each worker

    // initialize the status of workers
    for (int i = 0; i < size; i++) {
        finished[i] = false;
    }

    while (numFinishedWorkers < size) {
        // receive work requests from workers
        for (int i = 1; i < nProcesses; i++) {
            MPI_Irecv(&workerRank, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &reqAskForWork[i - 1]);
            msgRec[i-1] = false;
        }

        // test if the dispatcher has received a request from all workers
        // for each requester, send a chunk
        do {
            allMsgRec = true;
            for (int i = 1; i < nProcesses; i++) {
                if (!msgRec[i-1]) {
                    recVal = false;
                    MPI_Test(&reqAskForWork[i - 1], (int *)&recVal, MPI_STATUS_IGNORE);
                    if (recVal) {
                        msgRec[i-1] = true;
                        chunkData.chunk = (char *)malloc((MAX_CHUNK_SIZE + 1) * sizeof(char)); // +1 for null terminator
                        chunkData.chunkSize = 0;

                        if (currentFile == nFiles) {
                            MPI_Isend(&chunkData.chunkSize, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &reqSendLength[i - 1]);
                            numFinishedWorkers++;
                            finished[i-1] = true;
                            continue;
                        }

                        if (finalFileData[currentFile].fp == NULL) {
                            if ((finalFileData[currentFile].fp = fopen(finalFileData[currentFile].fileName, "rb")) == NULL) {
                                perror("Error opening file");
                                exit(EXIT_FAILURE);
                            }
                        }

                        workerCurrentFile[i-1] = currentFile;

                        retrieveData(finalFileData[currentFile].fp, &chunkData);
                        if (chunkData.finished) {
                            fclose(finalFileData[currentFile].fp);
                            currentFile++;
                        }

                        // send chunk to worker
                        MPI_Isend(&chunkData.chunkSize, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &reqSendLength[i - 1]);
                        MPI_Isend(chunkData.chunk, chunkData.chunkSize, MPI_CHAR, i, 0, MPI_COMM_WORLD, &reqSendChunk[i - 1]);

                        memset(chunkData.chunk, 0, MAX_CHUNK_SIZE);
                        chunkData.chunkSize = 0; // reset chunk size
                    } else {
                        allMsgRec = false;
                    }
                }
            }
        } while (!allMsgRec && numFinishedWorkers < size);

        // receive results from all active workers
        for (int i = 1; i < nProcesses; i++) {
            if (finished[i-1]) continue;
            MPI_Irecv(&recvData[i-1], sizeof(partial_results), MPI_BYTE, i, 0, MPI_COMM_WORLD, &reqRecvResults[i - 1]);
            msgRec[i-1] = false;
        }

        // test if the dispatcher has received results from all workers and update final results
        do {
            allMsgRec = true;
            for (int i = 1; i < nProcesses; i++) {
                if (finished[i-1]) continue;
                if (!msgRec[i-1]) {
                    recVal = false;
                    MPI_Test(&reqRecvResults[i - 1], (int *)&recVal, MPI_STATUS_IGNORE);
                    if (recVal) {
                        finalFileData[workerCurrentFile[i-1]].nWords += recvData[i-1].nWords;
                        finalFileData[workerCurrentFile[i-1]].nWordsWMultCons += recvData[i-1].nWordsWMultCons;
                        // printf("Dispatcher: worker %d has new results for file %s\n", i, finalFileData[workerCurrentFile[i-1]].fileName);
                        msgRec[i-1] = true;
                    } else {
                        allMsgRec = false;
                    }
                }
            }
        } while (!allMsgRec);
    }
}

/**
 * \brief Worker lifecycle:
 * - Ask for work
 * - If there is work, receive chunk from the dispatcher
 * - Process chunk
 * - Send partial results back to the dispatcher
 * 
 * \param rank worker rank
 */
void workerRoutine(int rank) {
    int chunkSize;
    char *chunk;

    partial_results partialResults;
    int ptr;
    char *currentChar = (char *) malloc(MAX_CHAR_LENGTH * sizeof(char));

    int consOcc[26];
    bool detMultCons, inWord;

    while (true) {
        // ask for work
        MPI_Send(&rank, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);

        // receive chunk size (if 0, finish)
        MPI_Recv(&chunkSize, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        if (chunkSize == 0) {
            break;
        }

        chunk = (char *) malloc((chunkSize + 1) * sizeof(char));
        MPI_Recv(chunk, chunkSize, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        partialResults.nWords = 0;
        partialResults.nWordsWMultCons = 0;
        inWord = false;

        ptr = 0;
        detMultCons = false;
        memset(consOcc, 0, 26 * sizeof(int));

        chunk[chunkSize] = '\0';

        while (extractCharFromChunk(chunk, currentChar, &ptr) != -1) {
            processChar(currentChar, &inWord, &partialResults.nWords, &partialResults.nWordsWMultCons, consOcc, &detMultCons);
        }

        // send back partial results
        MPI_Send(&partialResults, sizeof(partialResults), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
    }
}

/** \brief Prints the final results of each file.
 *
 *  \param finalFileData array with final results of each file
 *  \param _nFiles number of files
 */
void printResults(final_file_results *finalFileData, int _nFiles) {
    for (int i = 0; i < _nFiles; i++) {
        printf("File name: %s\n", finalFileData[i].fileName);
        printf("Total number of words: %d\n", finalFileData[i].nWords);
        printf("Total number of words with at least two instances of the same consonant: %d\n\n", finalFileData[i].nWordsWMultCons);
    }
}

int main(int argc, char *argv[]) {
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 2) {
        fprintf(stderr, "Error: This program requires at least 2 processes\n");
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    // DISPATCHER
    if (rank == 0) {
        char **fileNames;
        char *cmd_name = argv[0];
        int nFiles;

        // process command line options
        int opt;
        do {
            opt = getopt(argc, argv, "h");
            switch (opt) {
                case 'h':
                    printf("Usage: mpiexec MPI_REQUIRED %s REQUIRED OPTIONAL\n"
                            "MPI_REQUIRED:\n"
                            "-n number_of_processes    : number of processes (minimum is 2)\n"
                            "REQUIRED:\n"
                            "file1_path ... fileN_path : list of files to be processed\n"
                            "OPTIONAL:\n"
                            "-h                        : shows how to use the program\n", cmd_name);
                    MPI_Abort(MPI_COMM_WORLD, EXIT_SUCCESS);
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
                    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }
        } while (opt != -1);

        printf("1 dispatcher and %d workers\n", size - 1);

        final_file_results *finalFileData = (final_file_results *)malloc((nFiles + 1) * sizeof(final_file_results));
        for (int i = 0; i < nFiles; i++) {
            finalFileData[i].fileName = fileNames[i];
            finalFileData[i].nWords = 0;
            finalFileData[i].nWordsWMultCons = 0;
            finalFileData[i].fp = NULL;
        }
        initializeCharMeaning(); // to start using wordUtils

        get_delta_time();
        distributeChunks(finalFileData, size, nFiles);
        printf("Elapsed time: %f\n", get_delta_time());
        printResults(finalFileData, nFiles);
    }
    // WORKER
    else {
        initializeCharMeaning(); // to start using wordUtils
        workerRoutine(rank);
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}
