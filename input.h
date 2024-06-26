#ifndef LABIIPROJECT_INPUT_H
#define LABIIPROJECT_INPUT_H

#include "structures.h"

void* thread_read(void *arg);
grafo read_input(const char *filename, int t, int *arcs_read);

#endif
