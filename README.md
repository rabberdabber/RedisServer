# Assignment 2

- Student ID: 20170844
- Your Name: Bereket Assefa
- Submission date and time: 17/10/20

## Ethics Oath
I pledge that I have followed all the ethical rules required by this course (e.g., not browsing the code from a disallowed source, not sharing my own code with others, so on) while carrying out this assignment, and that I will not distribute my code to anyone related to this course even after submission of this assignment. I will assume full responsibility if any violation is found later.

Name: Bereket Assefa
Date: 17/10/20

## Performance measurements
Set 8kB-sized value into a specific key. Measure the time for running 1,000 concurrent GET requests on the key using `ab -c 1000 -n 1000`.
- Part 1
  - Completed requests: 1000
  - Time taken for test: 140 ms
  - 99%-ile completion time: 105 ms
- Part 2
  - Completed requests: 1000
  - Time taken for test: 141 ms
  - 99%-ile completion time: 99 ms
- Part 3
  - Completed requests: 1000
  - Time taken for test: 132 ms
  - 99%-ile completion time: 95 ms

Briefly compare the performance of part 1 through 3 and explain the results.
I have found the performance of all three servers to be similar for small requests like for 1000 requests. for 1000 requests with concurrency 1, I have found that the time taken by webserver_fork is 1.255 sec, webserver_thread is 1.245 sec and the webserver_libevent is 1.041 sec with all having 99 percentile serving time of 1 ms. for 10,000 requests with 100 concurrency I have found webserver_fork to take 1.091 sec with 13 ms of 99 percentile serving time. webserver_thread took 1.1 sec(greater time than webserver_fork) with 13 ms 99 percentile serving time. And webserver_libevent took 1.05 sec with 12 ms of 99 percentile serving time. for 1000 requests with 1000 concurrency webserver_fork took 140 ms with 105 ms 99 percentile serving time. Meanwhile webserver_thread took 141 ms with 99 ms 99 percentile serving time. And finally the webserver_libevent took 132 ms with 95 ms 99 percentile serving time. To conclude the webserver_libevent is more faster than all the servers but more or less they are similar in their efficiency. I would say webserver_thread and webserver_fork are rather equal in efficiency for 1000 requests and less. The webserver_libevent is not that efficient as it works with only one thread meanwhile the other servers have many processes or threads. I believe we can make the webserver_libevent quite faster using multiple threads.

## Brief description
Briefly describe here how you implemented the project.
part 1: I have implemented the redis web server using a modularized approach, I have defined functions to set a single set command or to get a single get command, and functions that make a client header, and also used robust input output to parse the header information received from client. To make my webserver robust I have used vector of strings to hold the strings corresponding to the keys and values instead of a single string. I also defined a parsing function that parses multiple set commands. The function sends a set command if it finds either & or the content has ended, and will repeat as long as the content is done. I have also defined my own functions that receive and send messages to clients and servers. To make it robust I send messages as vectors so that any size of messages can be sent without difficulty.


