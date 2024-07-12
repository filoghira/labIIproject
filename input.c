#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>
#include "utils.h"
#include "error.h"
#include "input.h"

#define BUFF_SIZE 1024
#define SECTIONS 100

struct timeval start, end, delta1;

// Lavoratore che elabora gli archi in input
void* thread_read(void *arg){
    // Ottengo i dati passati come argomento
    thread_data_read *data = arg;

    // Contatore per il numero di archi letti
    int count = 0;

    // Struttura per i nuovi archi
    struct inmap *new = NULL;

    // Struttura per i dati da leggere dal buffer
    // ReSharper disable once CppJoinDprint_graph(&g);eclarationAndAssignment
    struct node_read *curr;

    while(true){
        sem_wait(data->sem_buffer);

        // Aspetto che il buffer sia disponibile
        pthread_mutex_lock(data->m_buffer);
        // Leggo i dati dal buffer
        // ReSharper disable once CppJoinDeclarationAndAssignment
        curr = data->buffer;

        if (curr->end) {
            pthread_mutex_unlock(data->m_buffer);
            sem_post(data->sem_buffer);
            break;
        }
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
            pthread_mutex_lock(data->m_g[curr->j/SECTIONS]);
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
                    pthread_mutex_unlock(data->m_g[curr->j/SECTIONS]);
                    free(new);
                    continue;
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
            new->next = data->g->in[curr->j];
            data->g->in[curr->j] = new;
            // Aumento il numero di archi uscenti del nodo i
            data->g->out[new->node]++;

            pthread_mutex_unlock(data->m_g[curr->j/SECTIONS]);

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

    int n;

    gettimeofday(&start, NULL);
    // Leggo il file riga per riga
    while(getline(&line, &len, file) != -1){
        // Se la riga è un commento, la salto
        if(line[0] == '%') continue;

        // fprintf(stderr, "Line: %d\n", line_count);

        // Se è la prima riga
        if(!line_count) {
            // Numero di nodi e archi
            int r, c;
            sscanf(line, "%d %d %d", &r, &c, &n);

            // Controllo se la matrice è quadrata
            if(r != c)
                termina("La matrice non è quadrata");

            // Assegno il numero di nodi al grafo
            data->g->N = r;
            data->g->out = malloc(r*sizeof(int));
            data->g->in = malloc(r*sizeof(struct inmap*));

            sem_t *sem_buffer = malloc(sizeof(sem_t));
            sem_init(sem_buffer, 0, 0);

            data->sem_buffer = sem_buffer;

            int n_mutex = r / SECTIONS+1;
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
            new->next = data->buffer;
            new->end = false;
            data->buffer = new;
            pthread_mutex_unlock(&m_buffer);
            sem_post(data->sem_buffer);
            n--;
        }

        // Incremento il contatore delle righe
        line_count++;
    }

    // Chiudo il file
    fclose(file);

    fprintf(stderr, "Lettura terminata\n");

    pthread_mutex_lock(&m_buffer);
    struct node_read *new = malloc(sizeof(struct node_read));
    new->end = true;
    new->j = -1;
    new->i = -1;
    new->next = NULL;

    if (data->buffer == NULL) {
        data->buffer = new;
    } else {
        // Put the end node at the tail of the buffer
        struct node_read* current = data->buffer;
        while (current->next != NULL)
            current = current->next;
        current->next = new;
    }
    pthread_mutex_unlock(&m_buffer);
    sem_post(data->sem_buffer);

    // Aspetto che i thread terminino
    for(int i = 0; i < t; i++){
        pthread_join(threads[i], NULL);
    }

    // Distruzione dei semafori e dei mutex
    pthread_mutex_destroy(&m_buffer);
    sem_destroy(data->sem_buffer);

    // Libero la memoria
    free(line);
    free(data);

    gettimeofday(&end, NULL);
    timersub(&end, &start, &delta1);

    fprintf(stderr, "tempo lettura: %ld.%06ld\n", delta1.tv_sec, delta1.tv_usec);

    // Restituisco il grafo
    return g;
}