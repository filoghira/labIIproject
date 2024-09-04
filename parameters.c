#include "structures.h"
#include "parameters.h"
#include "error.h"
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <ctype.h>
#include <string.h>

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
                termina("Errore nella lettura dei parametri forniti");
        }
    }

    // Se il numero degli argomenti passati è maggiore di optind, allora c'è il nome del file di input
    if(optind < argc){
        // Controllo il formato del file
        const char* extension = strrchr(argv[optind], '.');
        if (strcmp(extension, ".mtx") != 0)
            termina("Formato del file non valido");
        data->filename = argv[optind];
    } else {
        // Altrimenti non è stato passato il nome del file di input
        termina("File di input non fornito");
    }

    // Restituisco la struttura con i dati in input
    return data;
}
