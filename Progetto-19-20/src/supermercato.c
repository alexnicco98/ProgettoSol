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

//define K    // numero di casse del supermercato
//define C    // numero massimo di persone all'interno del supermercato
//define T    // T tempo variabile speso dai clienti per fare acquisti
//define E    // se il numero di clienti all'interno del supermercato
              // scende sotto di E, ne vengono fatti entrare altri E
//define P    // numero massimo di articoli acquistati per cliente
//define S1   // soglia per cui se S1 casse hanno al massimo un
              // cliente una viene chiusa (una rimane sempre aperta)
//define S2   // soglia per cui se almeno una cassa ha S2 clienti in coda
              // apre una nuova cassa (se possibile)

#define bufzise 128
#define namesize 32

pthread_mutex_t Mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t MtxInfo = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t NotFullClient = PTHREAD_COND_INITIALIZER;
pthread_cond_t Empty = PTHREAD_COND_INITIALIZER;
int numClient = 0;
int K, P, T, E, C, S1, S2, productTime, intervalTime, openCas;
infoQueueClient iqClientHead;
infoQueueClient iqClientTail;
infoQueueCashier *iqCashier;

// load the config file for the start info
void loadInputFile(char *config, char *log){
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
            int n = strlen(value);
            char *fchar = strpbrk( value, "\n");
            strcpy(fchar, "");
            strncpy(log, value, n);
            continue;
        }
        if(strcmp(word, "product_time") == 0 ){
            productTime = (int) atol(value);
            continue;
        }

        if(strcmp(word, "interval_time") == 0 ){
            intervalTime = (int) atol(value);
            continue;
        }
        if( strcmp(word, "open_cashiers") == 0){
            openCas = (int) atol(value);
            continue;
        }

        printf("Stringa %s non roconosciuta\n", word);
        exit(EXIT_FAILURE);
    }

    free(word);
    fclose(f);

}

// load final info in the log file, if sigquitReceived is equal to 1
// don't load the cashier info in the log file, otherwise yes
void loadOutputFile(char* log, int sigquitReceived){
    int i;
    FILE *f;

    f = fopen(log, "w+");
    if(f == NULL){
        perror("Error fopen");
        exit(EXIT_FAILURE);
    }

    while(iqClientHead != NULL){
        fprintf(f, "[Cliente] id %ld pBuy %d supermarketTime %0.3fs QueueTime %0.3fs changedQueue %d\n"
        , iqClientHead->idClient, iqClientHead->buyProducts, iqClientHead->supermarketTime, iqClientHead->queueTime, iqClientHead->changedQueue );
        infoQueueClient aux = iqClientHead;
        iqClientHead = iqClientHead->next;
        free(aux);
    }
    iqClientTail = NULL;

    for(i=0; i<K; i++){
        if( sigquitReceived != 1){
            fprintf(f, "[Cassa] id %d processedProducts %d clients %d openTime %0.3fs averangeTimeService %0.3fs closures %d\n"
            , iqCashier[i]->idCashier, iqCashier[i]->processedProducts, iqCashier[i]->clientServed, iqCashier[i]->openTime, iqCashier[i]->averangeTimeService
            , iqCashier[i]->numberClosures);
        }
        free(iqCashier[i]);
    }
    free(iqCashier);
    fclose(f);
    #if defined(DEBUG)
    printf("Dati raccolti durante il sistema, caricati nel file di log\n");
    #endif
}

// supermarket
int main(int argc, const char * argv[]) {
    var_struct *vcm = malloc(sizeof(var_struct));
    struct var_manager *vcmM = malloc(sizeof(struct var_manager));
    char config[namesize], log[namesize];
    int i, status;

    strcpy(config, "config.txt");
    loadInputFile(config, log);
    vcmM->iqClientHead = iqClientHead;
    vcmM->iqClientTail = iqClientTail;
    iqCashier = malloc(sizeof(nodoInfoCashier)*K);
    for(i=0; i<K; i++){
        iqCashier[i] = malloc(sizeof(nodoInfoCashier));
        iqCashier[i]->idCashier = i+1;
        iqCashier[i]->processedProducts = 0;
        iqCashier[i]->clientServed = 0;
        iqCashier[i]->numberClosures = 0;
        iqCashier[i]->openTime = 0;
        iqCashier[i]->averangeTimeService = 0;
    }
    vcm->numClient = C; // at start all clients enter in the supermarket
    vcm->openCashiers = openCas;
    vcm->finish = 0;
    vcm->sigquitReceived = 0;
    vcm->NotFullClient = NotFullClient;
    vcm->Empty = Empty;
    vcm->mtx = Mtx;
    vcmM->iqCashier = iqCashier;
    vcmM->vcm = vcm;

    // create the manager
    pthread_t manager;
    pthread_create(&manager, NULL, &direttore, (void*) vcmM);

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGQUIT);
    if( pthread_sigmask(SIG_BLOCK, &set, NULL) != 0){
        perror("Error pthread sigmask");
        exit(EXIT_FAILURE);
    }

    sleep(25);
    pthread_kill(manager, SIGHUP);
    #if defined(DEBUG)
    printf("\n\n        mandato SIGHUP\n\n");
    #endif
    pthread_join(manager,(void*) &status);

    loadOutputFile(log, vcm->sigquitReceived);

    free(vcm);
    free(vcmM);
    return 0;
}
