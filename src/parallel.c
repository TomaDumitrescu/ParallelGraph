/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

#define NUM_THREADS 4

static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;

/* Define graph synchronization mechanisms. */
int working_tasks;
pthread_mutex_t mutex;
pthread_cond_t done_processing;

/* Define graph task argument. */
struct graph_arg_t {
    int index;
};
typedef struct graph_arg_t graph_arg_t;

static void graph_task_function(void *arg);

static void process_node(unsigned int index)
{
    /* Implement thread-pool based processing of graph. */

    /* Verify if the node is in a running task */
    pthread_mutex_lock(&mutex);
    if (graph->visited[index] != PROCESSING) {
        pthread_mutex_unlock(&mutex);
        return;
    }
    pthread_mutex_unlock(&mutex);

    os_node_t *node = graph->nodes[index];

    pthread_mutex_lock(&mutex);

    sum += node->info;
    graph->visited[index] = DONE;

    working_tasks--;
    if (working_tasks == 0)
        pthread_cond_signal(&done_processing);

    pthread_mutex_unlock(&mutex);

    /* Visit every non-visited neighbours */
    for (int i = 0; i < (int)node->num_neighbours; i++) {
        int neighbour = node->neighbours[i];

        pthread_mutex_lock(&mutex);
        if (graph->visited[neighbour] == NOT_VISITED) {
            /* Move the neighbour from the ready state in a running task */
            graph->visited[neighbour] = PROCESSING;
            working_tasks++;
            pthread_mutex_unlock(&mutex);

            graph_arg_t *arg = malloc(sizeof(*arg));
            DIE(arg == NULL, "malloc");
            arg->index = neighbour;
            enqueue_task(tp, create_task(graph_task_function, arg, free));
        } else {
            pthread_mutex_unlock(&mutex);
        }
    }
}

static void graph_task_function(void *arg)
{
    process_node(((graph_arg_t *) arg)->index);
}

int main(int argc, char *argv[])
{
    FILE *input_file;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s input_file\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    input_file = fopen(argv[1], "r");
    DIE(input_file == NULL, "fopen");

    graph = create_graph_from_file(input_file);
    fclose(input_file);

    /* Initialize graph synchronization mechanisms. */
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&done_processing, NULL);

    /* Create the threadpool */
    tp = create_threadpool(NUM_THREADS);
    working_tasks = 1;

    /* Initial task starting from node 0 */
    pthread_mutex_lock(&mutex);

    graph->visited[0] = PROCESSING;

    pthread_mutex_unlock(&mutex);

    graph_arg_t *arg = malloc(sizeof(*arg));
    DIE(arg == NULL, "malloc");
    arg->index = 0;
    enqueue_task(tp, create_task(graph_task_function, arg, free));

    /* Signal done_processing when working_tasks is 0 */
    pthread_mutex_lock(&mutex);

    while (working_tasks != 0)
        pthread_cond_wait(&done_processing, &mutex);

    pthread_mutex_unlock(&mutex);

    pthread_mutex_lock(&tp->mutex);

    tp->work_done = 1;
    pthread_cond_broadcast(&tp->condition);

    pthread_mutex_unlock(&tp->mutex);

    wait_for_completion(tp);
    destroy_threadpool(tp);

    /* Print the final sum without a trailing newline using write() */
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d", sum);
    if (len < 0) {
        perror("snprintf");
        exit(EXIT_FAILURE);
    }
    if (write(STDOUT_FILENO, buf, len) < 0) {
        perror("write");
        exit(EXIT_FAILURE);
    }

    return 0;
}
