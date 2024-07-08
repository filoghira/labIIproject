#define _GNU_SOURCE
#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>
#include "utils.h"
#include "error.h"
#include "input.h"

struct timeval start, end, delta1, delta2, delta3;

// Rimuove i duplicati
void* remove_duplicates(void *arg){
    thread_duplicates_removal *data = arg;
    struct node *curr = NULL;
    int removed = 0;

    while (true)
    {
        pthread_mutex_lock(data->m_count);
        if (data->count == data->n)
        {
            pthread_mutex_unlock(data->m_count);
            break;
        }
        data->count++;
        pthread_mutex_unlock(data->m_count);

        pthread_mutex_lock(data->m_buffer);
        curr = data->buffer;
        data->buffer = data->buffer->next;
        pthread_mutex_unlock(data->m_buffer);

        // Controllo se ci sono duplicati nella lista in posizione curr->index
        // Se la lista ha 0 elementi , non ci sono duplicati
        if (data->in[curr->index] == NULL) continue;

        struct inmap *current = data->in[curr->index]->next;

        if (current == NULL) continue;

        while (current->next != NULL)
        {
            if (current->node == current->next->node)
            {
                struct inmap* next = current->next->next;
                free(current->next);
                current->next = next;
                data->out[current->node]--;
                removed++;
            } else {
                current = current->next;
            }
        }
        // fprintf(stderr, "Thread %ld: %d removed: %d\n", pthread_self(), curr->index, removed);
        free(curr);
    }

    int *ret = malloc(sizeof(int));
    *ret = removed;
    pthread_exit(ret);
}

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

    struct inmap **in = malloc(data->g->N*sizeof(struct inmap));
    int *out = malloc(data->g->N*sizeof(int));

    for (int i = 0; i < data->g->N; i++)
    {
        in[i] = NULL;
        out[i] = 0;
    }

    while(true){

        pthread_mutex_lock(data->m_count);
        if(data->count == 0)
        {
            pthread_mutex_unlock(data->m_count);
            break;
        }
        data->count--;
        pthread_mutex_unlock(data->m_count);

        // Aspetto che il buffer sia disponibile
        pthread_mutex_lock(data->m_buffer);
        // Leggo i dati dal buffer
        // ReSharper disable once CppJoinDeclarationAndAssignment
        curr = data->buffer;
        // Rimuovo l'elemento dal buffer
        data->buffer = data->buffer->next;
        // Rilascio il buffer
        pthread_mutex_unlock(data->m_buffer);

        // Preparo il nuovo arco decrementando i valori di i e j di 1
        new = malloc(sizeof(struct inmap));
        new->node = curr->i - 1;
        new->next = NULL;
        curr->j--;

        // fprintf(stderr, "Thread %ld waiting for graph\n", pthread_self());

        if(!valid_arc(new->node, curr->j, data->g->N))
            termina("Identificatori dei nodi non validi");

        // Controllo se l'arco non è un ciclo
        if(new->node != curr->j){

            // Aggiungo il nuovo arco alla lista degli archi entranti in testa
            // new->next = data->g->in[curr->j];
            // data->g->in[curr->j] = new;
            if (in[curr->j] == NULL || in[curr->j]->node >= new->node) {
                new->next = in[curr->j];
                in[curr->j] = new;
            } else {
                struct inmap* current = in[curr->j];
                while (current->next != NULL && current->next->node < new->node)
                    current = current->next;
                new->next = current->next;
                current->next = new;
            }
            // Aumento il numero di archi uscenti del nodo i
            out[new->node]++;


            // Incremento il contatore degli archi letti
            count++;
        }else
        {
            // Se l'arco non è valido o è duplicato, lo elimino
            free(new);
        }

        // Libero la memoria
        free(curr);
    }

    // Restituisco il grafo temporaneo
    grafo *g = malloc(sizeof(grafo));
    g->in = in;
    g->out = out;
    pthread_exit(g);
}

