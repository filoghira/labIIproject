# Progetto di Laboratorio II
#### Filippo Ghirardini - 654829 - AA 2023/2024

## Parallelizzazione del calcolo di Pagerank

Ho definito un thread singolo ([pagerank.c](pagerank.c)) che gestisce due fasi:

1. Calcolo della somma parziale **S** e del vettore **Y** all'iterazione t
2. Calcolo della componente j del vettore **newX** (compreso di copia di **newX** in **X**) e dell'errore parziale 
all'iterazione t

Il MainThread ad ogni iterazione carica il buffer con dei _batch_, ovvero dei range di nodi che spettano da calcolare
ad ogni thread. Il buffer è gestito tramite produttori e consumatori su un buffer di dimensione limitata.
Mentre il buffer viene caricato i thread iniziano ad elaborare i dati. Finito il calcolo per la prima fase da parte di
tutti i thread, si calcola la somma totale **S**. 
Successivamente si fa la stessa cosa per la fase 2.

## Parallelizzazione della lettura del file di input
Analogamente al calcolo, con la differenza che il batch non è un semplice range ma contiene un insieme di archi
da elaborare. Inoltre per evitare racing contition, ogni thread ha un proprio grafo in cui inserisce gli archi;
al termine di ogni thread, eseguo un merge ordinato di tutti i grafi.
Infine la eseguo la rimozione dei duplicati (che è il motivo per cui eseguo gli inserimenti ordinati).

## Parallelizzazione del Client-Server
### Server
Il server crea un Pool di thread. Ogni volta che viene accettata una connessione, ne fa partire uno passandogliela.
Nel momento in cui riceve un segnale SIGUSR1, aspetta che ogni thread della pool finisca e poi termina.
### Client
Il client crea un Pool di thread. Poi, per ogni file ricevuto in input, fa partire il thread che per conto suo 
stabilirà la connessione con il server. Una volta creati tutti i thread si metterà in attesa che terminino.