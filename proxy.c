#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

// 캐시는 쓸때 혼자만 쓰기 기능하게 해야함,
// 그리고 쓸때 만약 쓰기 현재 캐시사이즈 + 써야할 캐시 사이즈가 맥스 사이즈를 넘어가면..
// 들어있는 캐시 삭제, 그리고 위에 반복 체크
typedef struct cache_node
{
  struct cache_node *next;
  char uri[MAXLINE];
  char cache_buff[MAX_OBJECT_SIZE];
  int count;
  int len;
} cache_node;

int current_cache_size = 0;
struct cache_node *root;
volatile int lock = 0;

// -- 함수들 -- //
int parse_uri(char *uri, char *hostname, char *path, char *port);
void proxy_to_server(int server_fd);
void *thread(void *);
cache_node *init_list();
int add_first_node(struct cache_node *head, char *uri, char *cache_buff, int len);
int node_len(struct cache_node *head);
cache_node *find_node_uri(struct cache_node *head, char *find_uri);
int delete_node(struct cache_node *head, int index);
int find_delete_node_index(cache_node *head);

int main(int argc, char **argv)
{
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
  root = init_list();

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfdp = Malloc(sizeof(int));
    *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
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
  char response_buf[MAX_CACHE_SIZE];

  /* 캐시 */
  int temp_cache_size = 0;
  cache_node *cache;

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

  if ((cache = find_node_uri(root, request_uri)) != NULL) // 캐시 히트인경우
  {
    printf("\n");
    printf("캐시히트 : %s\n", cache->uri);
    printf("캐시히트 횟수: %d\n", cache->count);
    printf("캐시버퍼 사이즈: %d\n", cache->len);
    printf("current_cache_size = %d\n", current_cache_size);
    printf("\n");

    Rio_writen(client_fd, cache->cache_buff, cache->len);
    cache->count = (cache->count) + 1;
    return;
  }

  server_fd = Open_clientfd(server_IP, server_port);

  snprintf(request_buf, MAXLINE, "%s %s HTTP/1.0\r\n:", client_method, request_uri);
  printf("request_line : %s \n\n", request_buf);
  Rio_writen(server_fd, request_buf, strlen(request_buf));
  while (strcmp(request_buf, "\r\n"))
  {
    Rio_readlineb(&rio, request_buf, MAXLINE);
    Rio_writen(server_fd, request_buf, strlen(request_buf));
  }

  // --- 캐시 미스 날 경우 서버로 요청 + spin lock 걸어서 한명만 쓸수있게--- //
  int n;
  int total_bytes = 0;
  while ((n = Rio_readn(server_fd, response_buf, MAX_CACHE_SIZE)) > 0)
  {
    Rio_writen(client_fd, response_buf, n);
    temp_cache_size += n;
    total_bytes += n;
  }
  printf("total_bytes: %d\n", total_bytes);

  // 스핀락을 걸어서 쓰기작업 쓰레드 하나만
  while (__sync_lock_test_and_set(&lock, 1) == 1)
    ;
  if (current_cache_size + temp_cache_size <= MAX_OBJECT_SIZE)
  {
    current_cache_size += add_first_node(root, request_uri, response_buf, total_bytes);
  }
  else if (temp_cache_size <= MAX_OBJECT_SIZE)
  {
    do
    {
      int target = find_delete_node_index(root);
      int del_size = delete_node(root, target);
      current_cache_size -= del_size;
    } while (current_cache_size + temp_cache_size > MAX_OBJECT_SIZE);
  }
  // 잠금 풀기
  lock = 0;
  printf("current_cache_size = %d\n", current_cache_size);

  Close(server_fd);
}

int parse_uri(char *uri, char *hostname, char *path, char *port)
{
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

// 캐시
struct cache_node *init_list()
{
  struct cache_node *head;
  head = (struct cache_node *)malloc(sizeof(struct cache_node));
  head->next = NULL;
  return head;
}

// 새로운 캐시를 넣는다.
int add_first_node(struct cache_node *head, char *uri, char *cache_buff, int len)
{
  printf("\n캐시 저장 uri key : %s\n", uri);
  struct cache_node *newNode = (struct cache_node *)malloc(sizeof(struct cache_node));
  newNode->next = head->next;
  strcpy(newNode->uri, uri);
  memcpy(newNode->cache_buff, cache_buff, MAX_OBJECT_SIZE);
  newNode->count = 0;
  newNode->len = len;

  head->next = newNode;
  return len;
}

int delete_node(cache_node *head, int index)
{
  int list_len = node_len(head);
  if ((head)->next == NULL)
    return NULL;
  else if (index < 0 || index > list_len)
    return NULL;

  if (index == 0)
  {
    struct cache_node *temp = head;
    int size = strlen(head->next->cache_buff);
    head = head->next;
    free(temp);
    return size;
  }

  struct cache_node *pre = head;

  int i = 0;
  while (i < index - 1)
  {
    i++;
    pre = pre->next;
  }
  struct cache_node *target_node = pre->next;

  if (target_node == NULL)
    return NULL;

  pre->next = target_node->next;
  target_node->next = NULL;
  int size = strlen(target_node->cache_buff);
  free(target_node);
  return size;
}

int node_len(cache_node *head)
{
  int count = 0;

  cache_node *curent;
  curent = head;

  while (curent->next != NULL)
  {
    count++;
    curent = curent->next;
  }

  return count;
}

int find_delete_node_index(cache_node *head)
{
  if (head->next == NULL)
    return;

  int min = head->next->count;
  int c = 0;
  int index = 0;

  cache_node *curent = head;
  while (curent->next != NULL)
  {
    if (curent->count < min)
    {
      index = c;
      min = curent->count;
      curent = curent->next;
    }
    c++;
  }
  return index;
}

cache_node *find_node_uri(cache_node *head, char *find_uri)
{
  cache_node *current = head;

  while (current != NULL && strcmp(current->uri, find_uri) != 0)
  {
    current = current->next;
  }

  if (current != NULL && strcmp(current->uri, find_uri) == 0)
  {
    return current;
  }
  else
  {
    printf("캐시 미스\n");
    return NULL;
  }
}
