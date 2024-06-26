#ifndef LABIIPROJECT_UTILS_H
#define LABIIPROJECT_UTILS_H

#include <stdbool.h>
#include "structures.h"

bool valid_arc(int i, int j, int N);
bool check_duplicate(struct inmap *in, int node);
void set_array(double *a, int n, double v);
int dead_end(const grafo *g);
int compare_desc(const void *a, const void *b);

#endif
