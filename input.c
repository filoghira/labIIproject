#define _DEFAULT_SOURCE
#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>
#include "utils.h"
#include "input.h"

#define SECTIONS 10
#define INCREMENT 100

struct timeval start1, end1, delta_load, delta_elaborate, delta_duplicates;

void* thread_read(void *arg){
    thread_data_read *args = arg;

    int nodes = 0;

    int i,j;
    while(true){
        pthread_mutex_lock(args->m_end);
        if (args->count == 0 )
        {
            pthread_mutex_unlock(args->m_end);
            break;
        }
        args->count--;
        pthread_mutex_lock(args->m_buffer);
        i = args->buffer[args->count][0];
        j = args->buffer[args->count][1];
        free(args->buffer[args->count]);
        pthread_mutex_unlock(args->m_buffer);
        pthread_mutex_unlock(args->m_end);

        if (i >= 0 && i < args->g->N && j >= 0 && j < args->g->N && i != j)
        {
            pthread_mutex_lock(args->m_g[j/SECTIONS]);

            if (args->g->in->size[j] % INCREMENT == 0)
                args->g->in->list[j] = (int *)realloc(args->g->in->list[j], (args->g->in->size[j]+INCREMENT) * sizeof(int));

            args->g->in->size[j]++;

            args->g->in->list[j][args->g->in->size[j] - 1] = i;

            pthread_mutex_unlock(args->m_g[j/SECTIONS]);
            nodes++;
        }
    }

    // Ritorno il numero di nodi elaborati
    int *ret = (int *)malloc(sizeof(int));
    *ret = nodes;
    pthread_exit((void *)ret);
}

// Funzione che legge il file di input e crea il grafo
grafo* read_input(const char *filename, const int t, int *arcs_read){
    thread_data_read *args = malloc(sizeof(thread_data_read));

    args->g = (grafo *)malloc(sizeof(grafo));
    args->g->in = (struct inmap *)malloc(sizeof(struct inmap));
    args->buffer = NULL;
    args->count = 0;
    sem_t sem;
    pthread_mutex_t m_buffer;
    pthread_mutex_t m_end;
    pthread_mutex_init(&m_buffer, NULL);
    pthread_mutex_init(&m_end, NULL);
    args->m_buffer = &m_buffer;
    args->m_end = &m_end;

    FILE *f = fopen(filename, "r");
    if (f == NULL)
    {
        fprintf(stderr,"Errore apertura file");
        exit(1);
    }

    char* line = NULL;
    size_t len = 0;
    int line_counter = 0;

    int r, c, n, n_mutex = 0;

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
            args->g->N = r;
            args->count = n;
            args->g->out = (int *)malloc(r * sizeof(int));
            args->g->in->list = (int **)malloc(r * sizeof(int*));
            args->g->in->size = (int *)malloc(r * sizeof(int));

            for (int i = 0; i < args->g->N; i++)
            {
                args->g->in->list[i] = NULL;
                args->g->in->size[i] = 0;
                args->g->out[i] = 0;
            }

            args->buffer = (int **)malloc(n * sizeof(int*));

            n_mutex = r / SECTIONS + 1;
            args->m_g = (pthread_mutex_t **)malloc(n_mutex * sizeof(pthread_mutex_t*));

            for (int i = 0; i < n_mutex; i++)
            {
                pthread_mutex_t *mutex = malloc(sizeof(pthread_mutex_t));
                pthread_mutex_init(mutex, NULL);
                args->m_g[i] = mutex;
            }

        }else{
            int i, j;
            sscanf(line, "%d %d", &i, &j);
            i--;
            j--;
            args->buffer[line_counter-1] = (int *)malloc(2 * sizeof(int));
            args->buffer[line_counter-1][0] = i;
            args->buffer[line_counter-1][1] = j;
        }
        line_counter++;
    }

    fclose(f);
    gettimeofday(&end1, NULL);
    timersub(&end1, &start1, &delta_load);
    fprintf(stderr, "Tempo di caricamento: %ld.%06ld\n", delta_load.tv_sec, delta_load.tv_usec);

    gettimeofday(&start1, NULL);
    pthread_t threads[t];

    for(int i=0; i<t; i++)
    {
        pthread_create(&threads[i], NULL, thread_read, (void *)args);
    }

    int sum=0;
    for(int i=0; i<t; i++)
    {
        int *ret;
        pthread_join(threads[i], (void **)&ret);
        sum += *ret;
        free(ret);
    }
    gettimeofday(&end1, NULL);
    timersub(&end1, &start1, &delta_elaborate);
    fprintf(stderr, "Tempo di elaborazione: %ld.%06ld\n", delta_elaborate.tv_sec, delta_elaborate.tv_usec);

    sem_destroy(&sem);
    pthread_mutex_destroy(&m_buffer);
    pthread_mutex_destroy(&m_end);

    // Dealloco la memoria
    free(line);
    free(args->buffer);

    gettimeofday(&start1, NULL);
    for (int i = 0; i < args->g->N; i++)
    {
        // Se il nodo ha archi entranti
        if (args->g->in->size[i] > 0)
        {
            // Alloco un array temporaneo
            int *temp = (int *)malloc(args->g->in->size[i] * sizeof(int));
            // Indice per l'array temporaneo
            int index = 0;

            // Array per gli archi entranti
            int *a = args->g->in->list[i];
            // Ordino l'array
            qsort(a, args->g->in->size[i], sizeof(int), custom_compare);

            // Per ogni arco entrante
            for (int j = 0; j < args->g->in->size[i]; j++)
            {
                // Se l'arco è diverso dal precedente
                if (j == 0 || a[j] != a[j - 1])
                {
                    // Aggiungo l'arco all'array temporaneo
                    temp[index] = a[j];
                    index++;
                    args->g->out[a[j]]++;
                    (*arcs_read)++;
                }
            }
            // Dealloco l'array vecchio
            free(args->g->in->list[i]);
            // Assegno l'array temporaneo
            args->g->in->list[i] = temp;
            // Aggiorno la lunghezza dell'array
            args->g->in->size[i] = index;
        }
    }
    gettimeofday(&end1, NULL);
    timersub(&end1, &start1, &delta_duplicates);
    fprintf(stderr, "Tempo di rimozione duplicati: %ld.%06ld\n", delta_duplicates.tv_sec, delta_duplicates.tv_usec);

    for (int i = 0; i < n_mutex; ++i) {
        pthread_mutex_destroy(args->m_g[i]);
        free(args->m_g[i]);
    }
    free(args->m_g);

    grafo *g_res = args->g;
    free(args);

    // Ritorno il grafo
    return g_res;
}