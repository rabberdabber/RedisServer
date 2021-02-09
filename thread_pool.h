#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <queue>
#include <stdlib.h>
#include <stdio.h>

#define NUM_OF_THREADS 10
#define FDS_PER_THREAD 4
using namespace std;

typedef struct job
{
    void (*exec_func)(struct job *job);
    void *func_data;
    struct event_base *base;
    struct bufferevent *bev;
} job_t;

typedef struct worker_thread
{
    pthread_t tid;
    struct bufferevent *fds[FDS_PER_THREAD];
    int curr_fd_index;
    struct event_base *base;
} worker_thread_t;

int init_pool(struct bufferevent **redis_bev, struct event_base **redis_bases);

void destroy_pool();

void add_job_to_pool(job_t *job);

#endif
