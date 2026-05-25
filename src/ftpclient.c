#include "csapp.h"
#include "request.h"
#include "slave_addr.h"
#include <sys/time.h>

static int port = 2121;
#define DIR_CLIENT "dirClient/"
#define BLOCK_SIZE 512

int main(int argc, char **argv)
{
    int clientfd;
    FILE *fout;
    char *host, buf[MAXLINE];
    char filename[512];
    char cmd[MAXLINE], arg[MAXLINE];
    char prog_filename[512]; // resume file: dirClient/.filename.prog
    rio_t rio;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <host>\n", argv[0]);
        exit(0);
    }

    host = argv[1];

    signal(SIGPIPE, SIG_IGN); // don't die on broken connections

    clientfd = Open_clientfd(host, port);
    printf("client connected to server OS\n");

    // master sends the slave address we should talk to
    slave_addr_t slave_addr;
    Rio_readinitb(&rio, clientfd);
    Rio_readnb(&rio, &slave_addr, sizeof(slave_addr_t));
    Close(clientfd);

    if (slave_addr.port == 0) {
        printf("No slave available, exiting\n");
        exit(1);
    }

    clientfd = Open_clientfd(slave_addr.ip, slave_addr.port);
    printf("redirected to slave %s:%d\n", slave_addr.ip, slave_addr.port);

    Rio_readinitb(&rio, clientfd);

    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        buf[strcspn(buf, "\r\n")] = '\0';

        int parsed = sscanf(buf, "%s %s", cmd, arg);

        if (parsed >= 1 && strcmp(cmd, "bye") == 0) {
            request_t req;
            req.type = BYE;
            Rio_writen(clientfd, &req, sizeof(request_t));
            break;
        }

        if (parsed == 2) {
            request_t req;
            if (strcmp(cmd, "get") == 0 || strcmp(cmd, "GET") == 0) {

                snprintf(prog_filename, sizeof(prog_filename), "%s.%.255s.prog", DIR_CLIENT, arg);

                size_t offset = 0;
                FILE *fprog = fopen(prog_filename, "rb");
                if (fprog != NULL) {
                    fread(&offset, sizeof(size_t), 1, fprog);
                    fclose(fprog);
                    printf("Resuming from block %zu\n", offset);
                }

                req.type = GET;
                strncpy(req.nom, arg, 255);
                req.nom[255] = '\0';
                req.offset = offset;

                // lowercase rio_writen returns -1 on failure so we can reconnect
                if (rio_writen(clientfd, &req, sizeof(request_t)) < 0) {
                    printf("Slave down, reconnecting to master...\n");
                    Close(clientfd);

                    clientfd = Open_clientfd(host, port);
                    Rio_readinitb(&rio, clientfd);
                    Rio_readnb(&rio, &slave_addr, sizeof(slave_addr_t));
                    Close(clientfd);

                    if (slave_addr.port == 0) {
                        printf("No slave available, exiting\n");
                        exit(1);
                    }

                    clientfd = Open_clientfd(slave_addr.ip, slave_addr.port);
                    Rio_readinitb(&rio, clientfd);
                    printf("Connected to new slave %s:%d\n", slave_addr.ip, slave_addr.port);

                    if (rio_writen(clientfd, &req, sizeof(request_t)) < 0) {
                        printf("Reconnection failed\n");
                        break;
                    }
                }

                response_t res;
                if (Rio_readnb(&rio, &res, sizeof(response_t)) > 0) {
                    if (res.code == SUCCES) {
                        strcpy(filename, DIR_CLIENT);
                        strcat(filename, arg);

                        // append if resuming, otherwise start fresh
                        fout = (offset > 0) ? fopen(filename, "ab") : fopen(filename, "wb");

                        if (fout == NULL) {
                            fprintf(stderr, "Cannot open %s for writing\n", filename);
                        } else {
                            struct timeval start, end;
                            ssize_t n;
                            size_t total_bytes = 0;

                            gettimeofday(&start, NULL);

                            size_t nb_blocs;
                            Rio_readnb(&rio, &nb_blocs, sizeof(size_t));

                            size_t remaining = (offset < nb_blocs) ? (nb_blocs - offset) : 0;

                            char bloc[BLOCK_SIZE];

                            for (size_t i = 0; i < remaining; i++) {
                                n = Rio_readnb(&rio, bloc, BLOCK_SIZE);
                                fwrite(bloc, 1, n, fout);
                                total_bytes += n;

                                // save progress after each block in case of crash
                                size_t done = offset + i + 1;
                                FILE *fp_prog = fopen(prog_filename, "wb");
                                if (fp_prog != NULL) {
                                    fwrite(&done, sizeof(size_t), 1, fp_prog);
                                    fclose(fp_prog);
                                }
                            }

                            fclose(fout);
                            remove(prog_filename);

                            gettimeofday(&end, NULL);

                            double elapsed = (end.tv_sec - start.tv_sec) +
                                             (end.tv_usec - start.tv_usec) / 1000000.0;
                            double kbps = (elapsed > 0) ? (total_bytes / 1024.0) / elapsed : 0;

                            printf("Transfer successfully complete.\n");
                            printf("%zu bytes received in %.4f seconds (%.2f Kbytes/s).\n",
                                   total_bytes, elapsed, kbps);
                        }
                    } else {
                        printf("Server error: file not found or invalid command\n");
                    }
                } else {
                    printf("Connection error\n");
                }

            } else if (strcmp(cmd, "rm") == 0 || strcmp(cmd, "RM") == 0) {
                request_t req;
                memset(&req, 0, sizeof(request_t));
                req.type = RM;
                strncpy(req.nom, arg, 255);
                req.nom[255] = '\0';

                Rio_writen(clientfd, &req, sizeof(request_t));

                response_t res;
                if (Rio_readnb(&rio, &res, sizeof(response_t)) > 0) {
                    if (res.code == SUCCES)
                        printf("File %s deleted.\n", arg);
                    else
                        printf("Error: file not found on server.\n");
                }

            } else if (strcmp(cmd, "put") == 0 || strcmp(cmd, "PUT") == 0) {
                char src[512];
                strcpy(src, DIR_CLIENT);
                strcat(src, arg);

                FILE *fin = fopen(src, "rb");
                if (fin == NULL) {
                    printf("Local file not found: %s\n", src);
                } else {
                    request_t req;
                    memset(&req, 0, sizeof(request_t));
                    req.type = PUT;
                    strncpy(req.nom, arg, 255);
                    req.nom[255] = '\0';

                    Rio_writen(clientfd, &req, sizeof(request_t));

                    response_t res;
                    if (Rio_readnb(&rio, &res, sizeof(response_t)) > 0 && res.code == SUCCES) {
                        fseek(fin, 0, SEEK_END);
                        size_t file_size = ftell(fin);
                        fseek(fin, 0, SEEK_SET);
                        size_t nb_blocs = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

                        Rio_writen(clientfd, &nb_blocs, sizeof(size_t));

                        char bloc[BLOCK_SIZE];
                        size_t sent = 0;
                        while (sent < nb_blocs) {
                            size_t n = fread(bloc, 1, BLOCK_SIZE, fin);
                            if (n < BLOCK_SIZE) memset(bloc + n, 0, BLOCK_SIZE - n);
                            Rio_writen(clientfd, bloc, BLOCK_SIZE);
                            sent++;
                        }

                        printf("File %s uploaded.\n", arg);
                    } else {
                        printf("Error: server not ready to receive.\n");
                    }

                    fclose(fin);
                }

            } else {
                printf("Unknown command\n");
            }

        } else if (parsed == 1 && (strcmp(cmd, "ls") == 0 || strcmp(cmd, "LS") == 0)) {
            request_t req;
            memset(&req, 0, sizeof(request_t));
            req.type = LS;

            Rio_writen(clientfd, &req, sizeof(request_t));

            response_t res;
            if (Rio_readnb(&rio, &res, sizeof(response_t)) > 0 && res.code == SUCCES) {
                size_t n;
                char buf[4096];
                Rio_readnb(&rio, &n, sizeof(size_t));
                if (n > 0) {
                    Rio_readnb(&rio, buf, n);
                    buf[n] = '\0';
                    printf("%s", buf);
                }
            } else {
                printf("ls failed\n");
            }
        } else {
            printf("Usage: get <file> | put <file> | rm <file> | ls | bye\n");
        }
    }

    Close(clientfd);
    exit(0);
}
