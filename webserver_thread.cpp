#include "helper.h"
#include <pthread.h>
#include <iostream>
#include <string>
#include <vector>
#include <bits/stdc++.h>
#include <algorithm>

using namespace std;

enum RequestType
{
    GET,
    POST,
    NOTHING
};
enum State
{
    KEY,
    VAL
};


struct Args {

  size_t arg1;
  char * arg2;
  char * arg3;

};


/*------------------------------------------------------------------------*/
/*      construct a client header given informations                      */
string make_cli_header(string status, int cont_len, string type, string msg)
{
    ostringstream out;
    out << "HTTP/1.1 " << status << "\r\nContent-Type: text/" << type << "\r\n"
                                                                         "Content-length: "
        << cont_len << "\r\n\r\n"
        << msg;
    return out.str();
}
/*-------------------------------------------------------------------------*/


size_t sizeof_vector(vector<string>& vec)
{
    size_t ini_size = 0;
    string tmp;

    for(auto iter = vec.begin();iter != vec.end();iter++)
    {
        tmp = *iter;
        ini_size += tmp.size();
    }


    return ini_size;
}


void send_vector(int fd,vector<string>& vec)
{
    string tmp;

    for(auto iter = vec.begin();iter != vec.end();iter++)
    {
        tmp = *iter;
        rio_writen(fd, (void *)tmp.c_str(),tmp.size() * sizeof(char));
    }

}



/*-------------------------------------------------------------------------*/
/*     send set command to redis-server                                    */
int send_set_command(int fd,vector<string>& key,vector<string> &val)
{
    size_t val_size, size;
    size_t key_size;
    string size_str;

    val_size = sizeof_vector(val);
    key_size = sizeof_vector(key);

    string cmd_str = "", clrf("\r\n");

    cmd_str += "*3" + clrf + "$3" + clrf + "SET" + clrf + "$" + to_string(key_size) + clrf;
    size_str = clrf + "$" + to_string(val_size) + clrf;

    rio_writen(fd, (void *)cmd_str.c_str(), cmd_str.size() * sizeof(char));
    send_vector(fd,key);
    rio_writen(fd, (void *)size_str.c_str(),size_str.size() * sizeof(char));
    send_vector(fd,val);
    rio_writen(fd, (void *)clrf.c_str(),clrf.size() * sizeof(char));

    cmd_str.clear();
    return 1;
}
/*-------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*    send get command to redis-server                                      */
int send_get_command(int fd,vector<string>& key)
{
    size_t key_size;
    string cmd_str = "", clrf("\r\n");

    key_size = sizeof_vector(key);

    cmd_str = "*2" + clrf + "$3" + clrf + "GET" + clrf + "$" + to_string(key_size) + clrf;
     
    rio_writen(fd, (void *)cmd_str.c_str(), cmd_str.size() * sizeof(char));
    send_vector(fd, key);
    rio_writen(fd, (void *)clrf.c_str(), clrf.size() * sizeof(char));
    return 1;
}

/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*     set the key and val while parsing and call send functions            */
void set_key_val(int fd, string &str,vector<string>& key,vector<string>& val, enum State &state)
{

    size_t size = str.size(), tmp_size;
    size_t pos = 0, start = 0, val_size;

    enum State curr_state = state;
    string value = "";


    while (start <= size)
    {
        if (curr_state == KEY && ((pos = str.find("=", start)) != string::npos))
        {
            key.push_back(str.substr(start, pos - start));
            curr_state = VAL;
        }
        else if (curr_state == VAL && ((pos = str.find("&", start)) != string::npos))
        {
            val.push_back(str.substr(start, pos - start));
            curr_state = KEY;

            /* send to redis this pairs */
            send_set_command(fd, key, val);

           vector<string>().swap(key);
           vector<string>().swap(val);
        }
        else
        {

            switch (curr_state)
            {
            case KEY:
                key.push_back(str.substr(start, size - start));
                break;

            case VAL:
                val.push_back(str.substr(start, size - start));
                break;
            }

            break;
        }

        start = pos + 1;
    }

    state = curr_state;
}

/*------------------------------------------------------------------------------*/



size_t search_vector(vector<string>& vec,string key,size_t ind,size_t *i)
{
    size_t index;
    size_t vec_size = vec.size();
    string tmp;

    for(;ind < vec_size;ind++)
    {
        tmp = vec[ind];
        if((index = tmp.find(key)) != string::npos)
        {
            if(i)
            {
                *i = index;
            }
            return ind;
        }
    }

    return -1;
}


/*-------------------------------------------------------------------------------*/
/*      parse the request string                                                 */

enum RequestType parse(vector<string> &request)
{
    /* check for small letter get or post(if get find key) and HTTP/1.0 or HTTP/1.1 */

