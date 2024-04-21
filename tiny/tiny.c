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
void serve_static(int fd, char *filename, int filesize, int flag);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, int flag);
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

  listenfd = Open_listenfd(argv[1]); /*Open_listendfd -> open_listendfd->  포트번호 받고 listenfd 값 생성 반환 - 서버의 프로세스 포트번호!*/
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s) & server port : %s\n", hostname, port, argv[1]);
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
  /*여기는 아직 CGI사용안하니 stdout해도 콘솔print */
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);

  if ((strcasecmp(method, "GET")) && strcasecmp(method, "HEAD"))
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  read_requesthdrs(&rio);

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
    int flag = 0;
    if (strcasecmp(method, "GET"))
      flag = 1;
    serve_static(fd, filename, sbuf.st_size, flag);
  }
  else
  {
    if (isAuthority(sbuf))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    int flag = 0;
    if (strcasecmp(method, "GET"))
      flag = 1;
    serve_dynamic(fd, filename, cgiargs, flag);
    return;
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

/*요청헤더를 읽기는하지만 아직은 사용x*/
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin"))
  {
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri) - 1] == '/')
      strcat(filename, "home.html");
    return 1;
  }
  else
  {
    ptr = index(uri, '?');
    if (ptr)
    {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize, int head_flag)
{
  int srcfd;
  char *srcp;
  char filetype[MAXLINE];
  char buf[MAXBUF];

  /*Send response headers to client*/
  get_filetype(filename, filetype);
  /* strcat*/
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer : Tiny Web Server-static\r\n", buf);
  sprintf(buf, "%sConnection : close\r\n", buf);
  sprintf(buf, "%sContent-length : %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type : %s\r\n\r\n", buf, filetype);

  Rio_writen(fd, buf, strlen(buf));
  if (head_flag)
    return;
  printf("Response headers:\n");
  printf("%s", buf);

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);

  /* *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
   *
   * *addr : 매핑할 메모리의 시작주소, 0->NULL 시스템이 적절한 주소 자동으로 선택
   * length : 매핑할 파일크기
   * prot : 매핑된 메모리에 대한 보호 수준 설정 . PROT_READ - 읽기전용 / PROT_WRITE(쓰기가능) / PROT_EXEC(실행 가능)/PROT_NONE(접근불가)
   * flags : 매핑 특성과 동작방식, MAP_PRIVATE - 쓰기 시 복사(copy on wirte)/ MAP_SHARED(변경사항 파일반영됨, 다른프로세스 공유됨)
   * fd : 파일 디스크립터
   * offset : 파일 내에서 매핑을 시작할곳, 0은 파일 시작부터 매핑을 시작함.
   */
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  srcp = (char *)malloc(filesize * sizeof(char));

  Rio_readn(srcfd, srcp, filesize);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);

  /* munmap(void *addr, size_t length);
   * *addr : 해제하려는 메모리 매핑 시작주소
   * length : 해제하려는 메모리 매핑 크기
   */
  // Munmap(srcp, filesize);
  free(srcp);
}

/*
 * get_filetype  - Derive file type from filename
 */
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "image/mp4");
  else
    strcpy(filetype, "text/plain");
}
void sigchld_handler(int signum)
{
  pid_t pid;
  int status;

  while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    printf("자식 프로세스 죽음 %d 종료\n", pid);
}

void serve_dynamic(int fd, char *filename, char *cgiargs, int head_flag)
{
  char buf[MAXLINE];
  char *emptylist[] = {NULL};
  pid_t pid;
  signal(SIGCHLD, sigchld_handler);

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server : Tiny Web Server-dynamic\r\n");
  Rio_writen(fd, buf, strlen(buf));

  /*
   * 슬립을 준다고해서 상태라인만 받았다고해서 바로 표시해주는게 아님, 상태라인 + http헤더 + http바디를 완성해야 로딩해줌!
   */
  // sleep(5);

  if (Fork() == 0) // Fork 하면 자식 pid 리턴한다 ~
  {
    /* Child*/
    /* Real server would set all CGI vars here */
    char method[5] = "GET";
    if (!head_flag)
      setenv("REQUEST_METHOD", method, 1);
    else
    {
      strcpy(method, "HEAD");
      setenv("REQUEST_METHOD", method, 1);
    }
    setenv("QUERY_STRING", cgiargs, 1); /*cgiargs에는 1&2가 저장 */
    Dup2(fd, STDOUT_FILENO);            /* Redirect stdout to client  그니까 stdout에 buffer에 입력되는게 -> fd로 리다이렉트?*/

    /* Run CGI program
     * filename : 실행파일 이름 여기서는 adder<= http://15.165.237.222:4000/cgi-bin/adder?1&2
     * emptylist : 실행파일에 전달되는 argument를 지정... 현재는 아무것도 전달안함
     * eniron : ? 새로운 환경변수 목록일 지정.. 새로 실행되는 프로그램이 현재 프로세스와 동일한 환경 설정을 상속받도록하기 위해 ..?
     */
    Execve(filename, emptylist, environ);
  }
}