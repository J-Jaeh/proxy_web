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

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int parse_uri(const char *request, char *server_ip, char *server_port, char *request_uri);
void proxy_to_server(int server_fd);

int main(int argc, char **argv)
{
  printf("%s", user_agent_hdr);

  int listenfd, connfd, serverfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  rio_t rio;

  // char serverIP[MAXLINE] = "13.125.79.202";
  // char serverPort[MAXLINE] = "4000";

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s) & proxy port : %s\n", hostname, port, argv[1]);

    // serverfd = Open_clientfd(serverIP, serverPort);
    proxy_to_server(connfd);

    Close(connfd);
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

  if (!parse_uri(request, server_IP, request_uri, server_port))
  {
    printf("Failed to parse URI\n");
    return;
  }

  // printf("%s %s %s\n", server_IP, server_port, request_uri);

  server_fd = Open_clientfd(server_IP, server_port);

  snprintf(request_buf, MAXLINE, "%s %s HTTP/1.0\r\n:", client_method, request_uri);
  Rio_writen(server_fd, request_buf, strlen(request_buf));
  while (strcmp(request_buf, "\r\n"))
  {
    Rio_readlineb(&rio, request_buf, MAXLINE);
    Rio_writen(server_fd, request_buf, strlen(request_buf));
  }
  // 일단 여기가지 서버요청에 요청 쌉가능 이제 서버가 보내준걸 받기만하면됨!

  // 서버로부터 응답을 받아 클라이언트로 전달
  Rio_readinitb(&rio, server_fd);
  while (Rio_readlineb(&rio, response_buf, MAXLINE) > 0)
  {
    Rio_writen(client_fd, response_buf, strlen(response_buf));
  }

  Close(server_fd);
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE];
  char body[MAXLINE];

  /*HTTP response body*/
  sprintf(body, "<html><head><link rel=\"shortcut icon\" href=\"#\"><title>Tiny Error</title></head></html>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em> The Tiny Web server</em>\r\n", body);

  /*print the HTTP response*/
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));

  sprintf(buf, "Content-type : text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));

  sprintf(buf, "Content-length : %d \r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));

  Rio_writen(fd, body, strlen(body));
}

int parse_uri(const char *request, char *server_ip, char *path, char *server_port)
{
  char *temp = strdup(request);

  // 첫 '/' 제거
  char *uri = temp + 1;

  // http:// 또는 https://를 찾는다.
  if (strncmp(uri, "http://", 7) == 0)
  {
    strcpy(server_port, "80");
    uri += 7; // "http://" 이후의 문자열로 이동
  }
  else if (strncmp(uri, "https://", 8) == 0)
  {
    strcpy(server_port, "443");
    uri += 8; // "https://" 이후의 문자열로 이동
  }

  // '/'를 기준으로 서버 주소와 경로를 분리한다.
  char *slash = strchr(uri, '/');
  if (slash != NULL)
  {
    *slash = '\0';           // 서버 주소와 경로를 분리하기 위해 '/'를 NULL로 변경
    strcpy(path, slash + 1); // 경로 복사
  }

  // ':'를 찾아 IP와 포트 번호를 분리한다.
  char *colon = strchr(uri, ':');
  if (colon != NULL)
  {
    *colon = '\0';                  // IP와 포트 번호를 분리하기 위해 ':'를 NULL로 변경
    strcpy(server_port, colon + 1); // 포트 번호 복사
  }

  strcpy(server_ip, uri); // 서버 IP 복사

  free(temp);
  return 1; // 파싱 성공
}
