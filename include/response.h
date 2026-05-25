#ifndef _RESPONSE_
#define _RESPONSE_

typedef struct response response_t;

typedef enum{ SUCCES, ERREUR } codeRetour;

struct response
{
    codeRetour code;
};

void afficherResponse(response_t res);

#endif
