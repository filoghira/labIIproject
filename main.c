#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <math.h>
#include <sys/time.h>
#include <signal.h>

// Variabili per il calcolo del tempo
struct timeval t0, t1, dt;
// Variabili per la gestione dei segnali
pthread_cond_t signal_cond;
pthread_mutex_t signal_mutex;

struct inmap{
    struct inmap *next; // Puntatore al prossimo elemento della lista
    int node; // Numero del nodo
};

typedef struct {
    int N; // Numero di nodi nel grafo
    int *out; // Array contenente il numero di archi uscenti per ogni nodo
    struct inmap **in; // Array contenente la lista degli archi entranti per ogni nodo
} grafo;

typedef struct {
    int k; // Top k nodi da stampare
    int m; // Numero di iterazioni massime
    double d; // Damping factor
    double e; // Error massimo
    int t; // Numero di thread
    char *filename; // Nome del file di input
} input_data;

struct node_read{
    int i; // Indice del primo nodo
    int j; // Indice del secondo nodo
    struct node_read *next; // Puntatore al prossimo elemento della lista
};

typedef struct {
    // Buffer per i dati in input
    struct node_read *buffer;
    // Semaforo binario (mutex) per il buffer
    sem_t *mutex_buffer;

    // Semaforo per gli archi da leggere
    sem_t *arcs;

    // Grafo
    grafo *g;
    // Semaforo binario (mutex) per il grafo
    sem_t *mutex_graph;

    // Numero totale di archi
    int total_arcs;
    // Numero massimo di archi
    int max_arcs;

    // Semaforo binario (mutex) per il numero di archi letti
    sem_t *mutex_arcs;

    // Flag per terminare i thread
    bool end;
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
    // Mutex per il vettore dei pagerank
    sem_t *mutex_X;

    // Vettore Y ausiliario
    double *Y;
    // Mutex per il vettore Y
    sem_t *mutex_Y;

    // Vettore dei nuovi pagerank
    double *Xnew;
    // Mutex per il vettore dei nuovi pagerank
    sem_t *mutex_Xnew;

    // Somma dei pagerank dei nodi senza archi uscenti
    double S;
    // Mutex per la somma
    sem_t *mutex_S;

    // Errore ad ogni iterazione
    double err;
    // Mutex per l'errore
    sem_t *mutex_err;

    // Grafo
    grafo *g;
    // Mutex per il grafo
    sem_t *sem_g;

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

// Funzione che legge i parametri passati come argomenti da riga di comando
input_data* input(const int argc, char *argv[]){
    // Alloco memoria per la struttura che conterrà i dati in input
    input_data *data = malloc(sizeof(input_data));

    // Inizializzo i valori di default
    data->k = 3;
    data->m = 100;
    data->d = 0.9;
    data->e = 0.0000001;
    data->t = 1;
    data->filename = NULL;

    // fprintf(stderr,"Reading options\n");

    int c;
    // Leggo le opzioni passate come argomenti da riga di comando
    while((c = getopt(argc, argv, "k:m:d:e:t:")) != -1){
        switch(c){
            // Top k nodi da stampare
            case 'k':
                data->k = atoi(optarg);
                break;
            // Numero massimo di iterazioni
            case 'm':
                data->m = atoi(optarg);
                break;
            // Numero di thread
            case 't':
                data->t = atoi(optarg);
                break;
            // Damping factor
            case 'd':
                data->d = atof(optarg);
                break;
            // Errore massimo
            case 'e':
                data->e = atof(optarg);
                break;
            // Opzione non valida
            case '?':
                // Se c'è un errore nella lettura delle opzioni ma questa viene riconosciuta, allora c'è un errore nel
                // valore dell'opzione
                if(optopt == 'k' || optopt == 'm' || optopt == 't' || optopt == 'd' || optopt == 'e'){
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                } else if(isprint(optopt)){
                    // Se l'opzione non è riconosciuta, allora questa è invalida
                    fprintf(stderr, "Unknown option '-%c'.\n", optopt);
                } else {
                    // Se l'opzione non è stampabile, allora il valore dell'opzione è invalido
                    fprintf(stderr, "Unknown option character '\\x%x'.\n", optopt);
                }
                exit(1);
            default:
                fprintf(stderr, "Unexpected error");
                exit(1);
        }
    }

    // Se il numero degli argomenti passati è maggiore di optind, allora c'è il nome del file di input
    if(optind < argc){
        data->filename = argv[optind];
    } else {
        // Altrimenti non è stato passato il nome del file di input
        fprintf(stderr, "Expected the name of the input file.\n");
        return NULL;
    }

    // Restituisco la struttura con i dati in input
    return data;
}

// Funzione che stampa i risultati iniziali dopo la lettura del file di input
void output_print_start(int n, int dead_end, int arcs){
    printf("Number of nodes: %d\n", n);
    printf("Number of dead-ends nodes: %d\n", dead_end);
    printf("Number of valid arcs: %d\n", arcs);
}

// Funzione che stampa i risultati finali dopo il calcolo del pagerank
void output_print_end(const int n, const double sum, const int k, const map *pagerank, const bool conv){
    printf("Did ");
    if(!conv){
        printf("not ");
    }
    printf("converge after %d iterations\n", n);
    printf("Sum of ranks: %.4lf\n", sum);
    printf("Top %d nodes:\n", k);

    if (pagerank == NULL)
    {
        printf("No pagerank to print\n");
        return;
    }

    for(int i = 0; i < k; i++)
        printf("%d %lf\n", pagerank[i].index, pagerank[i].val);
}

// Funzione che controlla se un arco è valido
bool valid_arc(const int i, const int j, const int N){
    return i >= 0 && i < N && j >= 0 && j < N && i != j;
}

// Funzione che controlla se un nodo è già presente nella lista degli archi entranti
bool check_duplicate(const struct inmap *in, const int node){
    const struct inmap *aux = in;
    // fprintf(stderr, "Thread %ld: Checking for duplicates in %p\n", pthread_self(), aux);
    while(aux != NULL){
        // fprintf(stderr, "Thread %ld: %p %p %d with node %d (to be compared with %d)\n", pthread_self(), aux, aux->next, NULL != aux, aux->node, node);
        if(aux->node == node){
            return true;
        }
        // fprintf(stderr, "Thread %ld: %p becames %p\n", pthread_self(), aux, aux->next);
        aux = aux->next;
    }
    // fprintf(stderr, "Thread %ld: No duplicates found\n", pthread_self());
    return false;
}

// Lavoratore che elabora gli archi in input
void* thread_read(void *arg){
    // Ottengo i dati passati come argomento
    thread_data_read *data = arg;

    // Contatore per il numero di archi letti
    int count = 0;

    // Struttura per i nuovi archi
    struct inmap *new = NULL;
    // Struttura per i dati da leggere dal buffer
    // ReSharper disable once CppJoinDeclarationAndAssignment
    struct node_read *curr;

    while(true){
        // fprintf(stderr, "Thread %ld: Waiting for arcs\n", pthread_self());

        // Aspetto che ci siano archi da leggere
        sem_wait(data->arcs);

        // Controllo se devo terminare
        if(data->end) break;

        // Aspetto che il buffer sia disponibile
        sem_wait(data->mutex_buffer);
        // Leggo i dati dal buffer
        // ReSharper disable once CppJoinDeclarationAndAssignment
        curr = data->buffer;
        // Rimuovo l'elemento dal buffer
        data->buffer = data->buffer->next;
        // Rilascio il buffer
        sem_post(data->mutex_buffer);

        // Preparo il nuovo arco decrementando i valori di i e j di 1
        new = malloc(sizeof(struct inmap));
        new->node = curr->i - 1;
        new->next = NULL;
        curr->j--;

        // fprintf(stderr, "Thread %ld waiting for graph\n", pthread_self());

        if (!valid_arc(new->node, curr->j, data->g->N))
        {
            free (new);
            exit(1);
        }

        // Controllo se l'arco è valido e se non è duplicato
        if(!check_duplicate(data->g->in[curr->j], new->node)){

            // Aspetto che il grafo sia disponibile
            sem_wait(data->mutex_graph);
            // Se non è il primo arco
            if(data->g->in[curr->j] != NULL)
                // Aggiorno il nuovo arco
                new->next = data->g->in[curr->j];
            // Aggiungo il nuovo arco alla lista degli archi entranti
            data->g->in[curr->j] = new;

            // Aumento il numero di archi uscenti del nodo i
            data->g->out[new->node]++;

            // Rilascio il grafo
            sem_post(data->mutex_graph);

            // Incremento il contatore degli archi letti
            count++;

            // fprintf(stderr, "Thread %ld: count %d\n", pthread_self(), count);
        }else
        {
            // Se l'arco non è valido o è duplicato, lo elimino
            free(new);
        }

        // Libero la memoria
        free(curr);
    }

    // Restituisco il numero di archi letti
    int *ret = malloc(sizeof(int));
    *ret = count;
    pthread_exit(ret);
}

// Funzione che legge il file di input e crea il grafo
grafo read_input(const char *filename, const int t, int *arcs_read){

    // fprintf(stderr, "Reading input file\n");

    // Alloco memoria per il grafo
    grafo g;

    // Apro il file in lettura
    FILE *file = fopen(filename, "r");

    // Controllo se il file è stato aperto correttamente
    if(file == NULL){
        fprintf(stderr, "Error opening file.\n");
        exit(1);
    }

    // Variabile per leggere la riga
    char *line = NULL;
    // La lunghezza della riga non è nota, quindi inizializzo a 0
    size_t len = 0;
    // Variabile per contare le righe lette (escuso i commenti)
    int line_count = 0;

    // Creo i semafori
    sem_t arcs;
    sem_t mutex_buffer;
    sem_t mutex_graph;
    sem_t mutex_arcs;

    // Inizializzo i semafori
    sem_init(&arcs, 0, 0);
    sem_init(&mutex_buffer, 0, 1);
    sem_init(&mutex_graph, 0, 1);
    sem_init(&mutex_arcs, 0, 1);

    // Alloco memoria per la struct che conterrà i dati per i thread
    thread_data_read *data = malloc(sizeof(thread_data_read));

    // Inizializzo la struct
    data->buffer = NULL;
    data->total_arcs = 0;
    data->arcs = &arcs;
    data->mutex_buffer = &mutex_buffer;
    data->mutex_graph = &mutex_graph;
    data->mutex_arcs = &mutex_arcs;
    data->g = &g;
    data->end = false;

    // Creo l'array che conterrà i thread
    pthread_t threads[t];

    // Leggo il file riga per riga
    while(getline(&line, &len, file) != -1){
        // Se la riga è un commento, la salto
        if(line[0] == '%') continue;

        // fprintf(stderr, "Line: %d\n", line_count);

        // Se è la prima riga
        if(!line_count) {
            // Numero di nodi e archi
            int r, c, n;
            sscanf(line, "%d %d %d", &r, &c, &n);

            // Controllo se la matrice è quadrata
            if(r != c){
                fprintf(stderr, "Error: the matrix is not square.\n");
                exit(1);
            }

            // Assegno il numero di nodi al grafo
            data->g->N = r;

            // Imposto il numero massimo di archi
            data->max_arcs = n;

            // Alloco memoria per gli array
            data->g->out = (int*)malloc(data->g->N*sizeof(int));
            data->g->in = (struct inmap**)malloc(data->g->N*sizeof(struct inmap));

            // Inizializzo gli array
            for(int i = 0; i < data->g->N; i++){
                data->g->out[i] = 0;
                data->g->in[i] = NULL;
            }

            // Faccio partire i thread
            for (int i = 0; i < t; i++)
                pthread_create(&threads[i], NULL, thread_read, data);
        }else{
            // Leggo gli indici degli archi
            int i, j;
            sscanf(line, "%d %d", &i, &j);

            // Preparo la struct per i dati da leggere
            struct node_read *new = malloc(sizeof(struct node_read));
            new->i = i;
            new->j = j;
            new->next = NULL;

            // Aspetto che il buffer sia disponibile
            sem_wait(&mutex_buffer);

            // Se il buffer non è vuoto
            if (data->buffer != NULL)
                // Aggiorno il nuovo nodo
                new->next = data->buffer;
            // Aggiorno il buffer
            data->buffer = new;
            // Rilascio il buffer
            sem_post(&mutex_buffer);

            // Incremento il numero di archi da leggere
            sem_post(&arcs);
        }

        // Incremento il contatore delle righe
        line_count++;
    }

    // Chiudo il file
    fclose(file);

    // Aspetto che tutti gli archi siano stati letti
    while (data->buffer != NULL) {}
    // Avviso i thread che non ci sono più archi da leggere
    data->end = true;

    // Sblocco i thread
    for (int i = 0; i < t; i++)
        sem_post(data->arcs);

    // Aspetto che i thread terminino
    for(int i = 0; i < t; i++){
        // Variabile per il valore di ritorno del thread (numero di archi letti)
        void *temp;
        pthread_join(threads[i], &temp);
        *arcs_read += *(int*) temp;
        free(temp);
    }

    // Distruzione dei semafori
    sem_destroy(&arcs);
    sem_destroy(&mutex_buffer);
    sem_destroy(&mutex_graph);
    sem_destroy(&mutex_arcs);

    // Libero la memoria
    free(line);
    free(data);

    // Restituisco il grafo
    return g;
}

// Funzione che inizializza un array di double con un valore
void set_array(double *a, const int n, const double v){
    if (a == NULL)
        return;
    for(int i = 0; i < n; i++){
        a[i] = v;
    }
}

// Funzione che gestisce i segnali
void signal_handler(const int sig){

    // Se il segnale è SIGUSR1
    if (sig == SIGUSR1)
        // Sblocco il thread che aspetta il segnale
        pthread_cond_signal(&signal_cond);
}

// Thread che gestisce il segnale SIGUSR1
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
                    sem_post(data->mutex_S);
                }
                // Decremento il numero di operazioni da eseguire
                sem_post(data->sem_calc);
                break;
            // Calcolo il nuovo pagerank
            case 2:
                // Calcolo la somma dei pagerank dei nodi entranti
                double sum = 0;
                for (const struct inmap *in = data->g->in[new->j]; in != NULL; in = in->next)
                {
                    sum += data->Y[in->node];
                }

                // Calcolo il nuovo pagerank
                data->Xnew[new->j] = (1-data->d)/data->g->N + data->d/data->g->N*data->S + data->d*sum;

                // fprintf(stderr, "Thread %ld: %d %f\n", pthread_self(), new->j, sum);

                // Decremento il numero di operazioni da eseguire
                sem_post(data->sem_calc);
                break;
            // Calcolo l'errore
            case 3:
                // Calcolo l'errore
                const double temp = fabs(data->Xnew[new->j] - data->X[new->j]);

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

    sem_t mutex_X;
    sem_init(&mutex_X, 0, 1);

    sem_t mutex_Y;
    sem_init(&mutex_Y, 0, 1);

    sem_t mutex_Xnew;
    sem_init(&mutex_Xnew, 0, 1);

    sem_t mutex_S;
    sem_init(&mutex_S, 0, 1);

    sem_t sem_g;
    sem_init(&sem_g, 0, 1);

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
    data.mutex_X = &mutex_X;
    data.Y = Y;
    data.mutex_Y = &mutex_Y;
    data.Xnew = Xnew;
    data.mutex_Xnew = &mutex_Xnew;
    data.S = 0;
    data.mutex_S = &mutex_S;
    data.err = 0;
    data.mutex_err = &mutex_err;
    data.g = g;
    data.sem_g = &sem_g;
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

        // Resetto il buffer
        temp_buffer = NULL;

        // Sezione per il calcolo dell'errore
        for (int j=0; j<g->N; j++)
        {
            // Preparo il nodo per il buffer
            struct node_calc *E = malloc(sizeof(struct node_calc));
            E->op = 3;
            E->j = j;

            // Aggiungo il nodo al buffer
            E->next = temp_buffer;
            temp_buffer = E;
        }

        // Aggiorno il buffer
        data.buffer = temp_buffer;

        // Sblocco i thread
        for (int i=0; i<g->N; i++)
            sem_post(&sem_buffer);

        // Aspetto che i thread terminino
        for (int i=0; i<g->N; i++)
            sem_wait(&sem_calc);

        // Calcolo la somma dei pagerank
        double sum = 0;
        for (int i=0; i<g->N; i++)
            sum += Xnew[i];

        /*
        fprintf(stderr, "Iteration %d err %f sum %f\n", iter, data.err, sum);
        for (int i=0; i<g->N; i++)
        {
            fprintf(stderr, "%f ", Xnew[i]);
        }
        fprintf(stderr, "\n");
        */

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

    // Restituisco il vettore dei pagerank
    return X;
}

