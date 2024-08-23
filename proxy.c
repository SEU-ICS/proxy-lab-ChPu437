/*
  Proxy Lab
  TODOs:
    Part #1:
      [*] implement a proxy to accept incoming connections
      [] read and parse the HTTP request
      [] forward the request to the end server
      [] read the server's response
      [] forward the response to the client
    Part #2:
      [] deal with multiple connections
    Part #3:
      [] implement a cache
 */

#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

typedef struct
{
  char *method;
  char *root;
  char *directory;
} http_request;

http_request *new_request()
{
  http_request *request = malloc(sizeof(http_request));
  request->method = malloc(MAXLINE);
  request->root = malloc(MAXLINE);
  request->directory = malloc(MAXLINE);
  return request;
}

void free_request(http_request *request)
{
  free(request->method);
  free(request->root);
  free(request->directory);
  free(request);
}

// wait and parse incoming request
void parse_request(int connfd, http_request *request)
{
}

// check if request is cached
int check_if_cached(http_request *request)
{
  return 0;
}

// forward cached response to client
void cache_response(int connfd, http_request *request) {}

// forward request to server and forward response to client
void forward_response(int connfd, http_request *request) {}

void do_proxy_work(int connfd)
{
  // // allocate space for storing request
  http_request *request = new_request();
  // // parse incoming request
  parse_request(connfd, request);

  // // chek if request is cached
  if (check_if_cached(request))
  {
    // if yes, send cached response
    cache_response(connfd, request);
  }
  else
  {
    // if no, forward request to server
    forward_response(connfd, request);
  }

  // // free request
  free_request(request);
}

int main(int argc, char **argv)
{
  printf("%s", user_agent_hdr);

  // get listening port from args
  char *port;
  if (argc != 2)
  {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    exit(1);
  }
  else
  {
    port = argv[1];
  }

  // setting up listening socket
  int listenfd, connfd;
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  char client_hostname[MAXLINE], client_port[MAXLINE];

  listenfd = Open_listenfd(port);
  while (1)
  {
    // accept incoming connection
    clientlen = sizeof(struct sockaddr_storage);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, client_hostname,
                MAXLINE, client_port, MAXLINE, 0);
    printf("Connected to (%s, %s)\n", client_hostname, client_port);

#ifndef MULTI_CLIENT
    // do proxy work
    do_proxy_work(connfd);

    // close connection
    Close(connfd);
#endif
  }

  return 0;
}
