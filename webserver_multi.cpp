#include<unistd.h>
#include <arpa/inet.h>
#include <assert.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <iostream>
#include <string>
#include <vector>
#include <bits/stdc++.h>
#include <algorithm>
#include <bits/stdc++.h>
#include "thread_pool.h"

using namespace std;

/* change the backlog value */
#define BACKLOG 1024
#define MIN(x, y) ((x) > (y) ? (y) : (x))
#define BUFSIZE 20480
#define MAX_REDIS_CONNECTIONS 40

long long served = 1;

typedef struct
{
    char *buf;
} buf_t;

enum RequestType
{
    GET,
    POST,
    NOTHING
};

enum Receiver
{
    CLIENT,
    REDIS
};

enum State
{
    KEY,
    VAL
};

typedef struct cli_state
{
    buf_t *dyn_buf;
    char *buffer;
    vector<string> key;
    vector<string> val;
    vector<string> res;
    vector<string> request;
    enum RequestType cmd_type;
    enum State current_state;
    struct bufferevent *client_bev;
    bool rd_header;
    bool msg_delivered;
    bool done_with_client;
    size_t content_len;
    size_t redis_recv_amt;
    size_t client_send_amt;
    int fd;
    struct bufferevent *redis_bev;
    struct event_base *cli_redis_base;
} client_info;

// redis port
const char *redis_port;

// ip address of redis
const char *ip_addr;

// server port
const char *serv_port;

/*------------------------------------------------------------------------*/
/*      construct a client header given informations                      */
string make_cli_header(string status, int cont_len, string type, string msg)
{
    ostringstream out;

    out << "HTTP/1.1 " << status << "\r\nContent-Type: text/" << type << "\r\n"
    "Content-length: " << cont_len << "\r\n\r\n" << msg;

    return out.str();
}
/*-------------------------------------------------------------------------*/

size_t sizeof_vector(vector<string> &vec)
{
    size_t ini_size = 0;
    string tmp;

    for (auto iter = vec.begin(); iter != vec.end(); iter++)
    {
        tmp = *iter;
        ini_size += tmp.size();
    }

    return ini_size;
}

void send_vector(client_info *cli_info, vector<string> &vec, enum Receiver tmp)
{
    string temp;

    struct bufferevent *bev;

    switch (tmp)
    {
    case CLIENT:
        bev = cli_info->client_bev;
        break;

    case REDIS:
        bev = cli_info->redis_bev;
        break;
    }

    for (size_t i = 0; i < vec.size(); i++)
    {
        temp = vec[i];

        if (tmp == CLIENT)
        {
            cli_info->client_send_amt -= temp.size();
        }
        bufferevent_write(bev, (void *)temp.c_str(), temp.size() * sizeof(char));
    }
}

/*-------------------------------------------------------------------------*/
/*     send set command to redis-server                                    */

int send_set_command(client_info *cli_info)
{
    struct bufferevent *bev = cli_info->redis_bev;

    assert(bev);

    size_t val_size;
    size_t key_size;
    string size_str;

    val_size = sizeof_vector(cli_info->val);
    key_size = sizeof_vector(cli_info->key);

    string cmd_str = "", clrf("\r\n");

    cmd_str += "*3" + clrf + "$3" + clrf + "SET" + clrf + "$" + to_string(key_size) + clrf;
    size_str = clrf + "$" + to_string(val_size) + clrf;

    bufferevent_write(bev, (void *)cmd_str.c_str(), cmd_str.size() * sizeof(char));
    send_vector(cli_info, cli_info->key, REDIS);
    bufferevent_write(bev, (void *)size_str.c_str(), size_str.size() * sizeof(char));
    send_vector(cli_info, cli_info->val, REDIS);
    bufferevent_write(bev, (void *)clrf.c_str(), clrf.size() * sizeof(char));

    cmd_str.clear();
    return 1;
}

/*-------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*    send get command to redis-server                                      */

int send_get_command(client_info *cli_info)
{
    size_t key_size;
    string cmd_str = "", clrf("\r\n");
    struct bufferevent *bev = cli_info->redis_bev;

    assert(bev);

    key_size = sizeof_vector(cli_info->request);

    cmd_str = "*2" + clrf + "$3" + clrf + "GET" + clrf + "$" + to_string(key_size) + clrf;

    bufferevent_write(bev, (void *)cmd_str.c_str(), cmd_str.size() * sizeof(char));
    bufferevent_flush(bev, EV_WRITE, BEV_FLUSH);

    send_vector(cli_info, cli_info->request, REDIS);
    bufferevent_write(bev, (void *)clrf.c_str(), clrf.size() * sizeof(char));
    return 1;
}