void merge(struct inmap* inmap, struct inmap* inmap1)
{
    for (const struct inmap* current = inmap1; current != NULL; current = current->next)
    {
        struct inmap* new = malloc(sizeof(struct inmap));
        new->node = current->node;
        new->next = NULL;
// TODO
    }
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

    // Creo i mutex
    pthread_mutex_t m_buffer;
    pthread_mutex_init(&m_buffer, NULL);
    pthread_mutex_t m_count;
    pthread_mutex_init(&m_count, NULL);


    // Alloco memoria per la struct che conterrà i dati per i thread
    thread_data_read *data = malloc(sizeof(thread_data_read));

    // Inizializzo la struct
    data->buffer = NULL;
    data->m_buffer = &m_buffer;
    data->g = &g;
    data->count = 0;
    data->m_count = &m_count;

    // Creo l'array che conterrà i thread
    pthread_t threads[t];

    gettimeofday(&start, NULL);
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
            data->count = n;
        }else{
            // Leggo gli indici degli archi
            int i, j;
            sscanf(line, "%d %d", &i, &j);

            // Preparo la struct per i dati da leggere
            struct node_read *new = malloc(sizeof(struct node_read));
            new->i = i;
            new->j = j;
            new->next = NULL;

            new->next = data->buffer;
            // Aggiorno il buffer
            data->buffer = new;
        }

        // Incremento il contatore delle righe
        line_count++;
    }

    // Chiudo il file
    fclose(file);
    gettimeofday(&end, NULL);
    timersub(&end, &start, &delta3);

    fprintf(stderr, "Tempo caricamento file: %ld.%06ld\n", delta3.tv_sec, delta3.tv_usec);

    gettimeofday(&start, NULL);

    // Faccio partire i thread
    for (int i = 0; i < t; i++)
        pthread_create(&threads[i], NULL, thread_read, data);

    // Alloco memoria per gli array
    data->g->out = (int*)malloc(data->g->N*sizeof(int));
    data->g->in = (struct inmap**)malloc(data->g->N*sizeof(struct inmap));

    // Aspetto che i thread terminino
    for(int i = 0; i < t; i++){
        // Variabile per il valore di ritorno del thread (numero di archi letti)
        void *temp_g;
        pthread_join(threads[i], &temp_g);

        for (int j = 0; j < data->g->N; j++)
        {
            if (((grafo*)temp_g)->in[j] == NULL)
            {
                data->g->in[j] = NULL;
                data->g->out[j] = 0;
            }
            if (data->g->in[j] == NULL)
            {
                data->g->in[j] = ((grafo*)temp_g)->in[j];
                data->g->out[j] = ((grafo*)temp_g)->out[j];
                *arcs_read += ((grafo*)temp_g)->out[j];
                continue;
            }
            merge(g.in[j], ((grafo*)temp_g)->in[j]);
            data->g->out[j] += ((grafo*)temp_g)->out[j];
            *arcs_read += ((grafo*)temp_g)->out[j];
        }
    }

    // Distruzione dei semafori e dei mutex
    pthread_mutex_destroy(&m_buffer);

    // Libero la memoria
    free(line);
    free(data);

    gettimeofday(&end, NULL);
    timersub(&end, &start, &delta1);

    fprintf(stderr, "tempo lettura: %ld.%06ld\n", delta1.tv_sec, delta1.tv_usec);

    thread_duplicates_removal *data_dup = malloc(sizeof(thread_duplicates_removal));

    pthread_mutex_t m_buffer_dup;
    pthread_mutex_init(&m_buffer_dup, NULL);
    pthread_mutex_t m_count_dup;
    pthread_mutex_init(&m_count_dup, NULL);
    pthread_mutex_init(&m_count_dup, NULL);

    data_dup->buffer = NULL;
    data_dup->m_buffer = &m_buffer_dup;
    data_dup->in = g.in;
    data_dup->n = g.N;
    data_dup->out = g.out;
    data_dup->count = 0;
    data_dup->m_count = &m_count_dup;

    struct node *new = NULL;
    pthread_t threads_dup[t];

    gettimeofday(&start, NULL);
    for (int i = 0; i < g.N; i++)
    {
        new = malloc(sizeof(struct node));
        new->index = i;
        new->next = data_dup->buffer;
        data_dup->buffer = new;
    }

    for (int i = 0; i < t; i++)
        pthread_create(&threads_dup[i], NULL, remove_duplicates, data_dup);

    int removed = 0;

    void *temp;
    for (int i = 0; i < t; i++)
    {
        pthread_join(threads_dup[i], &temp);
        removed += *(int*)temp;
    }

    gettimeofday(&end, NULL);
    timersub(&end, &start, &delta2);

    fprintf(stderr, "tempo rimozione duplicati: %ld.%06ld\n", delta2.tv_sec, delta2.tv_usec);

    *arcs_read -= removed;

    pthread_mutex_destroy(&m_buffer_dup);
    free(data_dup);

    // Restituisco il grafo
    return g;
}