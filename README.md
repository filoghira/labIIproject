# Progetto di Laboratorio II
#### Filippo Ghirardini - 654829 - AA 2023/2024

## Parallelizzazione del calcolo di Pagerank

Ho definito un thread singolo ([pagerank.c](pagerank.c)) che gestisce i 4 tipi di operazione:

1. Calcolo della somma S all'iterazione t
   - Calcolo del vettore Y all'iterazione t
2. Calcolo della componente j del vettore newX all'iterazione t
3. Calcolo dell'errore all'iterazione t

Si noti che la prima e la seconda operazione sono eseguibili contemporaneamente.

Ho quindi definito una lista di [nodi](structures.h) che contengono il codice dell'operazione (da 1 a 3), l'indice j 
della componente su cui eventualmente devo lavorare (solo per op. 2) e il puntatore al nodo successivo.

Il thread principale ad ogni iterazione, si compone di tre sezioni, ognuna funzionante allo stesso modo:
sfrutta un buffer (lista) al quale aggiunge le varie componenti da elaborare per la fase corrente. Dopo aver riempito il
buffer fa partire i thread, incrementando il semaforo sem_buffer N volte. Si mette poi in attesa che il lavoro sia 
completato (esegue una post sul semaforo sem_calc N volte, che a sua volta è incrementato dal thread appena finisce una
unità di lavoro).

La scelta di caricare il buffer anticipatamente e poi di far partire i calcoli è stata effettuate perché altrimenti 
sarebbe stata necessaria una sincronizzazione aggiuntiva che avrebbe notevolmente rallentato i thread.

Alla fine di ogni iterazione controllo se l'errore minimo è stato superato. In quel caso esco dal ciclo, sblocco i
thread e attendo che terminino.
## Parallelizzazione del Client-Server
### Server
Il server crea un Pool di thread. Ogni volta che viene accettata una connessione, ne fa partire uno passandogliela.
Nel momento in cui riceve un segnale SIGUSR1, aspetta che ogni thread della pool finisca e poi termina.
### Client
Il client crea un Pool di thread. Poi, per ogni file ricevuto in input, fa partire il thread che per conto suo 
stabilirà la connessione con il server. Una volta creati tutti i thread si metterà in attesa che terminino.