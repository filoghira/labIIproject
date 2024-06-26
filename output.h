#ifndef LABIIPROJECT_OUTPUT_H
#define LABIIPROJECT_OUTPUT_H

#include "structures.h"
#include <stdio.h>

void output_print_start(int n, int dead_end, int arcs);
void output_print_end(int n, double sum, int k, const map *pagerank, bool conv);

#endif
