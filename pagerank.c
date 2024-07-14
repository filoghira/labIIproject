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
        // Aspetto che ci siano operazioni da eseguire
        sem_wait(data->sem_buffer);

        // Controllo se devo terminare
        if (data->end) break;

        // Aspetto che il buffer sia disponibile
        sem_wait(data->mutex_buffer);
        // Leggo i dati dal buffer
        struct node_calc* new = data->buffer;
        // Rimuovo l'elemento dal buffer
        data->buffer = data->buffer->next;
        // Rilascio il buffer
        sem_post(data->mutex_buffer);

        // fprintf(stderr, "Thread %ld: %d\n", pthread_self(), new->op);

        // In base all'operazione da eseguire
        // 1: Calcolo la somma dei pagerank e preparo il vettore Y
        // 2: Calcolo il nuovo pagerank
        // 3: Calcolo l'errore
        switch (new->op)
        {
            // Calcolo la somma dei pagerank e preparo il vettore Y
            case 1:
                // Se il nodo ha archi uscenti
                if (data->g->out[new->j] != 0)
                {
                    // Aggiorno il vettore Y in posizione j
                    data->Y[new->j] = data->X[new->j] / data->g->out[new->j];
                } else {
                    // Altimenti aggiorno la somma dei pagerank dei nodi senza archi uscenti
                    sem_wait(data->mutex_S);
                    data->S += data->X[new->j];
                    data->Y[new->j] = 0;
                    sem_post(data->mutex_S);
                }
                // Decremento il numero di operazioni da eseguire
                sem_post(data->sem_calc);
                break;
            // Calcolo il nuovo pagerank
            case 2: ;
                // Calcolo la somma dei pagerank dei nodi entranti
                double sum = 0;
                for (int i = 0; i < data->g->in->size[new->j]; i++)
                {
                    int node = data->g->in->list[new->j][i];
                    sum += data->Y[node];
                }

                // Calcolo il nuovo pagerank
                data->Xnew[new->j] = (1-data->d)/data->g->N + data->d/data->g->N*data->S + data->d*sum;

                // fprintf(stderr, "Thread %ld: %f %f %f\n", pthread_self(), (1-data->d)/data->g->N, data->d/data->g->N*data->S, data->d*sum);
                // fprintf(stderr, "Thread %ld: nuovo %d %f\n", pthread_self(),new->j, data->Xnew[new->j]);

                // Decremento il numero di operazioni da eseguire
                sem_post(data->sem_calc);
                break;
                // Calcolo l'errore
            case 3: ;
                // Calcolo l'errore
                double temp = fabs(data->Xnew[new->j] - data->X[new->j]);

                // Attendo che l'errore sia disponibile
                sem_wait(data->mutex_err);
                // Aggiorno l'errore
                data->err += temp;
                // Rilascio l'errore
                sem_post(data->mutex_err);

                // Aggiorno il nuovo pagerank
                data->X[new->j] = data->Xnew[new->j];

                // Decremento il numero di operazioni da eseguire
                sem_post(data->sem_calc);
                break;
            default:
                break;
        }

        // Libero la memoria
        free(new);
    }

    // Termino il thread
    pthread_exit(NULL);
}