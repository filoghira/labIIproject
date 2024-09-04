#include <stddef.h>
#include "utils.h"

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

// Funzione di confronto per l'ordinamento del vettore degli int
int custom_compare(const void *a, const void *b)
{
    return (*(int *)a - *(int *)b);
}