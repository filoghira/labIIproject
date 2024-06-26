#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include "error.h"

void termina(const char *messaggio)
{
    if(errno==0)
        fprintf(stderr,"%s\n",messaggio);
    else
        perror(messaggio);
    exit(1);
}
