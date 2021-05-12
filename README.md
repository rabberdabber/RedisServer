


### Before you start

- Install redis (ref: [https://redis.io/download](https://redis.io/download))

bash
$ wget http://download.redis.io/releases/redis-6.0.8.tar.gz
$ tar xzf redis-6.0.8.tar.gz
$ cd redis-6.0.8
$ make
$ cd src 
$ ./redis-server --port [port]
$ ./redis-cli - p [port]

- Install libevent: "sudo apt-get install libevent-dev" 



### Running Example

protocol:

      POST: (inside the post body) [key]=[value]&[key]=[value]&[key]=[value]
      
      
      GET: `https://127.0.0.1:port/[key]`



- Client
    - POST file that contains the key-value pairs
     `ab -v 4 -p [file name] [http://]hostname[:webserver port]/`
    - GET key-value
     `ab -v 4 [http://]hostname[:webserver port]/[key]`

- Web Server

    `./webserver [webserver port] [redis ip] [redis port]`

- Redis

    `./redis-server --port [redis port]`


- Apache Bench (ab)
    - We will use ab as a benchmark to test the performance of
    your Web Servers.
    - Performance Test using Apache Bench:
        - ab with 1 concurrency 1,000 requests : `$ ab –n 1000 server_ip:server_port`
        - ab with 100 concurrency, 10,000 requests: `$ ab –c 100 -n 10000 server_ip:server_port`
    - Other options (run ab for more options)
    -p : postfile
    -v : verbosity
    -T : content-type **(**Note for Bonus: you should set this to application/x-www-form-urlencoded)






## Design Report for webserver_multi.cpp (**IMPORTANT**)

1. Explanation about thread pool, task queue, and task allocation (10 points)
    - design of thread pool, task queue: I designed the threadpool using 10 worker threads. The listener would call a method named init_pool when we start the server it will distribute 4 bufferevents that will be used for redis connection  to each thread. The listener thread( which is main thread) also allocates 10 event bases for each thread. When the listener thread spawn threads the threads will be in method activate_workers which is an infinite loop and only one thread has access to a job_queue at a time until it unlocks the mutex. the single worker thread will wait for a job in a conditional variable and when the listener thread adds a job to the thread pool it will notify that single worker then the worker thread will call the function dispatch which serves the client and returns the value and frees andy allocated memory. my write call back for client bufferevent has a logic to detect when the thread served the client and finished and will shut down the connection. The task allocation is a FIFO, using a single queue, which is achieved by the mutex lock _mutex_lock. I also used a queue to store the jobs which will be done by the worker threads. so the front job in the queue will be dispatched first before any other jobs. 

    - task allocation policy: the task allocation policy is the listener thread will give a socket and the thread will communicate with the client using the socket from the listener thread. The tasks are dequeued one by one in a FIFO fashion.


2. Explanation about Managing Redis connection, connection allocation (10 points)
    - Redis connection allocation policy
- redis connection allocation policy is I allocated redis connections in the listener thread(using their own bases allocated a bufferevent)  and distributed to the thread pool. each thread received 4 file descriptors(so also bufferevents). so max number of connection per thread is 4( FDS_PER_THREAD). The way I use this connections are i have 4 bufferevents for each thread so each worker has a field that stores the current_index the index. so the worker thread will use the bufferevent at index 0 first then will use the bufferevent at index 2 next time it gets notified by the listener thread and bufferevent at index 2 and then 3. and it repeats from the start like this so that we use all connections fairly. I noticed that it is necessary to drain the bufferevents after the the thread serves the client so I drained each bufferevents after I use it and will be cleaner for next use.

