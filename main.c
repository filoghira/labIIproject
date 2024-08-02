#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/time.h>
#include "structures.h"
#include "parameters.h"
#include "output.h"
#include "utils.h"
#include "input.h"
#include "pagerank.h"

struct timeval start, end, delta_input, delta_pagerank;

// Variabili per la gestione dei segnali
pthread_cond_t signal_cond;
pthread_mutex_t signal_mutex;

// Funzione che gestisce i segnali
void signal_handler(const int sig){

    // Se il segnale è SIGUSR1
    if (sig == SIGUSR1)
        // Sblocco il thread che aspetta il segnale
        pthread_cond_signal(&signal_cond);
}

// Thread che gestisce il segnale SIGUSR1
// ReSharper disable once CppParameterMayBeConstPtrOrRef
void* signal_thread(void *arg)
{
    // Ottengo i dati passati come argomento
    const thread_data_calc *data = arg;

    // Attendo una volta il segnale per iniziare
    pthread_cond_wait(&signal_cond, &signal_mutex);

    // Finchè non è terminato il calcolo
    while (!data->end)
    {
        // Attendo il segnale
        pthread_cond_wait(&signal_cond, &signal_mutex);

        // Stampo le informazioni
        fprintf(stderr, "Current iteration: %d\n", data->current_iter);

        double max = data->X[0];
        int max_index = 0;
        for (int i=1; i<data->g->N; i++)
        {
            if (data->X[i] > max)
            {
                max = data->X[i];
                max_index = i;
            }
        }

        fprintf(stderr, "Max pagerank: %d %f\n", max_index, max);
    }

    // Termino il thread
    pthread_exit(NULL);
}

// Funzione che calcola il pagerank
double* pagerank(grafo *g, double d, double eps, int maxiter, int taux, int* numiter){
    // Array per i pagerank
    double *X = malloc(g->N * sizeof(double));
    // Inizializzo l'array a 1/N
    set_array(X, g->N, 1.0/g->N);

    // Array per i nuovi pagerank
    double *Xnew = malloc(g->N * sizeof(double));
    // Inizializzo l'array a 0
    set_array(Xnew, g->N, 0.0);

    // Array per il vettore Y
    double *Y = malloc(g->N * sizeof(double));
    // Inizializzo l'array a 0
    set_array(Y, g->N, 0.0);

    // Creo i thread
    pthread_t threads[taux];

    // Creo e inizializzo i semafori
    sem_t sem_calc;
    sem_init(&sem_calc, 0, 0);

    sem_t mutex_err;
    sem_init(&mutex_err, 0, 1);

    sem_t sem_buffer;
    sem_init(&sem_buffer, 0, 0);
    sem_t mutex_buffer;
    sem_init(&mutex_buffer, 0, 1);

    sem_t mutex_S;
    sem_init(&mutex_S, 0, 1);

    sem_t mutex_iter;
    sem_init(&mutex_iter, 0, 1);

    // Creo la struttura per i dati dei thread
    thread_data_calc data;
    data.sem_calc = &sem_calc;
    data.end = false;
    data.buffer = NULL;
    data.mutex_buffer = &mutex_buffer;
    data.sem_buffer = &sem_buffer;
    data.X = X;
    data.Y = Y;
    data.Xnew = Xnew;
    data.S = 0;
    data.mutex_S = &mutex_S;
    data.err = 0;
    data.mutex_err = &mutex_err;
    data.g = g;
    data.d = d;
    data.current_iter = 0;
    data.mutex_iter = &mutex_iter;

    // Creo i thread
    for (int i = 0; i < taux; i++)
        pthread_create(&threads[i], NULL, thread_pagerank, &data);

    // Creo il thread per gestire i segnali
    pthread_t signal_thread_id;
    pthread_create(&signal_thread_id, NULL, signal_thread, &data);

    // Finché non ho raggiunto il numero massimo di iterazioni
    while (data.current_iter < maxiter)
    {
        // Aspetto che la variabile che conta l'iterazione corrrente sia disponibile
        sem_wait(data.mutex_iter);
        // Incremento il numero di iterazioni
        data.current_iter++;
        // Rilascio la variabile
        sem_post(data.mutex_iter);

        // Preparo il nuovo nodo per il buffer
        struct node_calc *temp_buffer = NULL;

        // fprintf(stderr, "Iteration %d\n", iter);

        // Sezione per la preparazione della somma dei pagerank e del vettore Y
        for (int j=0; j<g->N; j++)
        {
            // Preparo il nodo per il buffer
            struct node_calc *S = malloc(sizeof(struct node_calc));
            S->op = 1;
            S->j = j;

            // Aggiungo il nodo al buffer
            S->next = temp_buffer;
            temp_buffer = S;
        }

        // Aggiorno il buffer
        data.buffer = temp_buffer;

        // Sblocco i thread
        for (int i=0; i<g->N; i++)
            sem_post(&sem_buffer);

        // Aspetto che i thread terminino
        for (int i=0; i<g->N; i++)
            sem_wait(&sem_calc);

        // Resetto il buffer
        temp_buffer = NULL;

        // Sezione per il calcolo del nuovo pagerank
        for (int j=0; j<g->N; j++)
        {
            // Preparo il nodo per il buffer
            struct node_calc *X_buff = malloc(sizeof(struct node_calc));
            X_buff->op = 2;
            X_buff->j = j;

            // Aggiungo il nodo al buffer
            X_buff->next = temp_buffer;
            temp_buffer = X_buff;
        }

        // Aggiorno il buffer
        data.buffer = temp_buffer;

        // Sblocco i thread
        for (int i=0; i<g->N; i++)
            sem_post(&sem_buffer);

        // Aspetto che i thread terminino
        for (int i=0; i<g->N; i++)
            sem_wait(&sem_calc);

        // Aggiorno il vettore dei pagerank invertendo i puntatori
        double *temp = data.X;
        data.X = data.Xnew;
        data.Xnew = temp;

        // Controllo se l'errore è minore della soglia, se sì esco
        if (data.err < eps) break;

        // Initializzo l'errore e la somma a 0
        data.err = 0;
        data.S = 0;
    }

    // Avviso i thread che devono terminare
    data.end = true;

    // Sblocco i thread
    for (int i = 0; i < taux; i++)
        sem_post(data.sem_buffer);

    // Aspetto che i thread terminino
    for (int i = 0; i < taux; i++)
        pthread_join(threads[i], NULL);

    // Sblocco il thread che gestisce i segnali
    pthread_cond_signal(&signal_cond);
    pthread_join(signal_thread_id, NULL);

    // Restituisco il numero di iterazioni
    *numiter = data.current_iter;

    // Libero la memoria
    free(data.buffer);
    free(data.Y);
    free(data.Xnew);

    sem_destroy(&sem_calc);
    sem_destroy(&mutex_err);
    sem_destroy(&sem_buffer);
    sem_destroy(&mutex_buffer);
    sem_destroy(&mutex_S);
    sem_destroy(&mutex_iter);

    // Restituisco il vettore dei pagerank
    return X;
}

