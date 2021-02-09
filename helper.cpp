
#include "helper.h"

/******************************** 
 * Client/server helper functions
 ********************************/


#define INCR_SIZE 5413

#define BACKLOG 1024

#define IS_ALNUM(ch)                \
    (ch >= 'a' && ch <= 'z') ||     \
        (ch >= 'A' && ch <= 'Z') || \
        (ch >= '0' && ch <= '9') || \
        (ch >= '-' && ch <= '.')



/* $begin rio_read */
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
    int cnt;

    while (rp->rio_cnt <= 0)
    { /* Refill if buf is empty */
        rp->rio_cnt = read(rp->rio_fd, rp->rio_buf,
                           sizeof(rp->rio_buf));
        if (rp->rio_cnt < 0)
        {
            if (errno != EINTR) /* Interrupted by sig handler return */
                return -1;
        }
        else if (rp->rio_cnt == 0) /* EOF */
            return 0;
        else
            rp->rio_bufptr = rp->rio_buf; /* Reset buffer ptr */
    }

    /* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
    cnt = n;
    if (rp->rio_cnt < n)
        cnt = rp->rio_cnt;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}
/* $end rio_read */

/*
 * rio_readnb - Robustly read n bytes (buffered)
 */
/* $begin rio_readnb */
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = (char *) usrbuf;

    while (nleft > 0)
    {
        if ((nread = rio_read(rp, bufp, nleft)) < 0)
            return -1; /* errno set by read() */
        else if (nread == 0)
            break; /* EOF */
        nleft -= nread;
        bufp += nread;
    }
    return (n - nleft); /* return >= 0 */
}
/* $end rio_readnb */

/*
 * rio_writen - Robustly write n bytes (unbuffered)
 */
/* $begin rio_writen */
ssize_t rio_writen(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = (char *)usrbuf;

    while (nleft > 0)
    {
        if ((nwritten = write(fd, bufp, nleft)) <= 0)
        {
            if (errno == EINTR) /* Interrupted by sig handler return */
                nwritten = 0;   /* and call write() again */
            else
                return -1; /* errno set by write() */
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    return n;
}
/* $end rio_writen */

/*
 * rio_readinitb - Associate a descriptor with a read buffer and reset buffer
 */
/* $begin rio_readinitb */
void rio_readinitb(rio_t *rp, int fd)
{
    rp->rio_fd = fd;
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}
/* $end rio_readinitb */

void rio_read_all(rio_t *rp,void *usrbuf)
{
    memcpy(usrbuf,rp->rio_bufptr,rp->rio_cnt);
    return;
}
/* 
 * rio_readlineb - Robustly read a text line (buffered)
 */
/* $begin rio_readlineb */
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen)
{
    int n, rc;
    char c,prev,*bufp = (char *) usrbuf;

    for (n = 1; n < maxlen; n++)
    {
        if ((rc = rio_read(rp, &c, 1)) == 1)
        {
            *bufp++ = c;
            
            if (c == '\n' && prev == '\r')
            {
                /* end of header line */
                if(n == 2)
                {
                    return -1;
                }
                
                n++;
                break;
            }
        }
        else if (rc == 0)
        {
            if (n == 1)
                return 0; /* EOF, no data read */
            else
                break; /* EOF, some data was read */
        }
        else
            return -1; /* Error */

        prev = c;
    }

    *bufp = 0;
    return n - 1;
}
/* $end rio_readlineb */


int open_listenfd(int port)
{
    int listenfd, optval = 1;
    struct sockaddr_in serveraddr = {0};

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return -1;
    }

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&optval, sizeof(int)) < 0)
    {
        close(listenfd);
        perror("setsockopt");
        return -1;
    }

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenfd, (struct sockaddr *)&serveraddr,
             sizeof(serveraddr)) < 0)
    {
        close(listenfd);
        perror("bind");
        return -1;
    }

    if (listen(listenfd, BACKLOG) < 0)
    {
        close(listenfd);
        perror("listen");
        return -1;
    }

    return listenfd;
}

int open_clientfd(char *hostname, int port)
{
    int clientfd, result;
    struct hostent *hp;
    struct sockaddr_in serveraddr = {0};

    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return -1;
    }

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);

    /* the given String is a valid ip address */
    if ((result = inet_pton(AF_INET, hostname, &(serveraddr.sin_addr)) == 1))
    {
    }

    else
    {
        if ((hp = gethostbyname(hostname)) == NULL)
        {
            close(clientfd);
            perror("gethostbyname");
            return -1;
        }

        bzero((char *)&serveraddr, sizeof(serveraddr));

        serveraddr.sin_family = AF_INET;

        bcopy((char *)hp->h_addr_list[0],
              (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
        serveraddr.sin_port = htons(port);
    }

    if (connect(clientfd, (struct sockaddr *)&serveraddr,
                sizeof(serveraddr)) < 0)
    {
        close(clientfd);
        perror("connect");
        return -1;
    }

    return clientfd;
}

char *url_decode(const char *str)
{
    int i, j = 0, len;
    char *tmp;
    char hex[3];
    len = strlen(str);
    hex[2] = 0;

    tmp = (char *)malloc(sizeof(char) * (len + 1));

    for (i = 0; i < len; i++, j++)
    {
        if (str[i] != '%')
            tmp[j] = str[i];
        else
        {
            if (IS_ALNUM(str[i + 1]) && IS_ALNUM(str[i + 2]) && i < (len - 2))
            {
                hex[0] = str[i + 1];
                hex[1] = str[i + 2];
                tmp[j] = strtol(hex, NULL, 16);
                i += 2;
            }
            else
                tmp[j] = '%';
        }
    }
    tmp[j] = 0;

    return tmp;
}
char *url_encode(const char *str)
{
    int i, j = 0, len;

    char *tmp;

    len = strlen(str);
    tmp = (char *)malloc((sizeof(char) * 3 * len) + 1);
    for (i = 0; i < len; i++)
    {
        if (IS_ALNUM(str[i]))
            tmp[j] = str[i];
        else
        {

            snprintf(&tmp[j], 4, "%%%02X", (unsigned char)str[i]);
            j += 2;
        }
        j++;
    }
    tmp[j] = 0;
    return tmp;
}


