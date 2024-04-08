/**
 *  \file shared.h (interface file) 
 *
 *  \brief Assignment 1.2: multithreaded bitonic sort.
 *
 *  This file contains the definition of the shared area with the final results and a worker internal data structure to store the partial results.
 *  Furthermore, it defines the operations to allocate the shared data, retrieve a chunk, save the partial results, and print the final ones.
 *
 *  \author João Fonseca - March 2024
 *  \author Rafael Gonçalves - March 2024
 */
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>

#define MAX_CHUNK_SIZE 4096

/** \brief Structure that represents the final results of each file */
struct SharedFileData {
    char *fileName;
    int nWords;
    int nWordsWMultCons;
    FILE *fp;
};

/** \brief Structure that represents the monitor to control the access to the shared data */
struct ChunkData {
    int fileIndex;
    bool finished;
    int nWords;
    int chunkSize;
    int nWordsWMultCons;
    bool inWord;
    char *chunk;
};

/** \brief Structure that represents the monitor to control the access to the shared data */
struct Monitor {
    int currentFile;
    int nFiles;
    struct SharedFileData *filesResults;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

/** \brief Allocates and initializes both the shared data and the monitor.
 *
 *  \param _nFiles number of files
 *  \param fileNames array with the names of the files
 */
extern void initSharedData(int _nFiles, char **fileNames);

/** \brief Retrieves a chunk of data from the current file, guaranteeing mutual exclusion.
 *
 *  \param workerId worker id
 *  \param chunkData pointer to the chunk data structure
 */
extern void retrieveData(uint8_t workerId, struct ChunkData *chunkData);

/** \brief Saves the partial results of a chunk in the shared data, guaranteeing mutual exclusion.
 *
 *  \param chunkData pointer to the chunk data structure
 */
extern void saveResults(struct ChunkData *chunkData);

/** \brief Prints the final results of each file.
 *
 *  \param _nFiles number of files
 */
extern void printResults(int _nFiles);