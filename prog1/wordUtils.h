/**
 *  \file wordUtils.h (inteface file)
 *
 *  \brief Assignment 1.2: multithreaded bitonic sort.
 *
 *  This file defines several functions (and the associated macros) to process UTF-8 characters from files or chunks of text.
 * 
 *  \author João Fonseca - March 2024
 *  \author Rafael Gonçalves - March 2024
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef WORD_UTILS_H
#define WORD_UTILS_H

// For Portuguese words, we don't have to consider some special characters
#define START_CHARS "0123456789abcdefghijklmnopqrstuvwxyzàáâãäåæçèéêëìíîïðñòóôõöøùúûüýÿ_"
#define SINGLE_BYTE_DELIMITERS " \t\n\r-\"[]().,:;?!–"
#define MAX_CHAR_LENGTH 5 // max number of bytes of a UTF-8 character + null terminator
#define CONSONANTS "bcdfghjklmnpqrstvwxyz"
#define MAX_CHUNK_SIZE 4096


/** \brief Array that stores the meaning of each single-byte character (1. start of the word, 2. single-byte delimiter) */
extern int charMeaning[256];

/**
 * \brief Returns the number of bytes of a UTF-8 character given its first byte.
 * 
 * \param firstByte The first byte of the UTF-8 character.
 */
extern int lengthCharUtf8(char firstByte);

/**
 * \brief Converts a UTF-8 character to lowercase and removes accents.
 * 
 * \param charUtf8 The UTF-8 character to be normalized.
 */
extern void normalizeCharUtf8(char *charUtf8);

/**
 * \brief Initializes the charMeaning array.
 */
extern void initializeCharMeaning();

/**
 * \brief Checks if a character is the start of a word.
 * 
 * \param charUtf8 The UTF-8 character to be checked.
 * 
 * \return 1 if the character is the start of a word, 0 otherwise.
 */
extern int isCharStartOfWordUtf8(const char *charUtf8);

/**
 * \brief Checks if a character is not allowed in a word.
 * 
 * \param charUtf8 The UTF-8 character to be checked.
 * 
 * \return 1 if the character is not allowed in a word, 0 otherwise.
 */
extern int isCharNotAllowedInWordUtf8(const char *charUtf8);

/**
 * \brief Extracts a UTF-8 character from a file. If the pointer is in the middle of a multi-byte character, it is moved to the beginning of the character.
 * 
 * \param textFile Pointer to the file from which a character will be extracted.
 * \param UTF8Char Where the extracted character will be stored.
 * \param charSize Where the number of bytes of the extracted character will be stored.
 * \param removePos How many bytes should the pointer be moved back to be at the beginning of the character (0 if single-byte character).
 * 
 * \return The extracted character.
 */
extern char extractCharFromFile(FILE *textFile, char *UTF8Char, uint8_t *charSize, uint8_t *removePos);

/**
 * \brief Extracts a UTF-8 character from a chunk of text.
 * 
 * \param chunk Array of characters (chunk).
 * \param UTF8Char Where the extracted character will be stored.
 * \param ptr Pointer to the position in the chunk.
 * 
 * \return The extracted character.
 */
extern char extractCharFromChunk(char *chunk, char *UTF8Char, int *ptr);

/**
 * \brief Processes a character to determine if it is part of a word.
 * 
 * \param currentChar (Pointer) The character being processed.
 * \param inWord (Pointer) Whether the program is currently processing a word.
 * \param nWords (Pointer) Number of words found.
 * \param nWordsWMultCons (Pointer) Number of words with equal consonants found.
 * \param consOcc Array that stores the number of occurrences of each consonant in the words.
 * \param detMultCons (Pointer) Indicates if the current word has equal consonants.
 */
extern void processChar(char *currentChar, bool *inWord, int *nWords, int *nWordsWMultCons, int consOcc[], bool *detMultCons);

typedef struct {
    char *chunk;
    int chunkSize;
    bool finished;
} chunk_data;

extern void retrieveData(FILE *fp, chunk_data *chunkData);

#endif