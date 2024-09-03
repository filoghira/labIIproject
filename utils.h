#ifndef LABIIPROJECT_UTILS_H
#define LABIIPROJECT_UTILS_H

#include <stdbool.h>
#include <stdio.h>

#include "structures.h"

bool valid_arc(int i, int j, int N);
void set_array(double *a, int n, double v);
int dead_end(const grafo *g);
int compare_desc(const void *a, const void *b);
void print_graph(const grafo *g);
int custom_compare(const void *a, const void *b);
int count_lines(FILE* file);

#endif
