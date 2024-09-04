#ifndef LABIIPROJECT_STRUCTURES_H
#define LABIIPROJECT_STRUCTURES_H

// ReSharper disable once CppUnusedIncludeDirective
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <stdbool.h>

// Struttura per tenere traccia degli archi entranti per ogni nodo
struct inmap{
    int **list;
    int *size;
};

// Struttura per tenere traccia del grafo
typedef struct {
    int N; // Numero di nodi nel grafo
    int *out; // Array contenente il numero di archi uscenti per ogni nodo
    struct inmap *in; // Array contenente la lista degli archi entranti per ogni nodo
} grafo;

// Struttura per tenere traccia dei dati in input
typedef struct {
    int k; // Top k nodi da stampare
    int m; // Numero di iterazioni massime
    double d; // Damping factor
    double e; // Error massimo
    int t; // Numero di thread
    char *filename; // Nome del file di input
} input_data;

// Struttura per i parametri dei thread di lettura
typedef struct {
    // Buffer per i dati in input
    void ***buffer;

    // Dimensione del buffer
    int buffer_size;
    // Dimensione del batch
    int batch_size;

    // Flag per terminare i thread
    bool end;

    // Mutex per il buffer
    pthread_mutex_t *m_buffer;

    // Semafori
    sem_t* sem_empty;
    sem_t* sem_full;

    // Posizione in cui scrivere e leggere
    int in;
    int out;

    // Dimensione del file
    int N;
} thread_data_read;

// Struttura per il risultato dei thread di lettura
typedef struct
{
    // Grafo parziale
    grafo *g;
    // Numero di nodi letti
    int nodes;
} read_return;

// Struttura per i parametri dei thread di calcolo
typedef struct
{
    // Semaforo per tenere traccia delle operazioni eseguite
    sem_t *sem_calc;

    // Dimensione del buffer
    int buffer_size;

    // Flag per terminare i thread
    bool end;

    // Codice dell'operazione da eseguire
    int op;

    // Buffer per i dati da elaborare
    int **buffer;

    // Mutex per il buffer
    sem_t *mutex_buffer;

    // Semafori per il buffer
    sem_t *sem_full;
    sem_t *sem_empty;

    // Posizione in cui scrivere e leggere
    int in;
    int out;

    // Vettore dei pagerank
    double *X;
    // Vettore Y ausiliario
    double *Y;
    // Vettore dei nuovi pagerank
    double *Xnew;

    // Somma dei pagerank dei nodi senza archi uscenti
    double S;

    // Errore ad ogni iterazione
    double *err;

    // Grafo
    grafo *g;

    // Damping factor
    double d;

    // Numero di iterazione corrente
    int current_iter;

    // Mutex per l'iterazione
    sem_t *mutex_iter;
} thread_data_calc;

// Struttura per tenere traccia del pagerank con l'indice del nodo
typedef struct
{
    // Valore del pagerank
    double val;
    // Indice del nodo
    int index;
} map;

#endif
