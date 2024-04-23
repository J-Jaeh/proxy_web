#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int parse_uri(char *uri, char *hostname, char *path, char *port);
void proxy_to_server(int server_fd);
void *thread(void *);

int main(int argc, char **argv)
{
  // printf("%s", user_agent_hdr);

  int listenfd, *connfdp, serverfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;
  rio_t rio;

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfdp = Malloc(sizeof(int));
    *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s) & proxy port : %s\n", hostname, port, argv[1]);
    Pthread_create(&tid, NULL, thread, connfdp);
  }
}

void proxy_to_server(int client_fd)
{
  int server_fd;
  rio_t rio;

  char client_buf[MAXLINE];
  char client_method[MAXLINE];
  char client_version[MAXLINE];
  char request[MAXLINE];

  /* request에 존재하는 parse_uri 파라미터 삼총사*/
  char server_IP[MAXLINE];
  char server_port[MAXLINE];
  char request_uri[MAXLINE];

  char request_buf[MAXLINE];
  char response_buf[MAXLINE];

  Rio_readinitb(&rio, client_fd);
  Rio_readlineb(&rio, client_buf, MAXLINE);

  sscanf(client_buf, "%s %s %s", client_method, request, client_version);

  // printf("client_buf %s\n", client_buf);
  // printf("request %s\n", request);

  if (strstr(request, "favicon"))
  {
    printf("Tiny couldn't find the favicon.");
    return;
  }

  if (parse_uri(request, server_IP, request_uri, server_port))
  {
    printf("Failed to parse URI\n");
    return;
  }

  printf("ip : %s \nport : %s \nrequest_uri : %s\n", server_IP, server_port, request_uri);

  server_fd = Open_clientfd(server_IP, server_port);

  snprintf(request_buf, MAXLINE, "%s %s HTTP/1.0\r\n:", client_method, request_uri);
  printf("request_buf : %s", request_buf);
  Rio_writen(server_fd, request_buf, strlen(request_buf));
  while (strcmp(request_buf, "\r\n"))
  {
    Rio_readlineb(&rio, request_buf, MAXLINE);
    Rio_writen(server_fd, request_buf, strlen(request_buf));
  }
  // 일단 여기가지 서버요청에 요청 쌉가능 이제 서버가 보내준걸 받기만하면됨!

  // 서버로부터 응답을 받아 클라이언트로 전달

  int n;
  while ((n = Rio_readn(server_fd, response_buf, MAXLINE)) > 0)
  {
    Rio_writen(client_fd, response_buf, n);
  }

  Close(server_fd);
}

int parse_uri(char *uri, char *hostname, char *path, char *port)
{
  printf("%s\n", uri);

  char *ptr = uri;

  if (*ptr == '/')
    ptr += 1; // 맨 앞 '/'가 있다면 건너뜀

  if (strncmp(ptr, "http://", 7) == 0)
    ptr += 7; // "http://" 길이만큼 포인터 이동

  // 초기 호스트네임과 패스 추출
  strcpy(hostname, ptr);

  // 패스 추출 (호스트네임 수정 전에 수행)
  char *path_ptr = strchr(hostname, '/');
  if (path_ptr)
  {
    strcpy(path, path_ptr); // 패스 추출
    *path_ptr = '\0';       // NULL 처리해줘서 path와 hostname 분리
  }
  else
    strcpy(path, "/"); // 패스 정보가 없으면 루트("/")로 설정

  // 포트 번호 추출
  char *port_ptr = strchr(hostname, ':');
  if (port_ptr)
  {
    strcpy(port, port_ptr + 1); // 포트 추출
    *port_ptr = '\0';           // NULL 처리해줘서 port와 hostname 분리
  }
  else
    strcpy(port, "80"); // 포트 정보가 없으면 기본값 "80"으로 설정

  return 0;
}

void *thread(void *vargp)
{
  int connfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  proxy_to_server(connfd);
  Close(connfd);
  return NULL;
}