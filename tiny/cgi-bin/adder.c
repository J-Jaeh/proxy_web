/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void)
{
  char *buf; /*쿼리스트링 문자를 담을 버퍼*/
  char *p;
  char arg1[MAXLINE];
  char arg2[MAXLINE];
  char content[MAXLINE]; /* HTTP response body*/

  int n1 = 0, n2 = 0;

  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) != NULL) /* serve_dynamic 함수에서 set_env에서 지정한 key값? 으로 가져옴 약간 map 구조인듯?*/
  {
    p = strchr(buf, '&');  /* strchr-> pram 1 에 있는 문자열에서 pram2를 찾는 함수 */
    *p = '\0';             /* 위에서 찾은 & 문자를 NULL 처리 -> 문자열의 끝 */
    strcpy(arg1, buf + 2); /* buf 값을 arg1에 복사*/
    strcpy(arg2, p + 3);   /* 두번째 인자값을 arg2에 복사(p는 &위치를 가리키고있었으니까+1)*/
    n1 = atoi(arg1);       /* atoi 는 문자열을 숫자로 변경하는것*/
    n2 = atoi(arg2);
  }

  /* Make the response body */
  /* sprintf 는 문자열을 형식에 맞게 생성하는 함수 , content에 저장*/

  sprintf(content, "%sWelcom to add.com: ", content);
  sprintf(content, "%sTHE Internet addition portal. \r\n<p>", content);
  sprintf(content, "%sThe answer is : %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* Generate the HTTP response */
  /* CGI의경우 stdout의 버퍼를 사용자의 웹브라우저로 보내는 역할을!*/
  /* 그런데 CGI를 이용해 웹브라우저로 보낼때는 상태라인과 헤더를 포함해야함 ~ .상태는 어디 포함되는거 ? 함수 호출전에 처리한듯 */
  printf("Connection : close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type:  text/html\r\n\r\n"); /* \r\n\r\n 헤더와 본문사이를 구분하기위한 약속*/
  char *str = getenv("REQUEST_METHOD");
  // printf("pointer : %s", str);
  if (!strcasecmp(str, "GET"))
    printf("%s", content);
  fflush(stdout); /* stdout 버퍼 비우기*/

  // exit(0);
}
/* $end adder */
