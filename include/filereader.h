#ifndef _FILE_READER_
#define _FILE_READER_

#include "response.h"
#include "csapp.h"

response_t filereader(int connfd, char fichier[256], size_t offset);
response_t filels(int connfd);
response_t filerm(int connfd, char fichier[256]);
response_t fileput(int connfd, char fichier[256], rio_t *rio);

#endif
