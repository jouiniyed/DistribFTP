#include "csapp.h"
#include "request.h"

#define NB_PROC 10
#define PORT_SLAVE 2122

static pid_t workers[NB_PROC];

void child_handler(int sig) {
    while (Waitpid(-1, NULL, WNOHANG) > 0);
}

void parent_stop_handler(int sig) {
    printf("\nSlave shutting down...\n");
    for (int i = 0; i < NB_PROC; i++)
        kill(workers[i], SIGTERM);
    while (wait(NULL) > 0);
    exit(0);
}

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;

    int port = (argc >= 2) ? atoi(argv[1]) : PORT_SLAVE;

    // remaining args are ip/port pairs for other slaves (for propagation)
    char peer_ips[10][64];
    int  peer_ports[10];
    int  j = 0;
    for (int i = 2; i + 1 < argc && j < 10; i += 2, j++) {
        strncpy(peer_ips[j], argv[i], 63);
        peer_ips[j][63] = '\0';
        peer_ports[j] = atoi(argv[i + 1]);
        printf("Will propagate to: %s:%d\n", peer_ips[j], peer_ports[j]);
    }
    set_slaves(peer_ips, peer_ports, j);

    Signal(SIGCHLD, child_handler);
    Signal(SIGINT, parent_stop_handler);

    clientlen = (socklen_t)sizeof(clientaddr);

    listenfd = Open_listenfd(port);
    printf("Slave listening on port %d\n", port);

    for (int i = 0; i < NB_PROC; i++) {
        pid_t p = Fork();
        if (p < 0) exit(-3);

        if (p == 0) {
            signal(SIGPIPE, SIG_IGN);

            while (1) {
                connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
                printf("slave: client connected\n");
                response_t res = requestHandler(connfd);
                afficherResponse(res);
                Close(connfd);
            }

            exit(0);
        }

        workers[i] = p;
    }

    while (1) pause();

    exit(0);
}
