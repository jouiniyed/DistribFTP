#include "csapp.h"
#include "request.h"
#include "slave_addr.h"

#define MAX_NAME_LEN 256
#define NB_PROC 10
#define PORT_MASTER 2121
#define PORT_WORKER 2122

static pid_t workers[NB_PROC];

void child_handler(int sig) {
    while (Waitpid(-1, NULL, WNOHANG) > 0);
}

void parent_stop_handler(int sig) {
    printf("\nServer shutting down...\n");
    for (int i = 0; i < NB_PROC; i++)
        kill(workers[i], SIGTERM);
    while (wait(NULL) > 0);
    exit(0);
}

int main(int argc, char **argv)
{
    socklen_t clientlen;
    struct sockaddr_in clientaddr;

    Signal(SIGCHLD, child_handler);
    Signal(SIGINT, parent_stop_handler);

    clientlen = (socklen_t)sizeof(clientaddr);

    // workers handle the actual requests on PORT_WORKER
    int workerfd = Open_listenfd(PORT_WORKER);

    for (int i = 0; i < NB_PROC; i++) {
        pid_t p = Fork();
        if (p < 0) exit(-3);

        if (p == 0) {
            signal(SIGPIPE, SIG_IGN);

            while (1) {
                int connfd = Accept(workerfd, (SA *)&clientaddr, &clientlen);
                printf("server: client connected\n");
                response_t res = requestHandler(connfd);
                afficherResponse(res);
                Close(connfd);
            }

            exit(0);
        }

        workers[i] = p;
    }

    // parent acts as master: redirect every client to PORT_WORKER on localhost
    int masterfd = Open_listenfd(PORT_MASTER);
    printf("ftpserveri listening on port %d (redirects to %d)\n", PORT_MASTER, PORT_WORKER);

    slave_addr_t addr;
    strncpy(addr.ip, "127.0.0.1", sizeof(addr.ip));
    addr.port = PORT_WORKER;

    while (1) {
        int connfd = Accept(masterfd, (SA *)&clientaddr, &clientlen);
        Rio_writen(connfd, &addr, sizeof(slave_addr_t));
        Close(connfd);
    }

    exit(0);
}
