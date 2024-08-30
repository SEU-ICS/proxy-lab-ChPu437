/*
  Proxy Lab
  TODOs:
    Part #1:
      [*] implement a proxy to accept incoming connections
      [*] read and parse the HTTP request
        - [*] GET
        - [*] (opt) POST
      [*] forward the request to the end server
      [*] read the server's response
      [*] forward the response to the client
    Part #2:
      [*] deal with multiple connections
    Part #3:
      [] implement a cache
 */

#include <stdio.h>
#include "csapp.h"

#define DEBUG
#define MULTI_CLIENT
#define HAS_CACHE

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

typedef struct
{
  const char *method;
  char *root;
  char *directory;
  char *port;
} http_request_t;

http_request_t *new_request()
{
#ifdef DEBUG
  printf("[DEBUG] Creating new request\n");
#endif
  http_request_t *request = malloc(sizeof(http_request_t));
  request->root = malloc(MAXLINE);
  request->directory = malloc(MAXLINE);
  request->port = malloc(6);
  memset(request->root, 0, MAXLINE);
  memset(request->directory, 0, MAXLINE);
  memset(request->port, 0, 6);
  return request;
}

void free_request(http_request_t *request)
{
#ifdef DEBUG
  printf("[DEBUG] Freeing request\n");
#endif
  free(request->root);
  free(request->directory);
  free(request);
}

// wait and parse incoming request
int parse_request(int connfd, http_request_t *request)
{
  static const char *method_get = "GET";
  static const char *method_post = "POST";
  static const char *version_1_1 = "HTTP/1.1";
  static const char *version_1_0 = "HTTP/1.0";
  static const char *http_prefix = "http://";

#ifdef DEBUG
  printf("[DEBUG] Parsing request\n");
#endif

  char buf[MAXLINE];
  rio_t rio;

  Rio_readinitb(&rio, connfd);
  Rio_readlineb(&rio, buf, MAXLINE);

#ifdef DEBUG
  printf("[DEBUG] Server get request: %s", buf);
#endif

  // slice the request
#ifdef DEBUG
  printf("[DEBUG] Slicing request\n");
#endif
  char *req_method = strtok(buf, " ");
  char *req_uri = strtok(NULL, " ");
  char *req_version = strtok(NULL, "\r\n");

  // check method
#ifdef DEBUG
  printf("[DEBUG] Parsing method\n");
#endif
  if (strcmp(req_method, method_get) == 0)
  {
    request->method = method_get;
  }
  else if (strcmp(req_method, method_post) == 0)
  {
    request->method = method_post;
  }
  else
  {
    Rio_writen(connfd, "[PROXY] Invalid method\n", strlen("[PROXY] Invalid method\n"));
    return 1;
  }
  // check version
#ifdef DEBUG
  printf("[DEBUG] Parsing version\n");
#endif
  if (req_version == NULL)
  {
    Rio_writen(connfd, "[PROXY] Invalid version\n", strlen("[PROXY] Invalid version\n"));
    return 1;
  }
  else if (strcmp(req_version, version_1_1) == 0)
  {
    // do nothing
  }
  else if (strcmp(req_version, version_1_0) == 0)
  {
    // do nothing
  }
  else
  {
    Rio_writen(connfd, "[PROXY] Invalid version\n", strlen("[PROXY] Invalid version\n"));
    return 1;
  }
  // parse uri
#ifdef DEBUG
  printf("[DEBUG] Parsing uri\n");
#endif
  // // check format
  if (strncmp(req_uri, http_prefix, strlen(http_prefix)) != 0)
  {
    Rio_writen(connfd, "[PROXY] Invalid uri\n", strlen("[PROXY] Invalid uri\n"));
    return 1;
  }
  // // parse root
  req_uri = req_uri + strlen(http_prefix);
  strtok(req_uri, "/");
  strcpy(request->root, req_uri);
  if (req_uri == NULL)
  {
    Rio_writen(connfd, "[PROXY] Invalid uri\n", strlen("[PROXY] Invalid uri\n"));
    return 1;
  }

  // // parse directory
  char *req_directory = strtok(NULL, "/");
  if (req_directory != NULL)
    while (req_directory != NULL)
    {
      strcat(request->directory, "/");
      strcat(request->directory, req_directory);
      req_directory = strtok(NULL, "/");
    }
  else
    strcat(request->directory, "/");

  // // check if port is specified
  request->root = strtok(request->root, ":");
  char *req_port = strtok(NULL, ":");
  if (req_port != NULL)
  {
    // if yes, parse port
    int port = atoi(req_port);
    if (port == 0 || port > 65535)
    {
      Rio_writen(connfd, "[PROXY] Invalid port\n", strlen("[PROXY] Invalid port\n"));
      return 1;
    }
    else
    {
      sprintf(request->port, "%d", port);
    }
  }
  else
  {
    // if not, set default port
    sprintf(request->port, "%d", 80);
  }
#ifdef DEBUG
  printf("[DEBUG] Request method: %s\n", request->method);
  printf("[DEBUG] Request root: %s\n", request->root);
  printf("[DEBUG] Request port: %s\n", request->port);
  printf("[DEBUG] Request directory: %s\n", request->directory);
#endif

  return 0;
}

