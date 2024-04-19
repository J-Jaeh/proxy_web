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
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int isAuthority(struct stat sbuf);

/*
 * argc = main 함수에 필요한 파라미터 갯수..
 * argv = [0] -> 프로그램 이름?
 *        [1] -> port 번호
 */
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

  listenfd = Open_listenfd(argv[1]); /*Open_listendfd -> open_listendfd->  포트번호 받고 listenfd 값 생성 반환*/
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit -> HTTP 트랜잭션처리
    Close(connfd); // line:netp:tiny:close
  }
}

void doit(int fd)
{
  int is_static;
  struct stat sbuf;  /* 파일의 상태정보 저장하는 구조체 */
  char buf[MAXLINE]; /* 클라에게 받은 HTTP 요청헤더를 저장하는 버퍼 */
  char method[MAXLINE];
  char uri[MAXLINE];
  char version[MAXLINE];

  char filename[MAXLINE]; /* 클라가 요청한 파일이름*/
  char cgiargs[MAXLINE];  /* 동적컨텐츠이ㅣ경우, CGI 스크립트에 전달된 인자를 저장*/

  rio_t rio; /*Robust I/O 패키지의 입출력 버퍼 구조체, 클라로부터 요청을 읽고,분석하고 응답을 보낼때 사용*/

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "method : %s, uri : %s, version : %s", method, uri, version);

  if (strcasecmp(method, "GET"))
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  /* URI 파싱 -> 정적 요청인지 아닌지 판단*/
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0)
  {
    clienterror(fd, filename, "403", "Not found", "Tiny couldn't read the file");
    return;
  }

  /*Serve static content*/
  if (is_static)
  {
    if (isAuthority(sbuf))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_mode);
  }
  else
  {
    if (isAuthority(sbuf))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}

int isAuthority(struct stat sbuf)
{
  if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    return 1;
  return 0;
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE];
  char body[MAXLINE];

  /*HTTP response body*/
  sprintf(body, "<html><title> Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em> The Tiny Web server</em?\r\n", body);

  /*print the HTTP response*/
  sprintf(buf, "HTTP/1.0 %s %s \r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));

  sprintf(buf, "Content-type : text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));

  sprinft(buf, "Content-lenght : %d \r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));

  Rio_writen(fd, body, strlen(body));
}