int main(const int argc, char *argv[]){
    // Inizializzo la variabile per la gestione dei segnali
    pthread_cond_init(&signal_cond, NULL);
    pthread_mutex_init(&signal_mutex, NULL);
    // Imposto il gestore dei segnali
    signal(SIGUSR1, signal_handler);

    // Leggo i parametri in input
    input_data *data = input(argc, argv);

    // Variabile per il numero di archi letti
    int arcs_read = 0;

    // gettimeofday(&t0, NULL);

    gettimeofday(&start, NULL);
    // Leggo il file di input e creo il grafo
    grafo *g = read_input(data->filename, data->t, &arcs_read);
    gettimeofday(&end, NULL);
    timersub(&end, &start, &delta_input);
    fprintf(stderr, "Time to read input: %ld.%06ld\n", delta_input.tv_sec, delta_input.tv_usec);

    // gettimeofday(&t1, NULL);
    // timersub(&t1, &t0, &dt);
    // fprintf(stderr, "Time to read input: %ld.%06ld\n", dt.tv_sec, dt.tv_usec);

    // Stampo i risultati iniziali
    output_print_start(g->N, dead_end(g), arcs_read);

    // Variabile per il numero di iterazioni
    int numiter = -1;

    gettimeofday(&start, NULL);

    // Calcolo il pagerank
    double *res = pagerank(g, data->d, data->e, data->m, data->t, &numiter);

    gettimeofday(&end, NULL);
    timersub(&end, &start, &delta_pagerank);
    fprintf(stderr, "Time to calculate pagerank: %ld.%06ld\n", delta_pagerank.tv_sec, delta_pagerank.tv_usec);

    // Creo un vettore di struct per tenere traccia del pagerank con l'indice del nodo
    map* map_res = malloc(g->N * sizeof(map));
    // Inizializzo il vettore
    for (int i=0; i<g->N; i++)
    {
        map_res[i].val = res[i];
        map_res[i].index = i;
    }

    // Ordino il vettore in ordine decrescente
    qsort(map_res, g->N, sizeof(map), compare_desc);

    // Calcolo la somma dei pagerank
    double sum = 0;
    for (int i=0; i<g->N; i++)
        sum += res[i];

    // Stampo i risultati finali
    output_print_end(numiter, sum, data->k, map_res);

    // Libero la memoria
    free(map_res);
    free(data);
    free(res);
    for (int i = 0; i < g->N; i++) {
        free(g->in->list[i]);
    }
    free(g->in->list);
    free(g->in->size);
    free(g->out);
    free(g->in);
    free(g);

    // Termino il programma
    return 0;
}