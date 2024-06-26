#include <stdbool.h>
#include <stddef.h>
#include "utils.h"

// Funzione che controlla se un arco è valido
bool valid_arc(const int i, const int j, const int N){
    return i >= 0 && i < N && j >= 0 && j < N;
}

// Funzione che controlla se un nodo è già presente nella lista degli archi entranti
bool check_duplicate(struct inmap *in, const int node){
    const struct inmap *aux = in;
    // fprintf(stderr, "Thread %ld: Checking for duplicates in %p\n", pthread_self(), aux);
    while(aux != NULL){
        // fprintf(stderr, "Thread %ld: %p %p %d with node %d (to be compared with %d)\n", pthread_self(), aux, aux->next, NULL != aux, aux->node, node);
        if(aux->node == node){
            return true;
        }
        // fprintf(stderr, "Thread %ld: %p becames %p\n", pthread_self(), aux, aux->next);
        aux = aux->next;
    }
    // fprintf(stderr, "Thread %ld: No duplicates found\n", pthread_self());
    return false;
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