/*--------------------------------------------------------------------------*/
/*     set the key and val while parsing and call send functions            */
void set_key_val(struct bufferevent *bev, string &str, client_info *cli_info)
{

    size_t size = str.size();
    size_t pos = 0, start = 0;

    enum State curr_state = cli_info->current_state;
    string value = "";

    while (start <= size)
    {
        if (curr_state == KEY && ((pos = str.find("=", start)) != string::npos))
        {
            cli_info->key.push_back(str.substr(start, pos - start));
            curr_state = VAL;
        }
        else if (curr_state == VAL && ((pos = str.find("&", start)) != string::npos))
        {
            cli_info->val.push_back(str.substr(start, pos - start));
            curr_state = KEY;

            /* send to redis this pairs */
            send_set_command(cli_info);

            vector<string>().swap(cli_info->key);
            vector<string>().swap(cli_info->val);
        }
        else
        {

            switch (curr_state)
            {
            case KEY:
                cli_info->key.push_back(str.substr(start, size - start));
                break;

            case VAL:
                cli_info->val.push_back(str.substr(start, size - start));
                break;
            }

            break;
        }

        start = pos + 1;
    }

    cli_info->current_state = curr_state;
}

/*------------------------------------------------------------------------------*/

size_t search_vector(vector<string> &vec, string key, size_t ind, size_t *i)
{
    size_t index;
    size_t vec_size = vec.size();
    string tmp;

    for (; ind < vec_size; ind++)
    {
        tmp = vec[ind];
        if ((index = tmp.find(key)) != string::npos)
        {
            if (i)
            {
                *i = index;
            }
            return ind;
        }
    }

    return -1;
}

/*--------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------*/

void deliver_message(client_info *cli_info, string status_msg, size_t size, string type, string msg)
{
    assert(cli_info && cli_info->client_bev);
    string sent_header = make_cli_header(status_msg, (int)size, type, msg);
    bufferevent_write(cli_info->client_bev, sent_header.c_str(), sent_header.size() * sizeof(char));
    cli_info->msg_delivered = true;
}
/*---------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------*/

size_t recv_from_redis(client_info *cli_info, size_t size)
{
    size_t nleft = size, min_rd, nread, tmp, amt_left;
    ssize_t rd;

    buf_t *buffer = cli_info->dyn_buf;
    struct evbuffer *input = bufferevent_get_input(cli_info->redis_bev);

    while (nleft > 0)
    {
        nread = bufferevent_read(cli_info->redis_bev, buffer->buf, nleft);
        size_t len = evbuffer_get_length(input);
        evbuffer_drain(input, len);

        if (nleft > nread)
        {
            amt_left = BUFSIZE - nread - 1;
            tmp = nleft - nread;

            min_rd = MIN(amt_left, tmp);
            int fd = bufferevent_getfd(cli_info->redis_bev);

            if ((rd = read(fd, buffer->buf + nread, min_rd)) > 0)
            {
                nread += rd;
            }
        }

        if (nread <= 0)
        {
            if (nread == 0)
                return nread;

            perror("read");
            return -1;
        }

        nleft -= nread;

        buffer->buf[nread] = '\0';
        string str(buffer->buf);
        cli_info->res.push_back(str);
    }

    return nread;
}

/*-------------------------------------------------------------------------------*/

size_t search(std::string str, std::string substr, size_t pos = 0)
{
    transform(str.begin(), str.end(), str.begin(), ::tolower);
    transform(substr.begin(), substr.end(), substr.begin(), ::tolower);
    return str.find(substr, pos);
}

void parse_headerline(client_info *cli_info)
{

    /*------------------------------------------------------------------*/
    /*          read the headerline sent by the ab client               */

    // make a string line
    char *temp, *ptr;

    // insert it into request vector

    if ((temp = strcasestr(cli_info->buffer, "GET /")))
    {
        temp += 5;
        cli_info->buffer = strtok_r(temp, " ", &ptr);
        string str(temp);
        cli_info->request.push_back(str);
        cli_info->cmd_type = GET;
    }

    /*---------------------------------------------------------------------*/

    if ((temp = strcasestr(cli_info->buffer, "content-length")))
    {
        temp += 16;
        cli_info->content_len = (size_t)atoi(temp);
    }
}

