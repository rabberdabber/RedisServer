


#ifndef __HELPER_H__
#define __HELPER_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/wait.h>

/* Persistent state for the robust I/O (Rio) package */
/* $begin rio_t */

#define BUFSIZE 10240

#define RIO_BUFSIZE 8192
typedef struct
{
    int rio_fd;                /* Descriptor for this internal buf */
    int rio_cnt;               /* Unread bytes in internal buf */
    char *rio_bufptr;          /* Next unread byte in internal buf */
    char rio_buf[RIO_BUFSIZE]; /* Internal buffer */
} rio_t;
/* $end rio_t */

typedef struct 
{
    char * buf;
    size_t len;
} buf_t;

void rio_read_all(rio_t *rp, void *usrbuf);
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n);
ssize_t rio_writen(int fd, void *usrbuf, size_t n);
void rio_readinitb(rio_t *rp, int fd);
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);
int recv_from_host(int socket,buf_t * dyn_buf);
int open_clientfd(char *hostname, int port);
int open_listenfd(int port);
char *url_decode(const char *str);
char *url_encode(const char *str);

#endif 