    enum RequestType type = NOTHING;
    size_t start_index, end_index, tmp = 0;
    size_t key_start, key_end, req_size;
    string beg, end;


    if ((key_end = search_vector(request, " HTTP/1.", tmp, NULL)) != -1)
    {

        if ((tmp = search_vector(request, "GET", 0, NULL)) != -1)
        {

            key_start = search_vector(request, "/", 0, &start_index);
            beg = request[key_start].substr(start_index + 1);
            auto iter_1 = request.begin() + (key_start-1);

            if (key_start == -1)
                return type;

            if (key_start != 0)
            {
                request.erase(request.begin(), iter_1);
            }
            else
            {
                request.erase(request.begin());
            }

            request.insert(request.begin(), beg);

            key_end = search_vector(request, " HTTP/1.", tmp, &end_index);
            end = request[key_end].substr(0, end_index);
            auto iter_2 = request.begin() + key_end;

            if (request.end() != iter_2)
            {
                request.erase(iter_2, request.end());
            }

            req_size = request.size();
            request.insert(request.begin() + req_size, end);

            type = GET;
        }
        else if ((tmp = search_vector(request, "POST", 0, NULL)) != -1)
        {
            type = POST;
        }
    }

    return type;
}


/*--------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------*/

void deliver_message(int fd, string status_msg, size_t size, string type, string msg)
{
    string sent_header = make_cli_header(status_msg, (int)size, type, msg);
    rio_writen(fd, (void *)sent_header.c_str(), sent_header.size() * sizeof(char));
}
/*---------------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------------*/