size_t
read_contents(struct bufferevent *bev, client_info *cli_info)
{

    size_t rem = cli_info->content_len, tmp = 0, min_rd, len, nread, nleft = rem;
    ssize_t rd;

    struct evbuffer *input = bufferevent_get_input(cli_info->client_bev);

    while (rem > 0)
    {

        nread = bufferevent_read(cli_info->client_bev, cli_info->dyn_buf->buf, BUFSIZE - 1);
        len = evbuffer_get_length(input);

        if (len > 0)
            evbuffer_drain(input, len);

        if (rem > nread)
        {
            nleft = rem - nread;
        }

        /* read the extra data from the socket */
        if (nleft > 0)
        {
            tmp = BUFSIZE - nread - 1;
            min_rd = MIN(nleft, tmp);

            int fd = bufferevent_getfd(cli_info->client_bev);
            if ((rd = read(fd, cli_info->dyn_buf->buf + nread, min_rd)) > 0)
            {
                nread += rd;
            }
        }

        tmp += nread;

        if (nread <= 0)
        {
            if (!nread)
            {
                if (rem == 0)
                    cli_info->done_with_client = true;

                break;
            }

            perror("error");
            exit(EXIT_FAILURE);
        }

        rem -= nread;
        cli_info->dyn_buf->buf[nread] = '\0';
        string str(cli_info->dyn_buf->buf);
        set_key_val(bev, str, cli_info);
    }

    send_set_command(cli_info);

    return tmp;
}

void free_cli_info(client_info *cli_info)
{
    assert(cli_info);

    free(cli_info->dyn_buf->buf);
    free(cli_info->dyn_buf);
    bufferevent_free(cli_info->client_bev);
    free(cli_info);
}

client_info *alloc_cli_info()
{

    client_info *cli_info = (client_info *)calloc(1, sizeof(client_info));
    buf_t *dyn_buf;

    if (!cli_info)
    {
        perror("calloc");
        return NULL;
    }

    cli_info->dyn_buf = dyn_buf = (buf_t *)calloc(1, sizeof(buf_t));

    if (!dyn_buf)
    {
        perror("calloc");
        free(cli_info);
        return NULL;
    }

    dyn_buf->buf = (char *)calloc(BUFSIZE, sizeof(char));

    if (!dyn_buf->buf)
    {
        perror("calloc");
        free(dyn_buf);
        free(cli_info);
        return NULL;
    }

    cli_info->rd_header = false;
    cli_info->cmd_type = POST;

    return cli_info;
}

void read_from_redis_cb(struct bufferevent *bev, void *ctx)
{
    assert(bev && ctx);

    client_info *cli_info = (client_info *)ctx;

    if (cli_info->redis_recv_amt == 0 && !cli_info->msg_delivered)
    {
        size_t rd, serv_ret;

        char *buf;

        /* read the first line from redis                */
        /* the bufferevent must be enabled and set first */
        struct evbuffer *input = bufferevent_get_input(cli_info->redis_bev);

        buf = evbuffer_readln(input, &rd, EVBUFFER_EOL_LF);

        buf[rd] = '\0';

        string redis_out(buf);

        if ((serv_ret = redis_out.find("+OK")) != string::npos)
        {
            deliver_message(cli_info, "200 OK", 2, "plain", "OK");
        }

        else if ((serv_ret = redis_out.find("$-")) != string::npos)
        {
            deliver_message(cli_info, "404 Not Found", 5, "html", "ERROR");
        }

        else if ((serv_ret = redis_out.find("$")) != string::npos)
        {
            serv_ret = stoi(redis_out.substr(serv_ret + 1, 10));
            cli_info->redis_recv_amt = serv_ret;
        }

        else
        {

            deliver_message(cli_info, "404 Not Found", 5, "html", "ERROR");
        }
    }

    // have some redis data to receive
    if (cli_info->redis_recv_amt && !cli_info->msg_delivered)
    {
        size_t min_rd, cli_ret;

        while (cli_info->redis_recv_amt)
        {

            min_rd = MIN(BUFSIZE - 1, cli_info->redis_recv_amt);

            cli_ret = recv_from_redis(cli_info, min_rd);
            cli_info->redis_recv_amt -= cli_ret;

            if (cli_ret == 0)
            {
                break;
            }
        }

        // done receiving from redis
        if (cli_info->redis_recv_amt == 0)
        {
            ostringstream out;
            cli_ret = sizeof_vector(cli_info->res);

            out << "HTTP/1.1 "
                << "200 OK"
                << "\r\nContent-Type: text/"
                << "plain"
                << "\r\n"
                   "Content-length: "
                << cli_ret << "\r\n\r\n";

            string sent_header = out.str();

            bufferevent_write(cli_info->client_bev, (void *)sent_header.c_str(), sent_header.size() * sizeof(char));
            cli_info->client_send_amt = cli_ret;
            send_vector(cli_info, cli_info->res, CLIENT);
            cli_info->msg_delivered = true;
        }
    }
}

