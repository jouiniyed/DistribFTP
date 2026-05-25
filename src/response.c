#include "response.h"
#include <stdio.h>

void afficherResponse(response_t res){
    switch(res.code){
        case SUCCES:
            fprintf(stdout, "Request Successfull\n");
            break;
        case ERREUR:
            fprintf(stderr, "Request Unsuccessfull\n");
            break;
        default:
            fprintf(stderr, "Type unknown\n");
            break;
    }
}