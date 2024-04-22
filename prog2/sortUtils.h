/**
 *  \file sortUtils.h (interface file)
 *
 *  \brief Assignment 2.2: mpi-based bitonic sort.
 *
 *  This file contains the interface of the bitonic sort and merge routines.
 *
 *  \author João Fonseca - March 2024
 *  \author Rafael Gonçalves - March 2024
 */

#ifndef SORT_UTILS_H
#define SORT_UTILS_H

/**
 *  \brief Merges two halves of an integer array in the desired order.
 *
 *  \param arr array to be merged
 *  \param low_index index of the first element of the array
 *  \param count number of elements in the array
 *  \param direction 0 for descending order, 1 for ascending order
 */
extern void bitonic_merge(int *arr, int low_index, int count, int direction);

/**
 *  \brief Sorts an integer array in the desired order.
 *
 *  \param arr array to be sorted
 *  \param low_index index of the first element of the array
 *  \param count number of elements in the array
 *  \param direction 0 for descending order, 1 for ascending order
 */
extern void bitonic_sort(int *arr, int low_index, int count, int direction);

#endif /* SORT_UTILS_H */
