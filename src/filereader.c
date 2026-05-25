#include "csapp.h"
#include "filereader.h"
#include <dirent.h>
#include <unistd.h>

#define DIR_SERVER "dirServer/"
#define BLOCK_SIZE 512

response_t filels(int connfd) {
    response_t res;
    char buf[4096];
    int len = 0;

    DIR *d = opendir(DIR_SERVER);
    if (d == NULL) {
        res.code = ERREUR;
        rio_writen(connfd, &res, sizeof(res));
        return res;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        int n = snprintf(buf + len, sizeof(buf) - len, "%s\n", entry->d_name);
        if (n > 0) len += n;
    }
    closedir(d);

    res.code = SUCCES;
    rio_writen(connfd, &res, sizeof(res));
    size_t total = (size_t)len;
    rio_writen(connfd, &total, sizeof(size_t));
    if (total > 0)
        rio_writen(connfd, buf, total);

    return res;
}

response_t filereader(int connfd, char fichier[256], size_t offset)
{
    int fd;
    response_t res;
    char path[512];

    strcpy(path, DIR_SERVER);
    strcat(path, fichier);
    printf("file: %s\n", path);

    if ((fd = open(path, O_RDONLY)) < 0) {
        res.code = ERREUR;
        Rio_writen(connfd, &res, sizeof(res));
        return res;
    }

    res.code = SUCCES;
    Rio_writen(connfd, &res, sizeof(res));

    size_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    size_t nb_blocs = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    Rio_writen(connfd, &nb_blocs, sizeof(size_t));

    if (offset > 0 && offset < nb_blocs)
        lseek(fd, (off_t)(offset * BLOCK_SIZE), SEEK_SET);

    size_t blocs_a_envoyer = (offset < nb_blocs) ? (nb_blocs - offset) : 0;
    size_t envoyes = 0;

    char buf[BLOCK_SIZE];
    ssize_t n;

    while (envoyes < blocs_a_envoyer && (n = Rio_readn(fd, buf, BLOCK_SIZE)) > 0) {
        if (n < BLOCK_SIZE)
            memset(buf + n, 0, BLOCK_SIZE - n);
        usleep(10000); // 10ms per block — slow enough to interrupt for crash-recovery testing
        // lowercase rio_writen returns -1 on broken connection instead of exiting
        if (rio_writen(connfd, buf, BLOCK_SIZE) < 0) {
            close(fd);
            res.code = ERREUR;
            return res;
        }
        envoyes++;
    }

    close(fd);
    return res;
}

response_t filerm(int connfd, char fichier[256]) {
    response_t res;
    char path[512];

    strcpy(path, DIR_SERVER);
    strcat(path, fichier);

    res.code = (unlink(path) < 0) ? ERREUR : SUCCES;

    Rio_writen(connfd, &res, sizeof(res));
    return res;
}

response_t fileput(int connfd, char fichier[256], rio_t *rio) {
    response_t res;
    char path[512];

    strcpy(path, DIR_SERVER);
    strcat(path, fichier);

    res.code = SUCCES;
    Rio_writen(connfd, &res, sizeof(res));

    size_t nb_blocs;
    Rio_readnb(rio, &nb_blocs, sizeof(size_t));

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        res.code = ERREUR;
        return res;
    }

    char buf[BLOCK_SIZE];
    ssize_t n;
    for (size_t i = 0; i < nb_blocs; i++) {
        n = Rio_readnb(rio, buf, BLOCK_SIZE);
        Write(fd, buf, n);
    }

    close(fd);
    res.code = SUCCES;
    return res;
}