size_t recv_from_redis(int fd, buf_t *dyn_buf,vector<string> &vec, size_t size)
{
    size_t nleft = size;
    ssize_t nread;

    while (nleft > 0)
    {
        nread = read(fd, dyn_buf->buf, nleft);

        if (nread <= 0)
        {
            if (nread == 0)
                return nread;

            perror("read");
            return -1;
        }

        nleft -= nread;

        dyn_buf->buf[nread] = '\0';
        string str(dyn_buf->buf);
        vec.push_back(str);
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



size_t parse_header(rio_t&  rio,int connfd,buf_t * dyn_buf,vector<string>& request)
{

      size_t temp,content_len = 0,count = 0,cli_ret;
      bool finish = false;

      /*------------------------------------------------------------------*/
      /*          read the header sent by the ab client                   */

      rio_readinitb(&rio, connfd);

       while (1)
       {

           string piece = "";

            do
            {
                cli_ret = rio_readlineb(&rio, dyn_buf->buf, RIO_BUFSIZE - 1);

                if (cli_ret == -1)
                {
                    finish = true;
                    break;
                }

                if (cli_ret == RIO_BUFSIZE - 1)
                {
                    rio_readinitb(&rio, connfd);
                    string str(dyn_buf->buf);

                    if(!count)
                    {
                        request.push_back(str);
                    }
                }

            } while (cli_ret == RIO_BUFSIZE - 1);

            if (finish)
            {
                break;
            }

            if (cli_ret > 0)
            {
                string last_piece(dyn_buf->buf);
                piece += last_piece;


                if (!count)
                {
                    request.push_back(piece);
                }
            }

            /*---------------------------------------------------------------------*/

            if ((temp = search(piece, "content-length")) != string::npos)
            {
                temp += 16;
                string tmp = piece.substr(temp, 10);
                content_len = stoi(tmp);
            }


            count++;
	}
	
	return content_len;
}

void read_contents(rio_t&  rio,int connfd,int redisfd,size_t content_len,buf_t * dyn_buf)
{

    size_t rem = content_len;
    vector<string> key, val;
    ssize_t nread;
    enum State state = KEY;

    rio_read_all(&rio, dyn_buf->buf);
    dyn_buf->buf[rio.rio_cnt] = '\0';
    string str(dyn_buf->buf);

    set_key_val(redisfd, str, key, val, state);

    rem -= rio.rio_cnt;

    while (rem > 0)
    {
        nread = read(connfd, dyn_buf->buf, dyn_buf->len - 1);

        if (nread <= 0)
        {
            if (!nread)
                break;

            perror("error");
            exit(EXIT_FAILURE);
        }

        rem -= nread;
        dyn_buf->buf[nread] = '\0';
        string str(dyn_buf->buf);

        set_key_val(redisfd, str, key, val, state);
    }

    send_set_command(redisfd, key, val);

}
				
void * handle_connection(void * arguments)
{

	buf_t *dyn_buf = (buf_t *)calloc(1, sizeof(buf_t));
	dyn_buf->buf = (char *)calloc(BUFSIZE, sizeof(char));
	dyn_buf->len = BUFSIZE;

	size_t connfd,redisfd,cli_ret,serv_ret;
	char * ip_addr,*redis_port;
	struct Args * args;
	rio_t rio;
	enum RequestType type;
	vector<string> request,res;

	args = (struct Args *) arguments;

	connfd = args->arg1;
	ip_addr = args->arg2;
	redis_port = args->arg3;


	pthread_detach(pthread_self());


        /*------------------------------------------------------------------*/
        /*          read the header sent by the ab client                   */
	size_t content_len = parse_header(rio,connfd,dyn_buf,request);

        /* connect to redis server */

        if ((redisfd = open_clientfd(ip_addr, atoi(redis_port))) == -1)
        {
            exit(EXIT_FAILURE);
        }

	cout << "content-len is " << to_string(content_len);
        /*-------------------------------*/
        /* read the contents(if there is)*/
        if (content_len)
        {
           read_contents(rio,connfd,redisfd,content_len,dyn_buf);
        }
        /*------------------------------*/

        /*---------------------------------------------------------------*/
        /* parse the request and fetch the key(if there exists) */

        if ((type = parse(request)) != NOTHING)
        {
          
            if (type == GET)
            {
                send_get_command(redisfd,request);
               
            }

            /* first read */
            rio_readinitb(&rio, redisfd);
            cli_ret = rio_readlineb(&rio, dyn_buf->buf, RIO_BUFSIZE - 1);

            dyn_buf->buf[cli_ret] = '\0';
            string redis_out(dyn_buf->buf);


            /* posting succeeded */
            if ((serv_ret = redis_out.find("+OK")) != string::npos)
            {
                deliver_message(connfd, "200 OK", 2, "plain", "OK");
            }

            /* send ERROR */
            else if ((serv_ret = redis_out.find("$-")) != string::npos)
            {
                deliver_message(connfd, "404 Not Found", 5, "html", "ERROR");
            }


            /* parse the number and recv that amount */
            else if ((serv_ret = redis_out.find("$")) != string::npos)
            {

                serv_ret = stoi(redis_out.substr(serv_ret + 1, 10));
                size_t rd,min_rd;

                /* if read all */
                if(rio.rio_cnt > serv_ret)
                {
                    rd = rio.rio_cnt-2;
                    serv_ret = 0;
                }
                /* did not read either /r or /n or both */
                else
                {
                    rd = rio.rio_cnt;
                    serv_ret -= rd;
                }
                
                rio_read_all(&rio, dyn_buf->buf);
                dyn_buf->buf[rd] = '\0';
                string extra(dyn_buf->buf);
              

                res.push_back(extra);

                while (serv_ret)
                {
                    min_rd = dyn_buf->len-1;
                    if(min_rd > serv_ret)
                    {
                        min_rd = serv_ret;
                    }

                    cli_ret = recv_from_redis(redisfd, dyn_buf, res,min_rd);

                    if (cli_ret == 0)
                    {
                        break;
                    }

                    serv_ret -= cli_ret;
                }

                ostringstream out;
                cli_ret = sizeof_vector(res);

                out << "HTTP/1.1 "
                    << "200 OK"
                    << "\r\nContent-Type: text/"
                    << "plain"
                    << "\r\n"
                       "Content-length: "
                    << cli_ret << "\r\n\r\n";

                string sent_header = out.str();

                rio_writen(connfd, (void *)sent_header.c_str(), sent_header.size() * sizeof(char));
                send_vector(connfd,res);
            }

            else
            {
                deliver_message(connfd, "404 Not Found", 5, "html", "ERROR");
            }
        }

        /*---------------------------------------------------------------*/
        /* oops parsing failed   */
        else
        {
            deliver_message(connfd, "400 Bad Request", 5, "html", "ERROR");
        }

        /*---------------------------------------------------------------*/

    close(connfd);
    free(dyn_buf->buf);
	free(dyn_buf);
	free(arguments);
    return NULL;

}

/*-------------------------------------------------------------------------------*/

/* web server */
int main(int argc, char *argv[])
{

    int listenfd,connfd;
    struct sockaddr_in cliaddr, servaddr;
    socklen_t clilen = sizeof(cliaddr);

    char *port;

    if (argc != 4)
    {
        fprintf(stderr, "usage:./webserver_fork <port> <Ip> <redis-port>\n");
        exit(EXIT_FAILURE);
    }

    port = argv[1];

    if ((listenfd = open_listenfd(atoi(port))) < 0)
    {
        exit(EXIT_FAILURE);
    }


    for (;;)
    {
        if ((connfd = accept(listenfd, (struct sockaddr *)&cliaddr,
                             &clilen)) < 0)
        {
            perror("accept");
            continue;
        }
	
       struct Args * args = (struct Args *) calloc(1,sizeof(struct Args));

       args->arg1 = connfd;
       args->arg2 = argv[2];
       args->arg3 = argv[3];

       pthread_t tid;
       pthread_create(&tid,NULL,handle_connection,(void *) args);
    }

    close(listenfd);
    return 0;
}
