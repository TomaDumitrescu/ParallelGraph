# Parallel Graph

## Description:

The project involves processing the graph nodes in parallel manner, using a thread pool design pattern. The sum of all
node values is calculated by starting with node 0, then to its unvisited neighbors and so on, every discovered node being
considered as a new task that will be added in the waiting queue of the thread pool.

## Implementation:

1) Thread pool:

The thread pool offers the following functions: create_task(...); destroy_task(...); enqueue_task(...): the mutex will
lock the queue of tasks that can be accessed by other threads, and then it will add the task in the list, then increase
the number of ready tasks, then signal that the queue is not empty -- just enqueued an element; queue_is_empty(...);
dequeue_task(...): no ready tasks, then wait (otherwise dequeue from empty will result in an error), get and delete the
head node (pop_front), then the head task will move from ready to running; thread_loop_function(...) which executes
the tasks from the queue until the work_done flag changes; wait_for_completion(...) finishes all tasks from the queue and
changes the flag work_done, then joins the threads; create_threadpool(...) which initializes the data; destroy_threadpool(...).

2) Parallel graph processing:

- A threadpool for parallel processing is created, and also some synchronization mechanisms for the graph
- Create a task from the node 0 (that will be put to PROCESSING), then add it to the thread pool
- process_node(): for the node N, if it is not in PROCESSING state (running task), then discard it. Otherwise,
add it to the global sum (using a mutex) and mark it as DONE. Then, traverse its neighbors and process every neighbor that
was not visited in the same manner as node 0 was.

## Notes:
https://en.wikipedia.org/wiki/Thread_pool
