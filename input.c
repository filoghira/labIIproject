#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>
#include "utils.h"
#include "input.h"

#define INCREMENT 20

struct timeval start1, end1, delta_load, delta_elaborate, delta_duplicates;

// Funzione per i thread che leggono il file del grafo
void* thread_read(void *arg){
    // Argomenti passati al thread
    thread_data_read *args = arg;

    // Variabile per contare i nodi corretti effettivamente letti
    int nodes = 0;

    // Creo il grafo
    grafo *g = malloc(sizeof(grafo));
    g->N = args->N;
    g->in = (struct inmap *)malloc(sizeof(struct inmap));
    g->out = (int *)calloc(g->N, sizeof(int));
    g->in->list = (int **)calloc(args->N, sizeof(int*));
    g->in->size = (int *)calloc(args->N, sizeof(int));

    void** batch = NULL;
    int i = -1;
    int j = -1;

    while(true){
        // Aspetto che ci sia un elemento nel buffer
        sem_wait(args->sem_full);
        // Lock del buffer
        pthread_mutex_lock(args->m_buffer);

        // Se non ci sono più elementi da leggere e la fine è segnalata
        if(args->end && args->read_batches == args->total_batches){
            // Rilascio il lock sul buffer
            pthread_mutex_unlock(args->m_buffer);
            // Faccio partire un altro thread
            sem_post(args->sem_full);
            // Termino
            break;
        }

        // Prelevo il batch di archi dal buffer
        batch = args->buffer[args->out];
        // Aggiorno la posizione di lettura
        args->out = (args->out + 1) % args->buffer_size;

        args->read_batches++;

        // Segnalazione che c'è uno spazio vuoto nel buffer
        sem_post(args->sem_empty);
        // Rilascio il lock sul buffer
        pthread_mutex_unlock(args->m_buffer);

        // Scorro il batch di archi
        for(int k=0; k<args->batch_size; k++)
        {
            // Se l'arco è nullo, termino
            if(batch[k] == NULL)
                break;

            // Prelevo i nodi dell'arco
            i = ((int*)batch[k])[0];
            j = ((int*)batch[k])[1];

            // Se i nodi sono validi
            if (i >= 0 && i < g->N && j >= 0 && j < g->N && i != j)
            {

                // Se ho terminato lo spazio per l'array degli archi entranti, lo rialloco
                if (g->in->size[j] % INCREMENT == 0)
                    g->in->list[j] = (int *)realloc(g->in->list[j], (g->in->size[j]+INCREMENT) * sizeof(int));

                // Aggiungo l'arco entrante
                g->in->list[j][g->in->size[j]] = i;

                // Incremento il numero di archi entranti
                g->in->size[j]++;

                // Incremento il numero di nodi letti
                nodes++;
            }

            // Dealloco l'arco
            free(batch[k]);
        }

        // Dealloco il batch
        free(batch);
    }

    // Ordino gli archi entranti
    for (i = 0; i < g->N; i++)
    {
        if (g->in->size[i] > 0)
        {
            // Nota: migliora il consumo di memoria, ma rallenta l'esecuzione
            // g->in->list[i] = (int *)realloc(g->in->list[i], g->in->size[i] * sizeof(int));
            qsort(g->in->list[i], g->in->size[i], sizeof(int), custom_compare);
        }
    }

    // Creo la struttura di ritorno
    read_return *ret = malloc(sizeof(read_return));
    // Grafo ottenuto
    ret->g = g;
    // Numero di nodi letti
    ret->nodes = nodes;

    // Termino il thread
    pthread_exit(ret);
}

void merge_sorted_arrays(const int* a, const int size_a, const int* b, const int size_b, int* result) {
    int i = 0, j = 0, k = 0;

    // Confronta gli elementi dei due array e inserisci il più piccolo nel risultato
    while (i < size_a && j < size_b) {
        if (a[i] < b[j]) {
            result[k++] = a[i++];
        } else {
            result[k++] = b[j++];
        }
    }

    // Inserisci i rimanenti elementi dell'array 'a' (se ce ne sono)
    while (i < size_a) {
        result[k++] = a[i++];
    }

    // Inserisci i rimanenti elementi dell'array 'b' (se ce ne sono)
    while (j < size_b) {
        result[k++] = b[j++];
    }
}

