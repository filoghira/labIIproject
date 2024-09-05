#include <math.h>
#include <stdlib.h>
#include <pthread.h>
#include "pagerank.h"

#include <stdio.h>

// Somma tutte le componenti di Y
double Y_sum(const int j, const double *Y, int **list, const int *size){
    double sum = 0;
    for (int i = 0; i < size[j]; i++)
    {
        const int node = list[j][i];
        sum += Y[node];
    }
    return sum;
}

// Caclola tutte le componenti di Y
void Y_calc(const int low_limit, const int high_limit, const int* out, const double *X, double *Y) {
    for (int j=low_limit; j<high_limit; j++)
        // Se non è dead end
        if (out[j] != 0)
            // Aggiorno il vettore Y in posizione j
            Y[j] = X[j] / out[j];
}

// Thread che calcola una componente del pagerank
void* thread_pagerank(void *arg){
    // Ottengo i dati passati come argomento
    thread_data_calc *data = arg;

    int low_limit = -1;
    int high_limit = -1;

    while (true)
    {
        // Attendo che il buffer abbia un elemento
        sem_wait(data->sem_full);
        // Attendo che il buffer sia disponibile
        sem_wait(data->mutex_buffer);

        // Controllo se è arrivato il segnale di terminazione
        if (data->end)
        {
            // Rilascio il mutex
            sem_post(data->mutex_buffer);
            // Segnalo agli altri thread di terminare
            sem_post(data->sem_full);
            break;
        }

        // Estraggo i range del batch
        low_limit = data->buffer[data->out][0];
        high_limit = data->buffer[data->out][1];

        // Libero la memoria occupata dal batch
        free(data->buffer[data->out]);

        // Aggiorno l'indice di lettura
        data->out = (data->out + 1) % data->buffer_size;

        // Rilascio il mutex
        sem_post(data->mutex_buffer);
        // Segnalo che il buffer ha un elemento in meno
        sem_post(data->sem_empty);

        // In base all'operazione da eseguire
        // 1: Calcolo la somma dei pagerank e preparo il vettore Y
        // 2: Calcolo il nuovo pagerank, l'errore parziale e aggiorno il pagerank
        switch (data->op)
        {
            // Calcolo la somma dei pagerank e preparo il vettore Y
            case 1:
                // Se il nodo ha archi uscenti
                Y_calc(low_limit, high_limit, data->g->out, data->X, data->Y);
                break;
            // Calcolo il nuovo pagerank
            case 2:
                for (int j=low_limit; j<high_limit; j++)
                {
                    // Calcolo il nuovo pagerank
                    data->Xnew[j] = data->first_term + data->d/data->g->N*data->S + data->d* Y_sum(j, data->Y, data->g->in->list, data->g->in->size);

                    // Calcolo l'errore
                    data->err[j] = fabs(data->Xnew[j] - data->X[j]);

                    // Aggiorno il pagerank
                    data->X[j] = data->Xnew[j];
                }
                break;
        }

        // Segnalo che ho finito il calcolo
        sem_post(data->sem_calc);
    }

    // Termino il thread
    pthread_exit(NULL);
}