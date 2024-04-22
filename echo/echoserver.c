#include "csapp.h"

void echo(int);

void sigchld_handler(int sig)
{
    while (waitpid(-1, 0, WNOHANG) > 0)
        ;
    return;
}

int main(int argc, char **argv)
{
    int listendfd, connfd;
    socklen_t clientlen = sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    Signal(SIGCHLD, sigchld_handler);
    listendfd = Open_listenfd(atoi(argv[1]););
    while (1)
    {
        connfd = Accept(listendfd, (SA *)&clientaddr, &clientlen);
        if (Fork() == 0)
        {
            Close(listendfd); // -> 자식은 듣기 소켓 꺼도됨 !?
            echo(connfd);
            Close(connfd);
            exit(0);
        }
        Close(connfd);
    }
}
void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
    {
        printf("server received %d bytes\n", (int)n);
        Rio_writen(connfd, buf, n);
    }
}