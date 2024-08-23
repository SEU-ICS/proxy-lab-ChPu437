/*
  Proxy Lab
  TODOs:
    Part #1:
      [] implement a proxy to accept incoming connections
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

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

int main()
{
  printf("%s", user_agent_hdr);
  return 0;
}