void read_from_client_cb(struct bufferevent *bev, void *ctx)
{
    assert(bev && ctx);

    struct evbuffer *input;
    client_info *cli_info = (client_info *)ctx;

    string line;
    size_t len;
    bool equal;
    char *buf;
    size_t rd;

    input = bufferevent_get_input(bev);

    /*===========================================================*/
    /*     parse the headers until you are done                  */
    if (!cli_info->rd_header)
    {
        do
        {
            buf = evbuffer_readln(input, &rd, EVBUFFER_EOL_LF);
            equal = buf[0] == '\r';
            buf[rd - 1] = '\0';
            cli_info->buffer = buf;
            if (equal == 1)
            {
                // done with headers
                cli_info->rd_header = true;
            }
            len = strlen(cli_info->buffer);
            parse_headerline(cli_info);
            free(buf);

        } while (len >= 2 && !cli_info->rd_header);
    }

    /*===========================================================*/

    if (cli_info->rd_header && !cli_info->done_with_client)
    {

        //enable and set call backs for redis connections
        bufferevent_setcb(cli_info->redis_bev, read_from_redis_cb, NULL, NULL, cli_info);
        bufferevent_enable(cli_info->redis_bev, EV_READ | EV_WRITE | EV_PERSIST);

        if (cli_info->cmd_type == POST)
        {

            /*====================================*/
            /* allocate redis_key and val structs */

            // allocate the key buffer

            if (cli_info->content_len)
            {
                cli_info->content_len -= read_contents(cli_info->redis_bev, cli_info);
            }

            if (!cli_info->content_len)
            {
                send_set_command(cli_info);
            }
        }

        else if (cli_info->cmd_type == GET)
        {

            send_get_command(cli_info);
        }

        else
        {
            deliver_message(cli_info, "404 Not Found", 5, "html", "ERROR");
        }

        cli_info->done_with_client = true;
    }
}

void write_to_client_cb(struct bufferevent *bev, void *ctx)
{
    client_info *cli_info = (client_info *)ctx;
    struct evbuffer *output = bufferevent_get_output(bev);
    size_t len_out = evbuffer_get_length(output);

    // if finished delivering to client
    if (!len_out && (cli_info->msg_delivered && cli_info->client_send_amt == 0))
    {
        shutdown(cli_info->fd, SHUT_RDWR);

        event_base_loopexit(cli_info->cli_redis_base, NULL);
    }
}

static void dispatch(struct job *job)
{
    // fetch cli_info
    client_info *cli_info = (client_info *)job->func_data;
    // fetch the redis bufferevent
    cli_info->redis_bev = job->bev;

    // use the job for any DS from thread
    // assign a thread local base for the client-redis pair
    cli_info->cli_redis_base = job->base;

    cli_info->client_bev = bufferevent_socket_new(cli_info->cli_redis_base, cli_info->fd, BEV_OPT_CLOSE_ON_FREE);

    if (!cli_info->client_bev)
    {
        perror("bufferevent_socket_new");
        return;
    }

    bufferevent_setcb(cli_info->client_bev, read_from_client_cb, write_to_client_cb, NULL, (void *)cli_info);
    bufferevent_enable(cli_info->client_bev, EV_READ | EV_WRITE | EV_PERSIST);

    //enable and set call backs for redis connections
    bufferevent_setcb(cli_info->redis_bev, read_from_redis_cb, NULL, NULL, cli_info);
    bufferevent_enable(cli_info->redis_bev, EV_READ | EV_WRITE | EV_PERSIST);

    event_base_dispatch(cli_info->cli_redis_base);

    // drain any data from redis bufferevent for next use
    struct evbuffer *input = bufferevent_get_input(cli_info->redis_bev);
    size_t len = evbuffer_get_length(input);

    if (len > 0)
        evbuffer_drain(input, len);

    job->bev = NULL;
    free_cli_info(cli_info);
    free(job);
}