#ifdef HAS_CACHE
typedef struct
{
  http_request_t request;
  void *response;
  int size;
  time_t timestamp;
} cache_t;
const int cache_t_size = sizeof(cache_t) + MAX_OBJECT_SIZE;
void *cache_pool;

pthread_mutex_t cache_mutex;

void cache_init()
{
  // initialize cache mutex
  pthread_mutex_init(&cache_mutex, NULL);
  // allocate cache pool
  cache_pool = malloc(MAX_CACHE_SIZE);
  memset(cache_pool, 0, MAX_CACHE_SIZE);
}

void cache_destroy()
{
  free(cache_pool);
  pthread_mutex_destroy(&cache_mutex);
}

int request_is_same(http_request_t *requestA, http_request_t *requestB)
{
  if (requestA->method != requestB->method)
    return 0;
  if (strcmp(requestA->root, requestB->root) != 0)
    return 0;
  if (strcmp(requestA->directory, requestB->directory) != 0)
    return 0;
  if (strcmp(requestA->port, requestB->port) != 0)
    return 0;
  return 1;
}

int check_if_cached(http_request_t *request)
{
#ifdef DEBUG
  printf("[DEBUG] Checking if request is cached\n");
#endif
  pthread_mutex_lock(&cache_mutex);

  for (int i = 0; i < MAX_CACHE_SIZE / cache_t_size; i++)
  {
    cache_t *cache = cache_pool + i * cache_t_size;

    if (cache->response != NULL)
      if (request_is_same(request, &(cache->request)))
      {
        pthread_mutex_unlock(&cache_mutex);
        return i;
      }
  }

  pthread_mutex_unlock(&cache_mutex);
  return -1;
}

void delete_cache(int index)
{
  cache_t *cache = cache_pool + index * cache_t_size;
  free(cache->response);
  cache->response = NULL;
  cache->size = 0;
}

void new_cache(int index)
{
  cache_t *cache = cache_pool + index * cache_t_size;
  cache->request = *new_request();
  cache->response = malloc(MAX_OBJECT_SIZE);
  cache->size = 0;
  cache->timestamp = time(NULL);
}

int alloc_cache()
{
  int least_used = 0;
  cache_t *least_used_cache;
  for (int i = 0; i < MAX_CACHE_SIZE / cache_t_size; i++)
  {
    cache_t *cache = cache_pool + i * cache_t_size;
    least_used_cache = cache_pool + least_used * cache_t_size;
    if (cache->response == NULL)
    {
      new_cache(i);
      return i;
    }
    if (cache->timestamp < least_used_cache->timestamp)
    {
      least_used = i;
    }
  }
  // no available cache, delete least used cache
  delete_cache(least_used);
  new_cache(least_used);
  return least_used;
}

void request_copy(http_request_t *dest, http_request_t *src)
{
  dest->method = src->method;
  strcpy(dest->root, src->root);
  strcpy(dest->directory, src->directory);
  strcpy(dest->port, src->port);
}

int cache_response(http_request_t *request, void *response, int size)
{
#ifdef DEBUG
  printf("[DEBUG] Caching response\n");
#endif
  pthread_mutex_lock(&cache_mutex);

  int index = alloc_cache();
  cache_t *cache = cache_pool + index * cache_t_size;
  request_copy(&(cache->request), request);
  memcpy(cache->response, response, size);
  cache->size = size;
  cache->timestamp = time(NULL);

  pthread_mutex_unlock(&cache_mutex);
  return 0;
}

