/**
 *  \file shared.c (implementation file)
 *
 *  \brief Assignment 1.2: multithreaded bitonic sort.
 *
 *  This file contains the definition of the shared area and the operations it supports.
 *
 *  The shared area contains the configuration of the program and a the mechanism to assign tasks to each worker thread.
 *
 *  A distributor thread assigns tasks to each worker thread and waits for them to finish doing them. It is also
 *  responsible for controlling the number of tasks that need to be executed before assigning new ones.
 *
 *  A worker thread can perform 3 types of tasks:
 *  - sort (bitonic sort)
 *  - merge (bitonic merge of two sorted arrays)
 *  - termination (terminates the worker thread)
 *
 *  Main thread operations:
 *  - init_shared: initializes the shared area
 *
 *  Distributor thread operations:
 *  - set_tasks: assigns tasks to each worker thread
 *
 *  Worker thread operations:
 *  - get_task: gets a task to execute
 *  - task_done: decrements the number of tasks to be executed
 *
 *  \author João Fonseca - March 2024
 *  \author Rafael Gonçalves - March 2024
 */

#include <pthread.h>

/** \brief Structure that represents a task to be executed by a worker thread */
typedef struct {
    int type;
    int *arr;
    int low_index;
    int count;
    int direction;
} task_t;

/** \brief Structure that represents the configuration of the program */
typedef struct {
    char *file_path;
    int *arr;
    int size;
    int direction;
    int n_workers;
} config_t;

/** \brief Structure that represents the mechanism to assign tasks to each worker thread */
typedef struct {
    task_t *list;
    int size;
    int *is_thread_done;
    int done;
    pthread_cond_t tasks_ready;
    pthread_cond_t tasks_done;
} tasks_t;

/** \brief Structure that represents the shared area */
typedef struct {
    pthread_mutex_t mutex;
    config_t config;
    tasks_t tasks;
} shared_t;

/**
 * \brief Initializes the configuration of the program.
 *
 * Should be called by the main thread before creating the distributor and worker threads.
 *
 * \param config pointer to the configuration of the program
 * \param file_path path to the file with the array to be sorted
 * \param direction direction of the bitonic sort
 * \param n_workers number of worker threads
 */
void init_config(config_t *config, char *file_path, int direction, int n_workers) {
    config->file_path = file_path;
    config->direction = direction;
    config->n_workers = n_workers;
}

/**
 * \brief Initializes the array to be sorted.
 *
 * Should be called by the distributor thread before assigning tasks to the worker threads.
 *
 * \param config pointer to the configuration of the program
 * \param arr pointer to the array to be sorted
 * \param size size of the array to be sorted
 */
void init_arr(config_t *config, int *arr, int size) {
    config->arr = arr;
    config->size = size;
}

/**
 * \brief Initializes the tasks mechanism.
 *
 * Should be called by the main thread before creating the distributor and worker threads.
 *
 * \param tasks pointer to the mechanism to assign tasks to each worker thread
 * \param list pointer to the list of tasks
 * \param size size of the list of tasks
 * \param is_thread_done pointer to the array that indicates if a worker thread is done with its task
 */
void init_tasks(tasks_t *tasks, task_t *list, int size, int *is_thread_done) {
    tasks->list = list;
    tasks->size = size;
    tasks->is_thread_done = is_thread_done;
    tasks->done = 0;
    pthread_cond_init(&tasks->tasks_ready, NULL);
    pthread_cond_init(&tasks->tasks_done, NULL);
}

/**
 * \brief Initializes the shared area.
 *
 * Should be called by the main thread before creating the distributor and worker threads.
 * Called after init_config and init_tasks.
 *
 * \param shared pointer to the shared area
 * \param config pointer to the configuration of the program
 * \param tasks pointer to the mechanism to assign tasks to each worker thread
 */
void init_shared(shared_t *shared, config_t *config, tasks_t *tasks) {
    pthread_mutex_init(&shared->mutex, NULL);
    shared->config = *config;
    shared->tasks = *tasks;
}

/**
 * \brief Assigns tasks to each worker thread.
 *
 * Should be called by the distributor thread to assign tasks to each worker thread.
 * Blocks until all worker threads are done with their tasks.
 *
 * \param shared pointer to the shared area
 * \param list pointer to the list of tasks
 * \param size size of the list of tasks
 */
void set_tasks(shared_t *shared, task_t *list, int size) {
    pthread_mutex_lock(&shared->mutex);
    while (shared->tasks.done < shared->tasks.size) {
        pthread_cond_wait(&shared->tasks.tasks_done, &shared->mutex);
    }
    for (int i = 0; i < size; i++) {
        shared->tasks.list[i] = list[i];
        shared->tasks.is_thread_done[i] = 0;
    }
    shared->tasks.size = size;
    shared->tasks.done = 0;
    pthread_cond_broadcast(&shared->tasks.tasks_ready);
    pthread_mutex_unlock(&shared->mutex);
}

/**
 * \brief Gets a task to execute.
 *
 * Should be called by a worker thread to get a task to execute.
 * Blocks until there is a new task assigned to the worker thread.
 *
 * \param shared pointer to the shared area
 * \param index index of the worker thread
 * \return task to execute
 */
task_t get_task(shared_t *shared, int index) {
    task_t task;
    pthread_mutex_lock(&shared->mutex);
    while (shared->tasks.is_thread_done[index] == 1 || shared->tasks.size == 0) {
        pthread_cond_wait(&shared->tasks.tasks_ready, &shared->mutex);
    }
    task = shared->tasks.list[index];
    pthread_mutex_unlock(&shared->mutex);
    return task;
}

/**
 * \brief Decrements the number of tasks to be executed.
 *
 * Should be called by a worker thread after finishing its task.
 * Decrements the number of tasks to be executed and signals the distributor thread if all tasks are done.
 *
 * \param shared pointer to the shared area
 * \param index index of the worker thread
 */
void task_done(shared_t *shared, int index) {
    pthread_mutex_lock(&shared->mutex);
    shared->tasks.is_thread_done[index] = 1;
    shared->tasks.done++;
    if (shared->tasks.done == shared->tasks.size) {
        pthread_cond_signal(&shared->tasks.tasks_done);
    }
    pthread_mutex_unlock(&shared->mutex);
}
