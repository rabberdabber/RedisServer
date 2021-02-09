#include "thread_pool.h"
#include <stdio.h>

extern int event_base_dispatch(struct event_base *base);

pthread_cond_t _cond_mutex = PTHREAD_COND_INITIALIZER;
pthread_mutex_t _mutex_lock = PTHREAD_MUTEX_INITIALIZER;
queue<job_t *> job_queue;

static void *
activate_workers(void *tmp)
{
    worker_thread_t *worker = (worker_thread_t *)tmp;
    job_t *job = NULL;

    // connect to redis here
    // each thread has 4 fds

    for (;;)
    {

        pthread_mutex_lock(&_mutex_lock);

        // wait till job_queue is non-empty
        while (job_queue.empty())
        {
            pthread_cond_wait(&_cond_mutex, &_mutex_lock);
        }

        if (!job_queue.empty())
        {
            job = job_queue.front();

            // delete the front job
            job_queue.pop();
            worker->curr_fd_index = (worker->curr_fd_index + 1) & (FDS_PER_THREAD - 1);
            job->bev = worker->fds[worker->curr_fd_index];
            job->base = worker->base;
        }

        pthread_mutex_unlock(&_mutex_lock);

        // nothing to do
        if (!job)
        {
            continue;
        }

        job->exec_func(job);
    }
}

int init_pool(struct bufferevent **redis_connections, struct event_base **bases)
{
    int i;
    worker_thread_t *tmp;

    for (i = 0; i < NUM_OF_THREADS; i++)
    {
        tmp = (worker_thread_t *)calloc(1, sizeof(worker_thread_t));

        /* distribute the sockets to each thread */
        int num = 4 * i;
        tmp->fds[0] = redis_connections[num + 0];
        tmp->fds[1] = redis_connections[num + 1];
        tmp->fds[2] = redis_connections[num + 2];
        tmp->fds[3] = redis_connections[num + 3];

        tmp->base = bases[i];

        if (!tmp)
        {
            perror("calloc");
            return 0;
        }

        int ret = pthread_create(&tmp->tid, NULL, activate_workers, (void *)tmp);
        if (ret != 0)
        {
            perror("pthread_create");
            return 0;
        }
    }

    return 1;
}

void add_job_to_pool(job_t *job)
{
    pthread_mutex_lock(&_mutex_lock);
    job_queue.push(job);
    // notify workers
    pthread_cond_signal(&_cond_mutex);
    pthread_mutex_unlock(&_mutex_lock);
}
