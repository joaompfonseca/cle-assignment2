/**
 *  \file multiBitonic.c (implementation file)
 *
 *  \brief Assignment 1.2: multithreaded bitonic sort.
 *
 *  This file contains the definition of the multithreaded bitonic sort algorithm.
 *
 *  \author João Fonseca - March 2024
 *  \author Rafael Gonçalves - March 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "const.h"
#include "shared.c"

/**
 *  \brief Prints the usage of the program.
 *
 *  \param cmd_name name of the command that started the program
 */
void printUsage(char *cmd_name) {
    fprintf(stderr, "Usage: %s REQUIRED OPTIONS\n"
                    "REQUIRED:\n"
                    "-f --- input file with numbers\n"
                    "OPTIONS:\n"
                    "-h --- print this help\n"
                    "-n --- number of worker threads (default is %d, minimum is 1)\n", cmd_name, N_WORKERS);
}

/**
 *  \brief Gets the time elapsed since the last call to this function.
 *
 *  \return time elapsed in seconds
 */
static double get_delta_time(void) {
    static struct timespec t0, t1;

    // t0 is assigned the value of t1 from the previous call. if there is no previous call, t0 = t1 = 0
    t0 = t1;

    if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
        fprintf(stderr, "[TIME] Could not get the time\n");
        exit(EXIT_FAILURE);
    }
    return (double) (t1.tv_sec - t0.tv_sec) + 1.0e-9 * (double) (t1.tv_nsec - t0.tv_nsec);
}

/**
 * \brief Free memory from all pointers in the list.
 *
 * \param pointers list of pointers to be freed
 * \param size number of pointers in the list
 */
void free_all(void **pointers, int size) {
    for (int i = 0; i < size; i++) {
        free(pointers[i]);
    }
}

/**
 *  \brief Merges two halves of an integer array in the desired order.
 *
 *  \param arr array to be merged
 *  \param low_index index of the first element of the array
 *  \param count number of elements in the array
 *  \param direction 0 for descending order, 1 for ascending order
 */
