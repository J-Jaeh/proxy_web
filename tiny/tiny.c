/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv)
{
  /*
   * listendfd : 리스닝 소켓의 파일 디스크립터를 저장.
   * connfd : 연결소켓의 파일 디스크립터를 저장. -> accept되었을시 새로운 소켓..을통해 클라이언트와 데이터 주고받음..
   * 파일디스크립터 : 열려있는 파일이나 소켓같은 자원에 대한 고유한 참조번호.
   *              운영체제가 열려있는 파일이나 소켓에 대해 고유한 정수 값을 할당. 이값을 사용해서 입출력작업 진행
   */
  int listenfd, connfd;

  /*
   * hostname : 연결된 클라이언트의 호스트 이름을 저장
   * port : 연결된 클라이언트의 port번호를 문자열 형태로 저장
   */
  char hostname[MAXLINE], port[MAXLINE];

  /*
   * clientlen : clientaddr 구조체의 크기를 저장, accpet()를 호출할때 이변수를 인자로 전달. 연결 요청을 보낸 클라의 주소 정보 크기를 알 수있다.
   */
  socklen_t clientlen;

  /*
   * clientaddr : 클라주소저장, sockaddr_storage 는 모든 유형의 소켓 주소를 저장할 수 있도록 크게 설계된 구조체, IPv4, IPv6 모두 지원.
   *              accept 함수에서 클라 주소를 받아오는데 사용
   */
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit
    Close(connfd); // line:netp:tiny:close
  }
}
