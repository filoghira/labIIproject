#include <stdbool.h>
#include <stddef.h>
#include "utils.h"

#include <stdio.h>

#define BUF_SIZE 65536

// Funzione che controlla se un arco è valido
bool valid_arc(const int i, const int j, const int N){
    return i >= 0 && i < N && j >= 0 && j < N ;
}

// Funzione che inizializza un array di double con un valore
void set_array(double *a, const int n, const double v){
    if (a == NULL)
        return;
    for(int i = 0; i < n; i++){
        a[i] = v;
    }
}

// Funzione che conta i nodi senza archi uscenti
int dead_end(const grafo *g){
    int count = 0;
    for(int i = 0; i < g->N; i++)
        if(g->out[i] == 0)
            count++;
    return count;
}

// Funzione di confronto per l'ordinamento del vettore dei pagerank
int compare_desc(const void *a, const void *b) {
    const map arg1 = *(const map *)a;
    const map arg2 = *(const map *)b;

    if (arg1.val < arg2.val) return 1;
    if (arg1.val > arg2.val) return -1;
    return 0;
}

void print_graph(const grafo *g){
    fprintf(stderr, "Matrice di adiacenza:\n");
    for(int i = 0; i < g->N; i++){
        fprintf(stderr, "Node %d: ", i);
        for(int j = 0; j < g->in->size[i]; j++){
            fprintf(stderr,"%d ", g->in->list[i][j]);
        }
        fprintf(stderr, "\n");
    }

    fprintf(stderr, "Nodi uscenti:\n");
    for(int i = 0; i < g->N; i++){
        fprintf(stderr, "Node %d: %d\n", i, g->out[i]);
    }
}

int custom_compare(const void *a, const void *b)
{
    return (*(int *)a - *(int *)b);
}

int count_lines(FILE* file)
{
    int counter = 0;
    for(;;)
    {
        char buf[BUF_SIZE];
        const size_t res = fread(buf, 1, BUF_SIZE, file);
        if (ferror(file))
            return -1;

        for(int i = 0; i < res; i++)
            if (buf[i] == '\n')
                counter++;

        if (feof(file))
            break;
    }

    return counter;
}