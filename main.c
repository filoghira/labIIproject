#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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

// Variabili per il calcolo del tempo
struct timeval start, end, delta_input, delta_pagerank;
struct timeval start_1, end_1, delta_phase_1, temp1, start_2, end_2, delta_phase_2, temp2;

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

        // Trovo il pagerank massimo corrente
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

    // Dimensione del buffer
    const int buffer_size = 32;
    // Dimensione del batch
    const int batch_size = 600;

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

    // Array per l'errore
    double *err = malloc(sizeof(double) * g->N);
    // Inizializzo l'array a 0
    set_array(err, g->N, 0.0);

    // Array per i thread
    pthread_t threads[taux];

    // Creo e inizializzo i semafori
    sem_t sem_calc;
    sem_init(&sem_calc, 0, 0);

    sem_t sem_full;
    sem_init(&sem_full, 0, 0);

    sem_t sem_empty;
    sem_init(&sem_empty, 0, buffer_size);

    sem_t mutex_buffer;
    sem_init(&mutex_buffer, 0, 1);

    sem_t mutex_iter;
    sem_init(&mutex_iter, 0, 1);

    // Calcolo il numero di batch necessari
    const int batch_number = ceil((double)g->N / batch_size);

    // Creo la struttura per i dati dei thread
    thread_data_calc data;
    data.sem_calc = &sem_calc;
    data.end = false;
    data.buffer = malloc(buffer_size * sizeof(int*));
    data.mutex_buffer = &mutex_buffer;
    data.sem_full = &sem_full;
    data.sem_empty = &sem_empty;
    data.in = 0;
    data.out = 0;
    data.X = X;
    data.Y = Y;
    data.Xnew = Xnew;
    data.S = 0;
    data.err = err;
    data.g = g;
    data.d = d;
    data.current_iter = 0;
    data.mutex_iter = &mutex_iter;
    data.buffer_size = buffer_size;

    // Faccio partire i thread
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

        gettimeofday(&start_1, NULL);

        // Fase 1
        data.op = 1;

        // Per ogni batch
        for (int i=0; i<batch_number; i++)
        {
            // Aspetto che ci sia spazio nel buffer
            sem_wait(&sem_empty);
            // Lock del buffer
            sem_wait(&mutex_buffer);

            // Alloco la memoria per il batch
            int* batch = malloc(2 * sizeof(int));

            // Imposto l'indice di inizio e fine del batch
            batch[0] = i * batch_size;
            batch[1] = (i+1) * batch_size > g->N ? g->N : (i+1) * batch_size;

            // Inserisco il batch nel buffer
            data.buffer[data.in] = batch;

            // Incremento l'indice di inserimento
            data.in = (data.in + 1) % buffer_size;

            // Sblocco il buffer
            sem_post(&mutex_buffer);
            // Segnalo che c'è un batch da calcolare
            sem_post(&sem_full);
        }

        // Aspetto che tutti i batch siano stati calcolati
        for (int i=0; i<batch_number; i++)
            sem_wait(&sem_calc);

        // Calcolo l'array S
        for (int i=0; i<g->N; i++)
            if (g->out[i] == 0)
                data.S += data.X[i];

        gettimeofday(&end_1, NULL);
        timersub(&end_1, &start_1, &temp1);
        timeradd(&delta_phase_1, &temp1, &delta_phase_1);
        gettimeofday(&start_2, NULL);

        // Fase 2
        data.op = 2;

        // Resetto gli indici del buffer
        data.out = 0;
        data.in = 0;

        // Riempo il batch analogamente al passo precedente
        for (int i=0; i<batch_number; i++)
        {
            sem_wait(&sem_empty);
            sem_wait(&mutex_buffer);

            int* batch = malloc(2 * sizeof(int));
            batch[0] = i * batch_size;
            batch[1] = (i+1) * batch_size > g->N ? g->N : (i+1) * batch_size;
            data.buffer[data.in] = batch;
            data.in = (data.in + 1) % buffer_size;

            sem_post(&mutex_buffer);
            sem_post(&sem_full);
        }

        // Aspetto che tutti i batch siano stati calcolati
        for (int i=0; i<batch_number; i++)
            sem_wait(&sem_calc);

        // Calcolo l'errore
        double err_sum = 0;
        for (int i=0; i<g->N; i++)
            err_sum += data.err[i];

        gettimeofday(&end_2, NULL);
        timersub(&end_2, &start_2, &temp2);
        timeradd(&delta_phase_2, &temp2, &delta_phase_2);

        // Controllo se l'errore è minore della soglia, se sì esco
        if (err_sum < eps) break;

        // Initializzo l'errore e la somma a 0
        data.S = 0;
        set_array(data.Y, g->N, 0.0);
        set_array(err, g->N, 0.0);
    }

    // Avviso i thread che devono terminare
    data.end = true;

    // Sblocco i thread
    sem_post(&sem_full);

    // Aspetto che i thread terminino
    for (int i = 0; i < taux; i++)
        pthread_join(threads[i], NULL);

    // Sblocco il thread che gestisce i segnali
    pthread_cond_signal(&signal_cond);
    // Aspetto che il thread termini
    pthread_join(signal_thread_id, NULL);

    // Restituisco il numero di iterazioni
    *numiter = data.current_iter;

    // Libero la memoria
    free(data.buffer);
    free(data.Y);
    free(data.Xnew);
    free(data.err);

    // Distruggo i semafori
    sem_destroy(&sem_calc);
    sem_destroy(&sem_full);
    sem_destroy(&sem_empty);

    // Distruggo i mutex
    sem_destroy(&mutex_buffer);
    sem_destroy(&mutex_iter);

    fprintf(stderr, "Time to calculate phase 1: %ld.%06ld\n", delta_phase_1.tv_sec, delta_phase_1.tv_usec);
    fprintf(stderr, "Time to calculate phase 2: %ld.%06ld\n", delta_phase_2.tv_sec, delta_phase_2.tv_usec);

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

    gettimeofday(&start, NULL);

    // Leggo il file di input e creo il grafo
    grafo *g = read_input(data->filename, data->t, &arcs_read);

    gettimeofday(&end, NULL);
    timersub(&end, &start, &delta_input);
    fprintf(stderr, "Time to read input: %ld.%06ld\n", delta_input.tv_sec, delta_input.tv_usec);

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
    free(res);
    free(map_res);
    for (int i = 0; i < g->N; i++) {
        free(g->in->list[i]);
    }
    free(g->in->list);
    free(g->in->size);
    free(g->out);
    free(g->in);
    free(g);
    free(data);

    // Termino il programma
    return 0;
}