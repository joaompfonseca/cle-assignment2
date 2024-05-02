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
#include "fileDataHandler.h"
#include "wordUtils.h"

/** \brief Structure that represents the final results of each file */
final_file_results *finalFileData;

/** \brief Current file index */
int currentFile;

/** \brief Number of files being processed. */
int nFiles;

/** \brief Allocates and initializes both the shared data and the monitor.
 *
 *  \param _nFiles number of files
 *  \param fileNames array with the names of the files
 */
final_file_results* initFinalResults(int _nFiles, char **fileNames) {
    finalFileData = (final_file_results *)malloc((_nFiles + 1) * sizeof(final_file_results));
    for (int i = 0; i < _nFiles; i++) {
        finalFileData[i].fileName = fileNames[i];
        finalFileData[i].nWords = 0;
        finalFileData[i].nWordsWMultCons = 0;
        finalFileData[i].fp = NULL;
    }

    currentFile = 0;
    nFiles = _nFiles;

    return finalFileData;
}

/** \brief Retrieves a chunk of data from the current file, guaranteeing mutual exclusion.
 *
 *  \param chunkData pointer to the chunk data structure
 */
void retrieveData(chunk_data *chunkData) {

    if (currentFile < nFiles) {
        // open file
        if (finalFileData[currentFile].fp == NULL) {
            if ((finalFileData[currentFile].fp = fopen(finalFileData[currentFile].fileName, "rb")) == NULL) {
                perror("Error opening file");
                pthread_exit(NULL);
            }
        }

        // read chunk
        chunkData->chunk = (char *)malloc((MAX_CHUNK_SIZE + 1) * sizeof(char)); // +1 for null terminator
        chunkData->chunkSize = fread(chunkData->chunk, 1, MAX_CHUNK_SIZE, finalFileData[currentFile].fp);
        chunkData->fileIndex = currentFile;
        chunkData->finished = false;

        // if chunk size is less than MAX_CHUNK_SIZE, then it is the last chunk
        if (chunkData->chunkSize < MAX_CHUNK_SIZE) {
            fclose(finalFileData[currentFile].fp);
            currentFile++;
        }
        else {
            // read until a word is complete
            char *UTF8Char = (char *) malloc(MAX_CHAR_LENGTH * sizeof(char));
            uint8_t charSize;
            uint8_t removePos = 0;

            while (extractCharFromFile(finalFileData[currentFile].fp, UTF8Char, &charSize, &removePos) != EOF) {
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

}

/** \brief Saves the partial results of a chunk in the shared data, guaranteeing mutual exclusion.
 *
 *  \param chunkData pointer to the chunk data structure
 */
void saveResults(int nWords, int nWordsWMultCons, int fileIndex) {
    finalFileData[fileIndex].nWords += nWords;
    finalFileData[fileIndex].nWordsWMultCons += nWordsWMultCons;
}

/** \brief Prints the final results of each file.
 *
 *  \param _nFiles number of files
 */