void bitonic_merge(int *arr, int low_index, int count, int direction) { // NOLINT(*-no-recursion)
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
void bitonic_sort(int *arr, int low_index, int count, int direction) { // NOLINT(*-no-recursion)
    if (count <= 1) return;
    int half = count / 2;
    // sort left half in ascending order
    bitonic_sort(arr, low_index, half, ASCENDING);
    // sort right half in descending order
    bitonic_sort(arr, low_index + half, half, DESCENDING);
    // merge the two halves
    bitonic_merge(arr, low_index, count, direction);
}

/**
 *  \brief Argument structure for the worker threads.
 */
typedef struct {
    int index;
    shared_t *shared;
} bitonic_worker_arg_t;

/**
 *  \brief Worker thread function that executes tasks.
 *
 *  Lifecycle loop:
 *  - get a task from the shared area
 *  - if the task is a sort task, sort the array
 *  - if the task is a merge task, merge the array
 *  - if the task is a termination task, finish the thread
 *
 *  \param arg pointer to the argument structure, that contains the index of the worker thread and the shared area
 */
static void *bitonic_worker(void *arg) {
    bitonic_worker_arg_t *worker_arg = (bitonic_worker_arg_t *) arg;
    int index = worker_arg->index;
    shared_t *shared = worker_arg->shared;

    while (1) {
        task_t task = get_task(shared, index);
        if (task.type == SORT_TASK) {
            bitonic_sort(task.arr, task.low_index, task.count, task.direction);
            task_done(shared, index);
        } else if (task.type == MERGE_TASK) {
            bitonic_merge(task.arr, task.low_index, task.count, task.direction);
            task_done(shared, index);
        } else {
            // termination task
            task_done(shared, index);
            break;
        }
    }
    return (void *) EXIT_SUCCESS;
}

/**
 *  \brief Distributor thread function that assigns tasks to worker threads.
 *
 *  Lifecycle:
 *  - read the array from the file
 *  - divide the array into n_workers parts and assign a sort task to each worker thread
 *  - perform a bitonic merge of the sorted parts and assign a merge task to each worker thread
 *  - terminate worker threads that are not needed anymore
 *
 *  \param arg pointer to the shared area
 */
static void *bitonic_distributor(void *arg) {
    shared_t *shared = (shared_t *) arg;
    char *file_path = shared->config.file_path;
    int direction = shared->config.direction;
    int n_workers = shared->config.n_workers;

    // open the file
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        fprintf(stderr, "[DIST] Could not open file %s\n", file_path);
        return (void *) EXIT_FAILURE;
    }
    // read the size of the array
    int size;
    if (fread(&size, sizeof(int), 1, file) != 1) {
        fprintf(stderr, "[DIST] Could not read the size of the array\n");
        fclose(file);
        return (void *) EXIT_FAILURE;
    }
    // size must be power of 2
    if ((size & (size - 1)) != 0) {
        fprintf(stderr, "[DIST] The size of the array must be a power of 2\n");
        fclose(file);
        return (void *) EXIT_FAILURE;
    }
    fprintf(stdout, "[DIST] Array size: %d\n", size);
    // allocate memory for the array
    int *arr = (int *) malloc(size * sizeof(int));
    if (arr == NULL) {
        fprintf(stderr, "[DIST] Could not allocate memory for the array\n");
        fclose(file);
        return (void *) EXIT_FAILURE;
    }
    // load array into memory
    int num, ni = 0;
    while (fread(&num, sizeof(int), 1, file) == 1 && ni < size) {
        arr[ni++] = num;
    }
    // close the file
    fclose(file);

    // initialize the array to be sorted
    init_arr(&shared->config, arr, size);

    // allocate memory for the list of tasks
    task_t *list = (task_t *) malloc(n_workers * sizeof(task_t));
    if (list == NULL) {
        fprintf(stderr, "[DIST] Could not allocate memory for the list of tasks\n");
        free_all((void **) arr, 1);
        return (void *) EXIT_FAILURE;
    }

    // START TIME
    get_delta_time();

    if (size > 1) {
        // divide the array into n_workers parts
        // make each worker thread bitonic sort one part
        int count = size / n_workers;
        for (int i = 0; i < n_workers; i++) {
            int low_index = i * count;
            // direction of the sub-sort
            int sub_direction = (((low_index / count) % 2 == 0) != 0) == direction;
            task_t task = {SORT_TASK, arr, low_index, count, sub_direction};
            list[i] = task;
        }
        set_tasks(shared, list, n_workers);
        fprintf(stdout, "[DIST] Bitonic sort of %d parts of size %d\n", n_workers, count);

        // perform a bitonic merge of the sorted parts
        // make each worker thread bitonic merge one part, discard unused threads
        for (count *= 2; count <= size; count *= 2) {
            int n_tasks = size / count;
            // merge tasks
            for (int i = 0; i < n_tasks; i++) {
                int low_index = i * count;
                // direction of the sub-merge
                int sub_direction = (((low_index / count) % 2 == 0) != 0) == direction;
                task_t task = {MERGE_TASK, arr, low_index, count, sub_direction};
                list[i] = task;
            }
            // termination tasks
            for (int i = n_tasks; i < n_workers; i++) {
                task_t task = {TERMINATION_TASK};
                list[i] = task;
            }
            set_tasks(shared, list, n_workers);
            fprintf(stdout, "[DIST] Bitonic merge of %d parts of size %d\n", n_tasks, count);
            // update the number of workers
            n_workers = n_tasks;
        }

        // termination task to last worker thread
        task_t task = {TERMINATION_TASK};
        list[0] = task;
        set_tasks(shared, list, 1);
    }

    // END TIME
    fprintf(stdout, "[TIME] Time elapsed: %.9f seconds\n", get_delta_time());

    return (void *) EXIT_SUCCESS;
}