// forward cached response to client
void forward_cache(int connfd, int index)
{
#ifdef DEBUG
  printf("[DEBUG] Forwarding cached response\n");
#endif
  pthread_mutex_lock(&cache_mutex);
  cache_t *cache = cache_pool + index * cache_t_size;
  Rio_writen(connfd, cache->response, cache->size);
  pthread_mutex_unlock(&cache_mutex);
}

#endif

// forward request to server and forward response to client
void forward_request(int connfd, http_request_t *request)
{
#ifdef DEBUG
  printf("[DEBUG] Forwarding request\n");
#endif

#ifdef DEBUG
  printf("[DEBUG] Connecting to server\n");
#endif
  int serverfd = Open_clientfd(request->root, request->port);

#ifdef DEBUG
  printf("[DEBUG] Sending request to server\n");
#endif
  // NOTICE: /r/n
  char *request_line = malloc(MAXLINE);
  char *request_header = malloc(MAXLINE);
  sprintf(request_line, "%s %s %s\r\n", request->method, request->directory, "HTTP/1.0");
  sprintf(request_header, "Host: %s\r\n", request->root);
  Rio_writen(serverfd, request_line, strlen(request_line));
  Rio_writen(serverfd, request_header, strlen(request_header));
  Rio_writen(serverfd, (char *)user_agent_hdr, strlen(user_agent_hdr));
  Rio_writen(serverfd, "Connection: close\r\n", strlen("Connection: close\r\n"));
  Rio_writen(serverfd, "Proxy-Connection: close\r\n", strlen("Proxy-Connection: close\r\n"));
  Rio_writen(serverfd, "\r\n", strlen("\r\n"));

#ifdef DEBUG
  printf("[DEBUG] Receiving response from server and sending to client\n");
#endif

  char *response = malloc(MAXLINE);
  int n;
#ifdef HAS_CACHE
  void *cache_buf = malloc(MAX_OBJECT_SIZE);
  int cache_size = 0;
#endif
  while ((n = Rio_readn(serverfd, response, MAXLINE)) > 0)
  {
    Rio_writen(connfd, response, n);
#ifdef HAS_CACHE
    cache_size += n;
    if (cache_size < MAX_OBJECT_SIZE)
    {
      strcat(cache_buf, response);
    }
#endif
  }

#ifdef HAS_CACHE
  if (cache_size < MAX_OBJECT_SIZE)
    cache_response(request, cache_buf, cache_size);
  free(cache_buf);
#endif

  Close(serverfd);
  free(request_line);
  free(request_header);
  free(response);
}

#ifndef MULTI_CLIENT
void *do_proxy_work(int connfd)
{
#else
void *do_proxy_work(void *_connfd)
{
  int connfd = (int)_connfd;
#endif
#ifdef DEBUG
  printf("[DEBUG] Doing proxy work\n");
#endif
  // // allocate space for storing request
  http_request_t *request = new_request();
  // // parse incoming request
  if (parse_request(connfd, request))
  {
    // if error, free request and return
    free_request(request);
    return NULL;
  }

#ifndef HAS_CACHE
  forward_request(connfd, request);
#else
  // // check if request is cached
  int cache_index = check_if_cached(request);
  if (cache_index != -1)
  {
    // if yes, send cached response
    forward_cache(connfd, cache_index);
  }
  else
  {
    // if no, forward request to server (and cache it locally)
    forward_request(connfd, request);
  }
#endif

  // // free request
  free_request(request);
  return NULL;
}

int main(int argc, char **argv)
{

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

#ifdef HAS_CACHE
  cache_init();
#endif

  fprintf(stdout, "[PROXY] %s", user_agent_hdr);

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
    fprintf(stdout, "[PROXY] Connected to (%s, %s)\n", client_hostname, client_port);

#ifndef MULTI_CLIENT
    // do proxy work
    do_proxy_work(connfd);

    // close connection
    Close(connfd);
#else
    // create a new thread to handle the connection
    pthread_t tid;
    Pthread_create(&tid, NULL, do_proxy_work, (void *)connfd);
    Pthread_detach(tid);
#endif
  }

#ifdef HAS_CACHE
  cache_destroy();
#endif

  return 0;
}
