/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

/* Create a task that would be executed by a thread. */
os_task_t *create_task(void (*action)(void *), void *arg, void (*destroy_arg)(void *))
{
	os_task_t *t = malloc(sizeof(*t));
	DIE(t == NULL, "malloc");

	t->action = action;			// the function
	t->argument = arg;			// arguments for the function
	t->destroy_arg = destroy_arg;			// destroy argument function

	return t;
}

/* Destroy task. */
void destroy_task(os_task_t *t)
{
	if (t->destroy_arg != NULL)
		t->destroy_arg(t->argument);
	free(t);
}

/* Put a new task to threadpool task queue. */
void enqueue_task(os_threadpool_t *tp, os_task_t *t)
{
	assert(tp != NULL);
	assert(t != NULL);

	/* Enqueue task to the shared task queue. Use synchronization. */

	pthread_mutex_lock(&tp->mutex);

	list_add_tail(&tp->head, &t->list);
	tp->ready++;

	pthread_cond_signal(&tp->condition);

	pthread_mutex_unlock(&tp->mutex);
}

/*
* Check if queue is empty.
* This function should be called in a synchronized manner.
*/
static int queue_is_empty(os_threadpool_t *tp)
{
	return list_empty(&tp->head);
}

/*
 * Get a task from threadpool task queue.
 * Block if no task is available.
 * Return NULL if work is complete, i.e. no task will become available,
 * i.e. all threads are going to block.
 */
os_task_t *dequeue_task(os_threadpool_t *tp)
{
	/* Dequeue task from the shared task queue. Use synchronization. */

	pthread_mutex_lock(&tp->mutex);

	while (tp->ready == 0 && !tp->work_done)
		pthread_cond_wait(&tp->condition, &tp->mutex);

	if (tp->ready == 0 && tp->work_done) {
		pthread_mutex_unlock(&tp->mutex);
		return NULL;
	}

	os_list_node_t *node = tp->head.next;
	list_del(node);

	tp->running++;
	tp->ready--;

	pthread_mutex_unlock(&tp->mutex);

	return (os_task_t *)list_entry(node, os_task_t, list);
}

/* Loop function for threads */
static void *thread_loop_function(void *arg)
{
	os_threadpool_t *tp = (os_threadpool_t *) arg;

	while (1) {
		os_task_t *t = dequeue_task(tp);
		if (t == NULL)
			break;

		t->action(t->argument);
		destroy_task(t);

		pthread_mutex_lock(&tp->mutex);
		tp->running--;
		if (tp->running == 0 && tp->ready == 0)
			pthread_cond_signal(&tp->condition);

		pthread_mutex_unlock(&tp->mutex);
	}

	return NULL;
}

/* Wait completion of all threads. This is to be called by the main thread. */
void wait_for_completion(os_threadpool_t *tp)
{
	/* Wait for all worker threads. Use synchronization. */
	pthread_mutex_lock(&tp->mutex);

	while (tp->running > 0 || tp->ready > 0)
		pthread_cond_wait(&tp->condition, &tp->mutex);

	tp->work_done = true;
	pthread_cond_broadcast(&tp->condition);

	pthread_mutex_unlock(&tp->mutex);

	/* Join all worker threads. */
	for (unsigned int i = 0; i < tp->num_threads; i++)
		pthread_join(tp->threads[i], NULL);
}

/* Create a new threadpool. */
os_threadpool_t *create_threadpool(unsigned int num_threads)
{
	os_threadpool_t *tp = malloc(sizeof(*tp));
	DIE(tp == NULL, "malloc");

	list_init(&tp->head);

	/* Initialize synchronization data. */
	pthread_mutex_init(&tp->mutex, NULL);
	pthread_cond_init(&tp->condition, NULL);
	tp->ready = 0;
	tp->running = 0;
	tp->work_done = false;

	tp->num_threads = num_threads;
	tp->threads = malloc(num_threads * sizeof(*tp->threads));
	DIE(tp->threads == NULL, "malloc");

	for (unsigned int i = 0; i < num_threads; ++i) {
		int rc = pthread_create(&tp->threads[i], NULL, &thread_loop_function, (void *) tp);
		DIE(rc < 0, "pthread_create");
	}

	return tp;
}

/* Destroy a threadpool. Assume all threads have been joined. */
void destroy_threadpool(os_threadpool_t *tp)
{
	os_list_node_t *n, *p;
	list_for_each_safe(n, p, &tp->head) {
		list_del(n);
		destroy_task(list_entry(n, os_task_t, list));
	}

	/* Cleanup synchronization data. */
	pthread_mutex_destroy(&tp->mutex);
	pthread_cond_destroy(&tp->condition);

	free(tp->threads);
	free(tp);
}
