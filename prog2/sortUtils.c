/**
 *  \file sortUtils.c (implementation file)
 *
 *  \brief Assignment 2.2: mpi-based bitonic sort.
 *
 *  This file contains the implementation of the bitonic sort and merge routines.
 *
 *  \author João Fonseca
 *  \author Rafael Gonçalves
 */

#include "const.h"

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