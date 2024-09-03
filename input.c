#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>
#include "utils.h"
#include "input.h"

#define INCREMENT 20

struct timeval start1, end1, delta_load, delta_elaborate, delta_duplicates;

void* thread_read(void *arg){
    thread_data_read *args = arg;

    int nodes = 0;

    grafo *g = malloc(sizeof(grafo));
    g->N = args->N;
    g->in = (struct inmap *)malloc(sizeof(struct inmap));
    g->out = (int *)malloc(g->N * sizeof(int));
    g->in->list = (int **)malloc(args->N * sizeof(int*));
    g->in->size = (int *)malloc(args->N * sizeof(int));

    for (int i = 0; i < args->N; i++)
    {
        g->in->list[i] = NULL;
        g->in->size[i] = 0;
        g->out[i] = 0;
    }

    int temp=0;

    while(true){
        sem_wait(args->sem_full);
        pthread_mutex_lock(args->m_buffer);

        int buffer_content = 0;
        sem_getvalue(args->sem_full, &buffer_content);

        if(args->end && buffer_content == 0){
            pthread_mutex_unlock(args->m_buffer);
            sem_post(args->sem_full);
            break;
        }

        void** batch = args->buffer[args->out];
        args->out = (args->out + 1) % args->buffer_size;

        sem_post(args->sem_empty);
        pthread_mutex_unlock(args->m_buffer);

        temp++;

        for(int k=0; k<args->batch_size; k++)
        {
            if(batch[k] == NULL)
                break;

            const int i = ((int*)batch[k])[0];
            const int j = ((int*)batch[k])[1];

            if (i >= 0 && i < g->N && j >= 0 && j < g->N && i != j)
            {

                if (g->in->size[j] % INCREMENT == 0)
                    g->in->list[j] = (int *)realloc(g->in->list[j], (g->in->size[j]+INCREMENT) * sizeof(int));

                g->in->size[j]++;

                g->in->list[j][g->in->size[j] - 1] = i;

                nodes++;
            }

            free(batch[k]);
        }

        free(batch);
    }

    read_return *ret = malloc(sizeof(read_return));
    ret->g = g;
    ret->nodes = nodes;

    pthread_exit(ret);
}

