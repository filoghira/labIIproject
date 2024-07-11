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
        // Aspetto che il buffer sia disponibile
        pthread_mutex_lock(data->m_buffer);
        while (data->count == 0)
        {
            if (data->end)
            {
                pthread_mutex_unlock(data->m_buffer);
                pthread_exit(NULL);
            }
            pthread_cond_wait(data->full, data->m_buffer);
        }
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

            pthread_mutex_lock(data->m_g[3/100]);
            // Aggiungo il nuovo arco alla lista degli archi entranti in testa
            if (data->g->in[curr->j] == NULL || data->g->in[curr->j]->node >= new->node) {
                new->next = data->g->in[curr->j];
                data->g->in[curr->j] = new;
            } else {
                struct inmap* current = data->g->in[curr->j];
                struct inmap* prev = NULL;
                while (current != NULL && current->node < new->node) {
                    prev = current;
                    current = current->next;
                }
                if (current != NULL && current->node == new->node)
                {
                    free(new);
                    break;
                }

                if (prev != NULL)
                {
                    prev->next = new;
                    new->next = current;
                } else {
                    data->g->in[curr->j] = new;
                    new->next = current;
                }
            }
            // Aumento il numero di archi uscenti del nodo i
            data->g->out[new->node]++;

            pthread_mutex_unlock(data->m_g[3/100]);

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

    pthread_exit(NULL);
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

    // Alloco memoria per la struct che conterrà i dati per i thread
    thread_data_read *data = malloc(sizeof(thread_data_read));

    // Inizializzo la struct
    data->buffer = NULL;
    data->m_buffer = &m_buffer;
    data->g = &g;

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
            data->g->out = malloc(r*sizeof(int));
            data->g->in = malloc(r*sizeof(struct inmap*));

            pthread_cond_t empty, full;
            pthread_cond_init(&full, NULL);
            pthread_cond_init(&empty, NULL);

            data->full = &full;
            data->empty = &empty;

            data->count = 0;
            data->end = false;

            int n_mutex = r / 100+1;
            data->m_g = malloc(n_mutex*sizeof(pthread_mutex_t*));
            for (int i = 0; i < n_mutex; i++)
            {
                data->m_g[i] = malloc(sizeof(pthread_mutex_t));
                pthread_mutex_init(data->m_g[i], NULL);
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

            pthread_mutex_lock(&m_buffer);
            while (data->count >= 100)
                pthread_cond_wait(data->empty, &m_buffer);
            new->next = data->buffer;
            // Aggiorno il buffer
            data->buffer = new;
            pthread_cond_broadcast(data->full);
            pthread_mutex_unlock(&m_buffer);
        }

        // Incremento il contatore delle righe
        line_count++;
    }

    // Chiudo il file
    fclose(file);

    data->end = true;

    // Aspetto che i thread terminino
    for(int i = 0; i < t; i++){
        pthread_join(threads[i], NULL);
    }

    // Distruzione dei semafori e dei mutex
    pthread_mutex_destroy(&m_buffer);

    // Libero la memoria
    free(line);
    free(data);

    gettimeofday(&end, NULL);
    timersub(&end, &start, &delta1);

    fprintf(stderr, "tempo lettura: %ld.%06ld\n", delta1.tv_sec, delta1.tv_usec);

    // Restituisco il grafo
    return g;
}