// Funzione che legge il file di input e crea il grafo finale
grafo* read_input(const char *filename, const int t, int *arcs_read){
    // Preparo la struttura per i thread
    thread_data_read *args = malloc(sizeof(thread_data_read));

    // Dimensione del buffer
    const int buffer_size = 100;
    // Dimensione del batch di archi
    const int batch_size = 10000;

    // Inizializzo la struttura
    args->buffer = NULL;

    sem_t sem_full, sem_empty;
    sem_init(&sem_full, 0, 0);
    sem_init(&sem_empty, 0, buffer_size);
    args->sem_empty = &sem_empty;
    args->sem_full = &sem_full;

    args->in = 0;
    args->out = 0;

    pthread_mutex_t m_buffer;
    pthread_mutex_init(&m_buffer, NULL);
    args->m_buffer = &m_buffer;

    args->end = false;
    args->total_batches = 0;
    args->read_batches = 0;

    args->buffer_size = buffer_size;
    args->batch_size = batch_size;

    // Apro il file
    FILE *f = fopen(filename, "r");
    if (f == NULL)
    {
        fprintf(stderr,"Errore apertura file");
        exit(1);
    }

    // Contatore per i batch inseriti nel buffer
    int batch_counter=0;

    // Alloco la memoria per i batch
    int** batch = malloc(sizeof(int*) * batch_size);

    // Inizializzo i batch
    for (int i = 0; i < batch_size; i++)
        batch[i] = NULL;

    // Variabili per la lettura del file
    char* line = NULL;
    size_t len = 0;
    int line_counter = 0;

    // Variabili per la lettura della matrice
    int r, c, n;

    // Array di thread
    pthread_t threads[t];

    gettimeofday(&start1, NULL);

    // Per ogni riga del file
    while (getline(&line, &len, f) != -1)
    {
        // Se la riga è un commento, la salto
        if (line[0] == '%') continue;

        // Se è la prima riga, leggo la dimensione della matrice
        if (!line_counter)
        {
            sscanf(line, "%d %d %d", &r, &c, &n);

            // Controllo che la matrice sia quadrata
            if(r != c)
            {
                fprintf(stderr,"Errore: matrice non quadrata\n");
                exit(1);
            }

            // Dimensione della matrice
            args->N = r;

            // Alloco la memoria per il buffer
            args->buffer = malloc(buffer_size * sizeof(void**));

            // Faccio partire i thread
            for(int i=0; i<t; i++)
                pthread_create(&threads[i], NULL, thread_read, args);
        }else{
            // Prelevo i nodi dell'arco
            int i, j;
            sscanf(line, "%d %d", &i, &j);

            // Li decremento come da specifiche
            i--;
            j--;

            // Alloco la memoria per il batch
            batch[batch_counter] = malloc(sizeof(int) * 2);

            // Inserisco i nodi nel batch
            batch[batch_counter][0] = i;
            batch[batch_counter][1] = j;

            // Incremento il contatore dei batch
            batch_counter++;

            // Se ho raggiunto la dimensione massima del batch
            if (batch_counter == batch_size)
            {
                // Aspetto che ci sia spazio nel buffer
                sem_wait(&sem_empty);
                // Lock del buffer
                pthread_mutex_lock(&m_buffer);

                // Inserisco il batch nel buffer
                args->buffer[args->in] = (void*)batch;
                // Aggiorno la posizione di scrittura
                args->in = (args->in + 1) % buffer_size;

                // Segnalo che c'è un batch nel buffer
                sem_post(&sem_full);
                // Rilascio il lock sul buffer
                pthread_mutex_unlock(&m_buffer);

                // Resetto il contatore dei batch
                batch_counter = 0;

                // Alloco la memoria per un nuovo batch
                batch = malloc(sizeof(int*) * batch_size);

                // Inizializzo il nuovo batch
                for (int k=0; k<batch_size; k++)
                    batch[k] = NULL;

                args->total_batches++;
            }
        }
        // Incremento il contatore delle righe lette
        line_counter++;
    }

    // Chiudo il file
    fclose(f);

    // Se c'è un batch da inserire nel buffer (non è stato inserito per raggiungimento della dimensione)
    if (batch_counter > 0)
    {
        // Aspetto che ci sia spazio nel buffer
        sem_wait(&sem_empty);
        // Lock del buffer
        pthread_mutex_lock(&m_buffer);

        // Inserisco il batch nel buffer
        args->buffer[args->in] = (void*)batch;

        // Segnalo che c'è un batch nel buffer
        sem_post(&sem_full);
        // Rilascio il lock sul buffer
        pthread_mutex_unlock(&m_buffer);

        args->total_batches++;
    }

    gettimeofday(&end1, NULL);
    timersub(&end1, &start1, &delta_load);
    fprintf(stderr, "Tempo di caricamento: %ld.%06ld\n", delta_load.tv_sec, delta_load.tv_usec);

    // Segnalo la fine del file
    args->end = true;
    // Segnalo la terminazione
    sem_post(&sem_full);

    // Creo il grafo finale
    grafo *g = malloc(sizeof(grafo));
    g->N = args->N;
    g->in = (struct inmap *)malloc(sizeof(struct inmap));
    g->out = (int *)malloc(g->N * sizeof(int));
    g->in->list = (int **)malloc(args->N * sizeof(int*));
    g->in->size = (int *)malloc(args->N * sizeof(int));

    // Inizializzo il grafo
    for (int i = 0; i < args->N; i++)
    {
        g->in->list[i] = NULL;
        g->in->size[i] = 0;
        g->out[i] = 0;
    }

    gettimeofday(&start1, NULL);

    // Merge dei grafi parziali

    // Variabile per la somma dei nodi letti
    int sum=0;

    // Per ogni thread
    for(int i=0; i<t; i++)
    {
        // Variabile per la struttura di ritorno
        read_return *ret;

        // Aspetto che il thread termini
        pthread_join(threads[i], (void **)&ret);

        // Aggiorno la somma dei nodi letti
        sum += ret->nodes;

        // Se il grafo è vuoto, lo dealloco e passo al prossimo thread
        if (ret->nodes == 0)
        {
            for (int j = 0; j < ret->g->N; j++)
                free(ret->g->in->list[j]);
            free(ret->g->in->list);
            free(ret->g->in->size);
            free(ret->g->in);
            free(ret->g->out);
            free(ret->g);
            free(ret);
            continue;
        }

        // Per ogni nodo del grafo parziale del thread corrente
        for (int j = 0; j < ret->g->N; j++)
        {
            // Se il nodo ha archi entranti
            if (ret->g->in->size[j] > 0)
            {
                // Se il grafo finale non ha archi entranti per quel nodo
                if (g->in->size[j] == 0)
                {
                    // Copio gli archi entranti
                    g->in->list[j] = ret->g->in->list[j];
                    // Copio la lunghezza dell'array
                    g->in->size[j] = ret->g->in->size[j];
                }
                else
                {
                    // Alloco un array temporaneo
                    int *temp = malloc((g->in->size[j] + ret->g->in->size[j]) * sizeof(int));

                    // Merge ordinato
                    merge_sorted_arrays(g->in->list[j], g->in->size[j], ret->g->in->list[j], ret->g->in->size[j], temp);

                    // Dealloco l'array vecchio
                    free(g->in->list[j]);
                    // Assegno l'array temporaneo all'array del grafo finale
                    g->in->list[j] = temp;
                    g->in->size[j] = g->in->size[j] + ret->g->in->size[j];

                    free(ret->g->in->list[j]);
                }
            }
        }

        free(ret->g->in->list);
        free(ret->g->in->size);
        free(ret->g->in);
        free(ret->g->out);
        free(ret->g);
        free(ret);
    }

    gettimeofday(&end1, NULL);
    timersub(&end1, &start1, &delta_elaborate);
    fprintf(stderr, "Tempo di merge: %ld.%06ld\n", delta_elaborate.tv_sec, delta_elaborate.tv_usec);

    // Distruzione del mutex
    pthread_mutex_destroy(&m_buffer);

    // Distruzione dei semafori
    sem_destroy(&sem_empty);
    sem_destroy(&sem_full);

    // Dealloco la memoria
    free(line);
    free(args->buffer);

    gettimeofday(&start1, NULL);

    // Per ogni nodo del grafo
    for (int i = 0; i < g->N; i++)
    {
        // Se il nodo ha archi entranti
        if (g->in->size[i] > 0)
        {
            // Alloco un array temporaneo
            int *temp = (int *)malloc(g->in->size[i] * sizeof(int));
            // Indice per l'array temporaneo
            int index = 0;

            // Array per gli archi entranti
            const int *a = g->in->list[i];

            // Per ogni arco entrante
            for (int j = 0; j < g->in->size[i]; j++)
            {
                // Se l'arco è diverso dal precedente
                if (j == 0 || a[j] != a[j - 1])
                {
                    // Aggiungo l'arco all'array temporaneo
                    temp[index] = a[j];
                    index++;
                    g->out[a[j]]++;
                    (*arcs_read)++;
                }
            }
            // Dealloco l'array vecchio
            free(g->in->list[i]);
            // Assegno l'array temporaneo
            g->in->list[i] = temp;
            // Aggiorno la lunghezza dell'array
            g->in->size[i] = index;
        }
    }

    gettimeofday(&end1, NULL);
    timersub(&end1, &start1, &delta_duplicates);
    fprintf(stderr, "Tempo di rimozione duplicati: %ld.%06ld\n", delta_duplicates.tv_sec, delta_duplicates.tv_usec);

    // Dealloco la memoria
    free(args);

    // Ritorno il grafo
    return g;
}