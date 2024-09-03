#include <math.h>
#include <stdlib.h>
#include <pthread.h>
#include "pagerank.h"

#include <stdio.h>

// Thread che calcola una componente del pagerank
void* thread_pagerank(void *arg){
    // Ottengo i dati passati come argomento
    thread_data_calc *data = arg;

    while (true)
    {
        sem_wait(data->sem_full);
        sem_wait(data->mutex_buffer);

        if (data->end)
        {
            sem_post(data->mutex_buffer);
            sem_post(data->sem_full);
            break;
        }

        int low_limit = data->buffer[data->out][0];
        int high_limit = data->buffer[data->out][1];
        free(data->buffer[data->out]);
        data->out = (data->out + 1) % data->buffer_size;

        sem_post(data->mutex_buffer);
        sem_post(data->sem_empty);

        // In base all'operazione da eseguire
        // 1: Calcolo la somma dei pagerank e preparo il vettore Y
        // 2: Calcolo il nuovo pagerank
        // 3: Calcolo l'errore
        switch (data->op)
        {
            // Calcolo la somma dei pagerank e preparo il vettore Y
            case 1:
                // Se il nodo ha archi uscenti
                for (int j=low_limit; j<high_limit; j++)
                {
                    if (data->g->out[j] != 0)
                    {
                        // Aggiorno il vettore Y in posizione j
                        data->Y[j] = data->X[j] / data->g->out[j];
                    }
                }
                break;
            // Calcolo il nuovo pagerank
            case 2: ;
                for (int j=low_limit; j<high_limit; j++)
                {
                    // Calcolo la somma dei pagerank dei nodi entranti
                    double sum = 0;
                    for (int i = 0; i < data->g->in->size[j]; i++)
                    {
                        int node = data->g->in->list[j][i];
                        sum += data->Y[node];
                    }

                    // Calcolo il nuovo pagerank
                    data->Xnew[j] = (1-data->d)/data->g->N + data->d/data->g->N*data->S + data->d*sum;
                    // Calcolo l'errore
                    data->err[j] = fabs(data->Xnew[j] - data->X[j]);
                    data->X[j] = data->Xnew[j];
                }
                break;
            default:
                break;
        }

        sem_post(data->sem_calc);
    }

    // Termino il thread
    pthread_exit(NULL);
}