void accept_connection(evutil_socket_t listener, short event, void *arg)
{
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);
    job_t *job;

    int fd = accept(listener, (struct sockaddr *)&ss, &slen);

    /* the listener thread gave away the socket */

    if (fd < 0)
        perror("accept");

    else
    {
        client_info *cli_info = alloc_cli_info();

        if (!cli_info)
        {
            close(fd);
            return;
        }

        evutil_make_socket_nonblocking(fd); /*make non-blocking*/

        cli_info->fd = fd;

        /* create job and insert it */
        job = (job_t *)calloc(1, sizeof(job_t));

        if (!job)
        {
            perror("calloc");
            free_cli_info(cli_info);
            return;
        }

        job->exec_func = dispatch;
        job->func_data = cli_info;

        //add the job to the pool
        printf("added job#%llu\n",served++);
        fflush(stdout);
        add_job_to_pool(job);
    }
}

void run_server()
{
    int optval = 1;
    evutil_socket_t listener;
    struct sockaddr_in sin;
    struct event_base *base;
    struct event *listener_event;

    base = event_base_new();
    if (!base)
        return;

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(atoi(serv_port));

    if ((listener = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return;
    }

    evutil_make_socket_nonblocking(listener);

    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    if (bind(listener, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        perror("bind");
        return;
    }

    if (listen(listener, BACKLOG) < 0)
    {
        perror("listen");
        return;
    }

    struct sockaddr_in SockAddr = {0};
    SockAddr.sin_family = AF_INET;
    SockAddr.sin_port = htons(atoi(redis_port));
    SockAddr.sin_addr.s_addr = inet_addr(ip_addr);
    struct bufferevent *redis_connections[MAX_REDIS_CONNECTIONS];
    struct event_base *redis_bases[10];

    for (int i = 0; i < 10; i++)
    {
        redis_bases[i] = event_base_new();

        if (!redis_bases[i])
        {
            fprintf(stderr, "could not allocate bases\n");
            return;
        }
    }

    for (int i = 0; i < MAX_REDIS_CONNECTIONS; i++)
    {
        /*=======================================*/
        if (!(redis_connections[i] = bufferevent_socket_new(redis_bases[i / 4], -1, BEV_OPT_CLOSE_ON_FREE)))
        {
            fprintf(stderr, "couldn't create bufferevent\n");
            return;
        }

        bufferevent_setcb(redis_connections[i], read_from_redis_cb, NULL, NULL, NULL);

        if (bufferevent_socket_connect(redis_connections[i], (struct sockaddr *)&SockAddr, sizeof(SockAddr)) < 0)
        {
            fprintf(stderr, "couldn't connect to bufferevent\n");
            return;
        }

        bufferevent_enable(redis_connections[i], EV_READ | EV_WRITE | EV_PERSIST);
    }

    if (!init_pool(redis_connections, redis_bases))
    {
        fprintf(stderr, "failed creating thread pool!!!!\n");
        exit(EXIT_FAILURE);
    }

    listener_event = event_new(base, listener, EV_READ | EV_WRITE | EV_PERSIST, accept_connection, (void *)base);

    event_add(listener_event, NULL);
    event_base_dispatch(base);

    event_base_free(base);
    shutdown(listener, SHUT_RDWR);
    close(listener);
}

/*-------------------------------------------------------------------------------*/

/* web server */
int main(int argc, char *argv[])
{

    if (argc != 4)
    {

        fprintf(stderr, "Usage: ./webserver_libevent <port> <ip_address> <redis-port>");
        exit(EXIT_FAILURE);
    }

    redis_port = (const char *)argv[3];
    ip_addr = (const char *)argv[2];
    serv_port = (const char *)argv[1];

    run_server();
    return 0;
}
