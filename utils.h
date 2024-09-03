#ifndef LABIIPROJECT_UTILS_H
#define LABIIPROJECT_UTILS_H

#include <stdbool.h>
#include <stdio.h>

#include "structures.h"

void set_array(double *a, int n, double v);
int dead_end(const grafo *g);
int compare_desc(const void *a, const void *b);
int custom_compare(const void *a, const void *b);

#endif