/**
 *  \brief Main function of the program.
 *
 *  Lifecycle:
 *  - process command line options
 *  - allocate memory for the shared area, configuration, tasks, list of tasks and list of threads done
 *  - initialize the configuration, tasks and shared area
 *  - create distributor thread
 *  - create worker threads
 *  - wait for threads to finish
 *  - check if the array is sorted
 *
 *  \param argc number of command line arguments
 *  \param argv array of command line arguments
 *
 *  \return EXIT_SUCCESS if the array is sorted, EXIT_FAILURE otherwise
 */
int main(int argc, char *argv[]) {
    // program arguments
    char *cmd_name = argv[0];
    char *file_path = NULL;
    int n_workers = N_WORKERS;

    // process command line options
    int opt;
    do {
        switch ((opt = getopt(argc, argv, "f:n:"))) {
            case 'f':
                file_path = optarg;
                if (file_path == NULL) {
                    fprintf(stderr, "[MAIN] Invalid file name\n");
                    printUsage(cmd_name);
                    return EXIT_FAILURE;
                }
                break;
            case 'n':
                n_workers = atoi(optarg);
                if (n_workers < 1) {
                    fprintf(stderr, "[MAIN] Invalid number of worker threads\n");
                    printUsage(cmd_name);
                    return EXIT_FAILURE;
                }
                break;
            case 'h':
                printUsage(cmd_name);
                return EXIT_SUCCESS;
            case '?':
                fprintf(stderr, "[MAIN] Invalid option -%c\n", optopt);
                printUsage(cmd_name);
                return EXIT_FAILURE;
            case -1:
                break;
        }
    } while (opt != -1);
    if (file_path == NULL) {
        fprintf(stderr, "[MAIN] Input file not specified\n");
        printUsage(cmd_name);
        return EXIT_FAILURE;
    }
    fprintf(stdout, "[MAIN] Input file: %s\n", file_path);
    fprintf(stdout, "[MAIN] Worker threads: %d\n", n_workers);

    // allocate memory for the configuration
    config_t *config = (config_t *) malloc(sizeof(config_t));
    if (config == NULL) {
        fprintf(stderr, "[MAIN] Could not allocate memory for the configuration\n");
        return EXIT_FAILURE;
    }
    // allocate memory for the tasks
    tasks_t *tasks = (tasks_t *) malloc(sizeof(tasks_t));
    if (tasks == NULL) {
        fprintf(stderr, "[MAIN] Could not allocate memory for the list of tasks\n");
        free_all((void *[]) {config}, 1);
        return EXIT_FAILURE;
    }
    // allocate memory for the list of tasks
    task_t *list = (task_t *) malloc(n_workers * sizeof(task_t));
    if (list == NULL) {
        fprintf(stderr, "[MAIN] Could not allocate memory for the list of tasks\n");;
        free_all((void *[]) {config, tasks}, 2);
        return EXIT_FAILURE;
    }
    // allocate memory for the list of threads done
    int *is_thread_done = (int *) malloc(n_workers * sizeof(int));
    if (is_thread_done == NULL) {
        fprintf(stderr, "[MAIN] Could not allocate memory for the list of threads done\n");
        free_all((void *[]) {config, tasks, list}, 3);
        return EXIT_FAILURE;
    }
    // allocate memory for the shared area
    shared_t *shared = (shared_t *) malloc(sizeof(shared_t));
    if (shared == NULL) {
        fprintf(stderr, "[MAIN] Could not allocate memory for the shared area\n");
        free_all((void *[]) {config, tasks, list, is_thread_done}, 4);
        return EXIT_FAILURE;
    }
    // initialize the configuration, tasks and shared area
    init_config(config, file_path, DESCENDING, n_workers);
    init_tasks(tasks, list, n_workers, is_thread_done);
    init_shared(shared, config, tasks);

    // create distributor thread
    pthread_t *distributor = (pthread_t *) malloc(sizeof(pthread_t));
    if (distributor == NULL) {
        fprintf(stderr, "[MAIN] Could not allocate memory for the distributor thread\n");
        free_all((void *[]) {config, tasks, list, is_thread_done, shared}, 5);
        return EXIT_FAILURE;
    }
    if (pthread_create(distributor, NULL, bitonic_distributor, shared) != 0) {
        fprintf(stderr, "[MAIN] Could not create distributor thread\n");
        free_all((void *[]) {config, tasks, list, is_thread_done, shared, distributor}, 6);
        return EXIT_FAILURE;
    } else {
        fprintf(stdout, "[MAIN] Distributor thread has been created\n");
    }
    // create worker threads
    pthread_t *workers = (pthread_t *) malloc(n_workers * sizeof(pthread_t));
    if (workers == NULL) {
        fprintf(stderr, "[MAIN] Could not allocate memory for the worker threads\n");
        free_all((void *[]) {config, tasks, list, is_thread_done, shared, distributor}, 6);
        return EXIT_FAILURE;
    }
    bitonic_worker_arg_t *workers_arg = (bitonic_worker_arg_t *) malloc(n_workers * sizeof(bitonic_worker_arg_t));
    if (workers_arg == NULL) {
        fprintf(stderr, "[MAIN] Could not allocate memory for the worker threads arguments\n");
        free_all((void *[]) {config, tasks, list, is_thread_done, shared, distributor, workers}, 7);
        return EXIT_FAILURE;
    }
    for (int i = 0; i < n_workers; i++) {
        workers_arg[i] = (bitonic_worker_arg_t) {i, shared};
        if (pthread_create(&workers[i], NULL, bitonic_worker, &workers_arg[i]) != 0) {
            fprintf(stderr, "[MAIN] Could not create worker thread %d\n", i + 1);
            free_all((void *[]) {config, tasks, list, is_thread_done, shared, distributor, workers, workers_arg}, 8);
            return EXIT_FAILURE;
        } else {
            fprintf(stdout, "[MAIN] Worker threads have been created (%d/%d)\n", i + 1, n_workers);
        }
    }

    // wait for threads to finish
    void *ptr_retcode_void;
    int *ptr_retcode_int;
    pthread_join(*distributor, &ptr_retcode_void);
    ptr_retcode_int = (int *) ptr_retcode_void;
    if (ptr_retcode_int != EXIT_SUCCESS) {
        fprintf(stderr, "[MAIN] Distributor thread has failed with return code %d\n", *ptr_retcode_int);
        free_all((void *[]) {config, tasks, list, is_thread_done, shared, distributor, workers, workers_arg}, 8);
        return EXIT_FAILURE;
    } else {
        fprintf(stdout, "[MAIN] Distributor thread has finished\n");
    }
    for (int i = 0; i < n_workers; i++) {
        pthread_join(workers[i], &ptr_retcode_void);
        ptr_retcode_int = (int *) ptr_retcode_void;
        if (ptr_retcode_int != EXIT_SUCCESS) {
            fprintf(stderr, "[MAIN] Worker thread %d has failed with return code %d\n", i + 1,
                    *ptr_retcode_int);
            free_all((void *[]) {config, tasks, list, is_thread_done, shared, distributor, workers, workers_arg}, 8);
            return EXIT_FAILURE;
        } else {
            fprintf(stdout, "[MAIN] Worker threads have finished (%d/%d)\n", i + 1, n_workers);
        }
    }

    // check if array is sorted
    int *arr = shared->config.arr;
    int size = shared->config.size;
    for (int i = 0; i < size - 1; i++) {
        if (arr[i] < arr[i + 1]) {
            fprintf(stderr, "[MAIN] Error in position %d between element %d and %d\n",
                    i, arr[i], arr[i + 1]);
            free_all((void *[]) {config, tasks, list, is_thread_done, shared, distributor, workers, workers_arg}, 8);
            return EXIT_FAILURE;
        }
    }
    printf("[MAIN] The array is sorted, everything is OK! :)\n");

    free_all((void *[]) {config, tasks, list, is_thread_done, shared, distributor, workers, workers_arg, arr}, 9);
    return EXIT_SUCCESS;
}
