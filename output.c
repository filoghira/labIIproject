#include "output.h"

// Funzione che stampa i risultati iniziali dopo la lettura del file di input
void output_print_start(const int n, const int dead_end, const int arcs){
    printf("Number of nodes: %d\n", n);
    printf("Number of dead-ends nodes: %d\n", dead_end);
    printf("Number of valid arcs: %d\n", arcs);
}

// Funzione che stampa i risultati finali dopo il calcolo del pagerank
void output_print_end(const int n, const double sum, const int k, const map *pagerank, const bool conv){
    printf("Did ");
    if(!conv){
        printf("not ");
    }
    printf("converge after %d iterations\n", n);
    printf("Sum of ranks: %.4lf\n", sum);
    printf("Top %d nodes:\n", k);

    if (pagerank == NULL)
    {
        printf("No pagerank to print\n");
        return;
    }

    for(int i = 0; i < k; i++)
        printf("%d %lf\n", pagerank[i].index, pagerank[i].val);
}