// Funzione che legge il file di input e crea il grafo
grafo* read_input(const char *filename, const int t, int *arcs_read){
    thread_data_read *args = malloc(sizeof(thread_data_read));

    const int buffer_size = 100;
    const int batch_size = 14682258;

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
    args->buffer_size = buffer_size;
    args->batch_size = batch_size;

    FILE *f = fopen(filename, "r");
    if (f == NULL)
    {
        fprintf(stderr,"Errore apertura file");
        exit(1);
    }

    int batch_counter=0;
    int** batch = malloc(sizeof(int*) * batch_size);
    for (int i = 0; i < batch_size; i++)
        batch[i] = NULL;
    char* line = NULL;
    size_t len = 0;
    int line_counter = 0;

    int r, c, n;

    pthread_t threads[t];

    gettimeofday(&start1, NULL);
    while (getline(&line, &len, f) != -1)
    {
        if (line[0] == '%') continue;

        if (!line_counter)
        {
            sscanf(line, "%d %d %d", &r, &c, &n);

            if(r != c)
            {
                fprintf(stderr,"Errore: matrice non quadrata\n");
                exit(1);
            }
            args->N = r;

            args->buffer = malloc(buffer_size * sizeof(void**));

            for(int i=0; i<t; i++)
            {
                pthread_create(&threads[i], NULL, thread_read, args);
            }
        }else{
            int i, j;
            sscanf(line, "%d %d", &i, &j);
            i--;
            j--;

            batch[batch_counter] = malloc(sizeof(char) * 2);
            batch[batch_counter][0] = i;
            batch[batch_counter][1] = j;

            batch_counter++;

            if (batch_counter == batch_size)
            {
                sem_wait(&sem_empty);
                pthread_mutex_lock(&m_buffer);

                args->buffer[args->in] = (void*)batch;
                args->in = (args->in + 1) % buffer_size;

                sem_post(&sem_full);
                pthread_mutex_unlock(&m_buffer);

                batch_counter = 0;
                batch = malloc(sizeof(int*) * batch_size);
                for (int k=0; k<batch_size; k++)
                    batch[k] = NULL;
            }
        }
        line_counter++;
    }

    if (batch_counter > 0)
    {
        sem_wait(&sem_empty);
        pthread_mutex_lock(&m_buffer);

        args->buffer[args->in] = (void*)batch;

        sem_post(&sem_full);
        pthread_mutex_unlock(&m_buffer);
    }

    fclose(f);
    gettimeofday(&end1, NULL);
    timersub(&end1, &start1, &delta_load);
    fprintf(stderr, "Tempo di caricamento: %ld.%06ld\n", delta_load.tv_sec, delta_load.tv_usec);

    args->end = true;
    sem_post(&sem_full);

    grafo *g = malloc(sizeof(grafo));
    g->N = args->N;
    g->in = (struct inmap *)malloc(sizeof(struct inmap));
    g->out = (int *)malloc(g->N * sizeof(int));
    g->in->list = (int **)malloc(args->N * sizeof(int*));
    g->in->size = (int *)malloc(args->N * sizeof(int));

    for (int i = 0; i < args->N; i++)
    {
        g->in->list[i] = NULL;
        g->in->size[i] = 0;
        g->out[i] = 0;
    }

    gettimeofday(&start1, NULL);

    int sum=0;
    for(int i=0; i<t; i++)
    {
        read_return *ret;
        pthread_join(threads[i], (void **)&ret);
        sum += ret->nodes;

        if (ret->nodes == 0)
        {
            free(ret->g->in);
            free(ret->g->out);
            free(ret->g);
            free(ret);
            continue;
        }

        // Merge dei grafi
        for (int j = 0; j < ret->g->N; j++)
        {
            if (ret->g->in->size[j] > 0)
            {
                if (g->in->size[j] == 0)
                {
                    g->in->list[j] = ret->g->in->list[j];
                    g->in->size[j] = ret->g->in->size[j];
                }
                else
                {
                    int *temp = (int *)malloc((g->in->size[j] + ret->g->in->size[j]) * sizeof(int));
                    int index = 0;

                    int *a = g->in->list[j];
                    int *b = ret->g->in->list[j];

                    int a_index = 0;
                    int b_index = 0;

                    while (a_index < g->in->size[j] && b_index < ret->g->in->size[j])
                    {
                        if (a[a_index] < b[b_index])
                        {
                            temp[index] = a[a_index];
                            a_index++;
                        }
                        else if (a[a_index] > b[b_index])
                        {
                            temp[index] = b[b_index];
                            b_index++;
                        }
                        else
                        {
                            temp[index] = a[a_index];
                            a_index++;
                            b_index++;
                        }
                        index++;
                    }

                    while (a_index < g->in->size[j])
                    {
                        temp[index] = a[a_index];
                        a_index++;
                        index++;
                    }

                    while (b_index < ret->g->in->size[j])
                    {
                        temp[index] = b[b_index];
                        b_index++;
                        index++;
                    }

                    free(g->in->list[j]);
                    g->in->list[j] = temp;
                    g->in->size[j] = index;
                }
            }
        }

        free(ret);
    }

    gettimeofday(&end1, NULL);
    timersub(&end1, &start1, &delta_elaborate);
    fprintf(stderr, "Tempo di merge: %ld.%06ld\n", delta_elaborate.tv_sec, delta_elaborate.tv_usec);

    pthread_mutex_destroy(&m_buffer);

    // Dealloco la memoria
    free(line);
    free(args->buffer);

    gettimeofday(&start1, NULL);
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
            int *a = g->in->list[i];
            // Ordino l'array
            qsort(a, g->in->size[i], sizeof(int), custom_compare);

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

    free(args);

    // Ritorno il grafo
    return g;
}