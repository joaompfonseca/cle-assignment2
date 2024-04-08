/**
 *  \file shared.c (implementation file)
 *
 *  \brief Assignment 1.2: multithreaded bitonic sort.
 *
 *  This file contains the declaration of the shared area with the final results and a worker internal data structure to store the partial results.
 *  Furthermore, it contains the operations to allocate the shared data, retrieve a chunk, save the partial results, and print the final ones.
 *
 *  \author João Fonseca - March 2024
 *  \author Rafael Gonçalves - March 2024
 */
#include "shared.h"
#include "wordUtils.h"

/** \brief Structure that represents the final results of each file */
struct SharedFileData *sharedFileData;

/** \brief Structure that represents the monitor to control the access to the shared data */
struct Monitor monitor;

/** \brief Allocates and initializes both the shared data and the monitor.
 *
 *  \param _nFiles number of files
 *  \param fileNames array with the names of the files
 */
void initSharedData(int _nFiles, char **fileNames) {
    sharedFileData = (struct SharedFileData *)malloc((_nFiles + 1) * sizeof(struct SharedFileData));
    for (int i = 0; i < _nFiles; i++) {
        sharedFileData[i].fileName = fileNames[i];
        sharedFileData[i].nWords = 0;
        sharedFileData[i].nWordsWMultCons = 0;
        sharedFileData[i].fp = NULL;
    }

    monitor = (struct Monitor){
        0, // currentFile
        _nFiles, // nFiles
        sharedFileData, // filesResults
        PTHREAD_MUTEX_INITIALIZER, // mutex
        PTHREAD_COND_INITIALIZER // cond
    };
}

/** \brief Retrieves a chunk of data from the current file, guaranteeing mutual exclusion.
 *
 *  \param workerId worker id
 *  \param chunkData pointer to the chunk data structure
 */
void retrieveData(uint8_t workerId, struct ChunkData *chunkData) {
    if (pthread_mutex_lock(&monitor.mutex) != 0) {
        perror("Error: could not lock mutex");
        pthread_exit(NULL);
    }

    // get current file index
    int fileIndex = monitor.currentFile;

    if (monitor.currentFile < monitor.nFiles) {
        // open file
        if (sharedFileData[fileIndex].fp == NULL) {
            if ((sharedFileData[fileIndex].fp = fopen(sharedFileData[fileIndex].fileName, "rb")) == NULL) {
                perror("Error opening file");
                pthread_exit(NULL);
            }
        }

        // read chunk
        chunkData->chunk = (char *)malloc((MAX_CHUNK_SIZE + 1) * sizeof(char)); // +1 for null terminator
        chunkData->chunkSize = fread(chunkData->chunk, 1, MAX_CHUNK_SIZE, sharedFileData[fileIndex].fp);
        chunkData->fileIndex = fileIndex;
        chunkData->finished = false;

        // if chunk size is less than MAX_CHUNK_SIZE, then it is the last chunk
        if (chunkData->chunkSize < MAX_CHUNK_SIZE) {
            fclose(sharedFileData[fileIndex].fp);
            monitor.currentFile++;
        }
        else {
            // read until a word is complete
            char *UTF8Char = (char *) malloc(MAX_CHAR_LENGTH * sizeof(char));
            uint8_t charSize;
            uint8_t removePos = 0;

            while (extractCharFromFile(sharedFileData[fileIndex].fp, UTF8Char, &charSize, &removePos) != EOF) {
                if (isCharNotAllowedInWordUtf8(UTF8Char)) {
                    chunkData->chunkSize -= removePos;
                    chunkData->chunk[chunkData->chunkSize] = '\0';
                    break;
                }

                // realloc chunk if necessary
                if (chunkData->chunkSize + charSize > MAX_CHUNK_SIZE) {
                    chunkData->chunk = (char *)realloc(chunkData->chunk, (chunkData->chunkSize + charSize + 1) * sizeof(char));
                }

                // add char to chunk
                for (int i = 0; i < charSize; i++) {
                    chunkData->chunk[chunkData->chunkSize] = UTF8Char[i];
                    chunkData->chunkSize++;
                }
            }

            chunkData->chunk[chunkData->chunkSize] = '\0';
        }
    }

    // unlock mutex
    if (pthread_mutex_unlock(&monitor.mutex) != 0) {
        perror("Error: could not unlock mutex");
        pthread_exit(NULL);
    }
}

/** \brief Saves the partial results of a chunk in the shared data, guaranteeing mutual exclusion.
 *
 *  \param chunkData pointer to the chunk data structure
 */
void saveResults(struct ChunkData *chunkData) {
    if (pthread_mutex_lock(&monitor.mutex) != 0) {
        perror("Error: could not lock mutex");
        pthread_exit(NULL);
    }

    sharedFileData[chunkData->fileIndex].nWords += chunkData->nWords;
    sharedFileData[chunkData->fileIndex].nWordsWMultCons += chunkData->nWordsWMultCons;

    if (pthread_mutex_unlock(&monitor.mutex) != 0) {
        perror("Error: could not unlock mutex");
        pthread_exit(NULL);
    }
}

/** \brief Prints the final results of each file.
 *
 *  \param _nFiles number of files
 */
void printResults(int _nFiles) {
    for (int i = 0; i < _nFiles; i++) {
        printf("File name: %s\n", sharedFileData[i].fileName);
        printf("Total number of words: %d\n", sharedFileData[i].nWords);
        printf("Total number of words with at least two instances of the same consonant: %d\n\n", sharedFileData[i].nWordsWMultCons);
    }
}