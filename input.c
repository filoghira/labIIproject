#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include "utils.h"
#include "error.h"
#include "input.h"

// Lavoratore che elabora gli archi in input
void* thread_read(void *arg){
    // Ottengo i dati passati come argomento
    thread_data_read *data = arg;

    // Contatore per il numero di archi letti
    int count = 0;

    // Struttura per i nuovi archi
    struct inmap *new = NULL;
    // Struttura per i dati da leggere dal buffer
    // ReSharper disable once CppJoinDeclarationAndAssignment
    struct node_read *curr;

    while(true){
        // fprintf(stderr, "Thread %ld: Waiting for arcs\n", pthread_self());

        // Aspetto che ci siano archi da leggere
        sem_wait(data->arcs);

        // Controllo se devo terminare
        if(data->end) break;

        // Aspetto che il buffer sia disponibile
        sem_wait(data->mutex_buffer);
        // Leggo i dati dal buffer
        // ReSharper disable once CppJoinDeclarationAndAssignment
        curr = data->buffer;
        // Rimuovo l'elemento dal buffer
        data->buffer = data->buffer->next;
        // Rilascio il buffer
        sem_post(data->mutex_buffer);

        // Preparo il nuovo arco decrementando i valori di i e j di 1
        new = malloc(sizeof(struct inmap));
        new->node = curr->i - 1;
        new->next = NULL;
        curr->j--;

        // fprintf(stderr, "Thread %ld waiting for graph\n", pthread_self());

        if(!valid_arc(new->node, curr->j, data->g->N))
            termina("Identificatori dei nodi non validi");

        // Controllo se l'arco non è duplicato
        if(!check_duplicate(data->g->in[curr->j], new->node) && new->node != curr->j){

            // Aspetto che il grafo sia disponibile
            sem_wait(data->mutex_graph);
            // Se non è il primo arco
            if(data->g->in[curr->j] != NULL)
                // Aggiorno il nuovo arco
                new->next = data->g->in[curr->j];
            // Aggiungo il nuovo arco alla lista degli archi entranti
            data->g->in[curr->j] = new;

            // Aumento il numero di archi uscenti del nodo i
            data->g->out[new->node]++;

            // Rilascio il grafo
            sem_post(data->mutex_graph);

            // Incremento il contatore degli archi letti
            count++;

            // fprintf(stderr, "Thread %ld: count %d\n", pthread_self(), count);
        }else
        {
            // Se l'arco non è valido o è duplicato, lo elimino
            free(new);
        }

        // Libero la memoria
        free(curr);
    }

    // Restituisco il numero di archi letti
    int *ret = malloc(sizeof(int));
    *ret = count;
    pthread_exit(ret);
}

// Funzione che legge il file di input e crea il grafo
grafo read_input(const char *filename, const int t, int *arcs_read){

    // fprintf(stderr, "Reading input file\n");

    // Alloco memoria per il grafo
    grafo g;

    // Apro il file in lettura
    FILE *file = fopen(filename, "r");

    // Controllo se il file è stato aperto correttamente
    if(file == NULL)
        termina("Errore nell'apertura del file di input");

    // Variabile per leggere la riga
    char *line = NULL;
    // La lunghezza della riga non è nota, quindi inizializzo a 0
    size_t len = 0;
    // Variabile per contare le righe lette (escuso i commenti)
    int line_count = 0;

    // Creo i semafori
    sem_t arcs;
    sem_t mutex_buffer;
    sem_t mutex_graph;
    sem_t mutex_arcs;

    // Inizializzo i semafori
    sem_init(&arcs, 0, 0);
    sem_init(&mutex_buffer, 0, 1);
    sem_init(&mutex_graph, 0, 1);
    sem_init(&mutex_arcs, 0, 1);

    // Alloco memoria per la struct che conterrà i dati per i thread
    thread_data_read *data = malloc(sizeof(thread_data_read));

    // Inizializzo la struct
    data->buffer = NULL;
    data->total_arcs = 0;
    data->arcs = &arcs;
    data->mutex_buffer = &mutex_buffer;
    data->mutex_graph = &mutex_graph;
    data->mutex_arcs = &mutex_arcs;
    data->g = &g;
    data->end = false;

    // Creo l'array che conterrà i thread
    pthread_t threads[t];

    // Leggo il file riga per riga
    while(getline(&line, &len, file) != -1){
        // Se la riga è un commento, la salto
        if(line[0] == '%') continue;

        // fprintf(stderr, "Line: %d\n", line_count);

        // Se è la prima riga
        if(!line_count) {
            // Numero di nodi e archi
            int r, c, n;
            sscanf(line, "%d %d %d", &r, &c, &n);

            // Controllo se la matrice è quadrata
            if(r != c)
                termina("La matrice non è quadrata");

            // Assegno il numero di nodi al grafo
            data->g->N = r;

            // Imposto il numero massimo di archi
            data->max_arcs = n;

            // Alloco memoria per gli array
            data->g->out = (int*)malloc(data->g->N*sizeof(int));
            data->g->in = (struct inmap**)malloc(data->g->N*sizeof(struct inmap));

            // Inizializzo gli array
            for(int i = 0; i < data->g->N; i++){
                data->g->out[i] = 0;
                data->g->in[i] = NULL;
            }

            // Faccio partire i thread
            for (int i = 0; i < t; i++)
                pthread_create(&threads[i], NULL, thread_read, data);
        }else{
            // Leggo gli indici degli archi
            int i, j;
            sscanf(line, "%d %d", &i, &j);

            // Preparo la struct per i dati da leggere
            struct node_read *new = malloc(sizeof(struct node_read));
            new->i = i;
            new->j = j;
            new->next = NULL;

            // Aspetto che il buffer sia disponibile
            sem_wait(&mutex_buffer);

            // Se il buffer non è vuoto
            if (data->buffer != NULL)
                // Aggiorno il nuovo nodo
                new->next = data->buffer;
            // Aggiorno il buffer
            data->buffer = new;
            // Rilascio il buffer
            sem_post(&mutex_buffer);

            // Incremento il numero di archi da leggere
            sem_post(&arcs);
        }

        // Incremento il contatore delle righe
        line_count++;
    }

    // Chiudo il file
    fclose(file);

    // Aspetto che tutti gli archi siano stati letti
    while (data->buffer != NULL) {}
    // Avviso i thread che non ci sono più archi da leggere
    data->end = true;

    // Sblocco i thread
    for (int i = 0; i < t; i++)
        sem_post(data->arcs);

    // Aspetto che i thread terminino
    for(int i = 0; i < t; i++){
        // Variabile per il valore di ritorno del thread (numero di archi letti)
        void *temp;
        pthread_join(threads[i], &temp);
        *arcs_read += *(int*) temp;
        free(temp);
    }

    // Distruzione dei semafori
    sem_destroy(&arcs);
    sem_destroy(&mutex_buffer);
    sem_destroy(&mutex_graph);
    sem_destroy(&mutex_arcs);

    // Libero la memoria
    free(line);
    free(data);

    // Restituisco il grafo
    return g;
}