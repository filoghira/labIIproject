#ifndef LABIIPROJECT_STRUCTURES_H
#define LABIIPROJECT_STRUCTURES_H


// ReSharper disable once CppUnusedIncludeDirective
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <stdbool.h>

struct inmap{
    int **list;
    int *size;
};

typedef struct {
    int N; // Numero di nodi nel grafo
    int *out; // Array contenente il numero di archi uscenti per ogni nodo
    struct inmap *in; // Array contenente la lista degli archi entranti per ogni nodo
} grafo;

typedef struct {
    int k; // Top k nodi da stampare
    int m; // Numero di iterazioni massime
    double d; // Damping factor
    double e; // Error massimo
    int t; // Numero di thread
    char *filename; // Nome del file di input
} input_data;

typedef struct {
    // Buffer per i dati in input
    int **buffer;
    int count;

    pthread_mutex_t *m_buffer;
    pthread_mutex_t *m_end;

    // Grafo
    grafo *g;
    pthread_mutex_t **m_g;

    sem_t *sem_buffer;
} thread_data_read;

struct node_calc
{
    int op; // Operazione da eseguire
    int j; // Indice del valore nel vettore pagerank
    struct node_calc *next; // Puntatore al prossimo elemento della lista
};

typedef struct
{
    // Semaforo per tenere traccia delle operazioni da eseguire
    sem_t *sem_calc;

    // Flag per terminare i thread
    bool end;

    // Buffer per i dati da elaborare
    struct node_calc *buffer;
    // Mutex per il buffer
    sem_t *mutex_buffer;
    // Semaforo per il buffer
    sem_t *sem_buffer;

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
