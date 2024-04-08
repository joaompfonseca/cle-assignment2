/**
 *  \file const.h (interface file)
 *
 *  \brief Assignment 1.2: multithreaded bitonic sort.
 *
 *  Default problem parameters.
 *
 *  \author João Fonseca - March 2024
 *  \author Rafael Gonçalves - March 2024
 */

#ifndef CONST_H
#define CONST_H

/** \brief Default number of worker threads */
#define N_WORKERS  2

/** \brief Descending sort direction */
#define DESCENDING 0

/** \brief Ascending sort direction */
#define ASCENDING 1

/** \brief Represents a sort task */
#define SORT_TASK 0

/** \brief Represents a merge task */
#define MERGE_TASK 1

/** \brief Represents a termination task */
#define TERMINATION_TASK 2

#endif /* CONST_H */
