::Makefile for Assignment 2


SERVER_FORK_SRC = webserver_fork.cpp
SERVER_FORK_AUX = helper.cpp
SERVER_FORK_HDR = helper.h

SERVER_THREAD_SRC = webserver_thread.cpp
SERVER_THREAD_HDR =

SERVER_MULTI_SRC = webserver_multi.cpp
SERVER_MULTI_AUX = thread_pool.cpp
SERVER_MULTI_HDR = thread_pool.h

##############################

CC=g++
CPPFLAGS= -std=c++11
LDFLAGS= -lpthread -levent -levent_core
LIBS_PATH= -L/usr/local/lib

build: bin/webserver_fork bin/webserver_thread bin/webserver_multi

bin/webserver_fork: $(SERVER_FORK_SRC) $(SERVER_FORK_HDR) bin
	$(CC) $(CPPFLAGS) -o $@ $(SERVER_FORK_SRC) $(SERVER_FORK_AUX) $(LIBS_PATH) $(LDFLAGS)

bin/webserver_thread: $(SERVER_THREAD_SRC) $(SERVER_FORK_HDR) bin
	$(CC) $(CPPFLAGS) -o $@ $(SERVER_THREAD_SRC) $(SERVER_FORK_AUX) $(LIBS_PATH) $(LDFLAGS)

bin/webserver_multi: $(SERVER_MULTI_SRC) $(SERVER_MULTI_HDR) bin
	$(CC) $(CPPFLAGS) -o $@ $(SERVER_MULTI_SRC) $(SERVER_MULTI_AUX) $(LIBS_PATH) $(LDFLAGS)

.PHONY: clean build

bin:
	mkdir -p bin

clean:
	rm -rf bin