// Funzione che conta i nodi senza archi uscenti
int dead_end(const grafo *g){
    int count = 0;
    for(int i = 0; i < g->N; i++)
        if(g->out[i] == 0)
            count++;
    return count;
}

// Funzione di confronto per l'ordinamento del vettore dei pagerank
int compare_desc(const void *a, const void *b) {
    const map arg1 = *(const map *)a;
    const map arg2 = *(const map *)b;

    if (arg1.val < arg2.val) return 1;
    if (arg1.val > arg2.val) return -1;
    return 0;
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

    // Leggo il file di input e creo il grafo
    grafo g = read_input(data->filename, data->t, &arcs_read);

    // gettimeofday(&t1, NULL);
    // timersub(&t1, &t0, &dt);
    // fprintf(stderr, "Time to read input: %ld.%06ld\n", dt.tv_sec, dt.tv_usec);

    // Stampo i risultati iniziali
    output_print_start(g.N, dead_end(&g), arcs_read);

    // Variabile per il numero di iterazioni
    int numiter = -1;

    // gettimeofday(&t0, NULL);

    // Calcolo il pagerank
    double *res = pagerank(&g, data->d, data->e, data->m, data->t, &numiter);

    // gettimeofday(&t1, NULL);
    // timersub(&t1, &t0, &dt);
    // fprintf(stderr, "Time to calculate pagerank: %ld.%06ld\n", dt.tv_sec, dt.tv_usec);

    // Creo un vettore di struct per tenere traccia del pagerank con l'indice del nodo
    map* map_res = malloc(g.N * sizeof(map));
    // Inizializzo il vettore
    for (int i=0; i<g.N; i++)
    {
        map_res[i].val = res[i];
        map_res[i].index = i;
    }

    // Ordino il vettore in ordine decrescente
    qsort(map_res, g.N, sizeof(map), compare_desc);

    // Calcolo la somma dei pagerank
    double sum = 0;
    for (int i=0; i<g.N; i++)
        sum += res[i];

    // Stampo i risultati finali
    output_print_end(numiter, sum, data->k, map_res, true);

    // Libero la memoria
    free(map_res);
    free(data);
    free(res);
    for (int i=0; i<g.N; i++)
    {
        struct inmap *curr = g.in[i];
        while (curr != NULL)
        {
            struct inmap *temp = curr;
            curr = curr->next;
            free(temp);
        }
    }
    free(g.in);
    free(g.out);

    // Termino il programma
    return 0;
}