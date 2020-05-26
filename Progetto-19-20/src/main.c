//  Created by Alessandro Niccolini on 06/05/20.
//  Copyright © 2020 Alessandro Niccolini. All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include "direttore.h"
#include "utility.h"

//define K 4   // numero di casse del supermercato
//define C 20  // numero massimo di persone all'interno del supermercato
//define T 200 // T tempo variabile speso dai clienti per fare acquisti
//define E 5   // se il numero di clienti all'interno del supermercato
              // scende sotto di E, ne vengono fatti entrare altri E
//define P 6   // numero massimo di articoli acquistati per cliente
//define S1 2  // soglia per cui se S1 casse hanno al massimo un
              // cliente una viene chiusa
//define S2 7  // soglia per cui se almeno una cassa ha S2 clienti in coda
              // apre una nuova cassa (se possibile)

#define bufzise 128
#define namesize 32

pthread_mutex_t Mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t NotFullClient = PTHREAD_COND_INITIALIZER;
pthread_cond_t Empty = PTHREAD_COND_INITIALIZER;
int numClient = 0;
int K, P, T, E, C, S1, S2, productTime, intervalTime;

void loadFile(char *config, char *log){
    char *line = NULL, *word, *value;
    FILE *f;
    size_t len = 0;
    ssize_t r;

    f = fopen(config, "r");
    if(f == NULL){
        perror("Error fopen");
        exit(EXIT_FAILURE);
    }

    while( (r = getline(&line , &len, f)) != -1){
        word = strtok(line, " ");
        value = strtok(NULL, " ");

        if(strcmp(word, "K") == 0 ){
            K = (int) atol(value);
            continue;
        }
        if(strcmp(word, "P") == 0 ){
            P = (int) atol(value);
            continue;
        }
        if(strcmp(word, "T") == 0 ){
            T = (int) atol(value);
            continue;
        }
        if(strcmp(word, "E") == 0 ){
            E = (int) atol(value);
            continue;
        }
        if(strcmp(word, "C") == 0 ){
            C = (int) atol(value);
            continue;
        }
        if(strcmp(word, "S1") == 0 ){
            S1 = (int) atol(value);
            continue;
        }
        if(strcmp(word, "S2") == 0 ){
            S2 = (int) atol(value);
            continue;
        }
        if(strcmp(word, "logfile") == 0 ){
            strcpy(log, value);
            continue;
        }
        if(strcmp(word, "product_time") == 0 ){
            productTime = (int) atol(value);
            continue;
        }

        if(strcmp(word, "interval_time") == 0 ){
            intervalTime= (int) atol(value);
            continue;
        }

        printf("Stringa %s non roconosciuta\n", word);
        exit(EXIT_FAILURE);
    }
    //free(filename);
    free(word);
    //free(value);

    fclose(f);

}

int main(int argc, const char * argv[]) {
    var_client_manager *vcm = malloc(sizeof(var_client_manager));
    char config[namesize], log[namesize];
    int status;
    sigset_t set;
    FILE *f;
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGHUP);
    if( pthread_sigmask(SIG_SETMASK, &set, NULL) != 0 ){
        perror("Error pthread_simask ");
        exit(EXIT_FAILURE);
    }

    strcpy(config, "config.txt");
    loadFile(config, log);
    vcm->numClient = C; // all'inizio entrano tutti i clienti
    vcm->finish = 0;
    vcm->NotFullClient = NotFullClient;
    vcm->Empty = Empty;
    vcm->mtx = Mtx;

    pthread_t supermercato;
    pthread_create(&supermercato, NULL, &direttore, (void*) vcm);

    //pthread_detach(supermercato);
    printf("      PID SUPERMARKET: %ld\n", supermercato);
    f = fopen(log, "w+");
    fprintf(f, "%ld", supermercato);
    fclose(f);
    //printf("T: %d\n", vcm->t);
    pthread_join(supermercato,(void*) &status);
    printf("Stato di terminazione del manager: %d\n", status);
    free(vcm);
    //pthread_exit(0);
    return 0;
}
