//#define _POSIX_C_SOURCE  200112L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include "cliente.h"
#include "cassiere.h"
#include "direttore.h"
#include "utility.h"

//volatile_sig_atomic_t notFinish = 1;

// insert the cleint in cashier's tail
void insertInCashiersQueue(queue *client, var_client_manager *vcm, threadCassiere *cashiers){
    // generate a random number that corresponds at one cashier open
    int reg = rand()%( vcm->openCashiers);

    //printf("mi metto in coda nella cassa %d\n", reg+1);
    pthread_mutex_lock(&(cashiers[reg].lock));
    cashiers[reg].queueSize++;
    printf("lunghezza coda: %d, cassa: %d\n", cashiers[reg].queueSize, reg+1);
    insertClient(&(cashiers[reg].head), &(cashiers[reg].tail), client);
    pthread_cond_signal(&(cashiers[reg].notEmpty));
    pthread_mutex_unlock(&(cashiers[reg].lock));
}

void *client(void *arg) {
    struct vcmAndCashiers *var = arg;
    var_client_manager *vcm = var->vcm;
    threadCassiere *cashiers = var->regs;
    int tBuy, s, index = var->index;
    double diff;
    queue client = malloc(sizeof(nodoC));
    struct timespec t1;
    struct timespec queueStartTime;
    struct timespec queueEndTime;
    pthread_detach(pthread_self());

    //printf("[Client] numero indice %d\n",index);
    // block all signals
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGALRM);
    if( (s = pthread_sigmask(SIG_SETMASK, &set, NULL)) != 0 ){
        perror("Error pthread_simask ");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));
    tBuy = (rand()%( (T + 1) - 10) + 10) * 1000000;
    if(tBuy >= 1000000000 ){
        t1.tv_sec = tBuy / 1000000000;
        t1.tv_nsec = (tBuy % 1000000000);
    }
    else{
        t1.tv_sec = 0;
        t1.tv_nsec = tBuy;
    }

    client->buyTime = tBuy / 1000000;

    nanosleep(&t1, NULL);
    client->buyProducts = rand()%(P + 1);
    client->served = 0;
    if( pthread_mutex_init(&(client->lock), NULL) != 0){
        perror("Error initialize lock");
        exit(EXIT_FAILURE);
    }
    if( pthread_cond_init(&(client->myTurn), NULL) != 0){
        perror("Error initialize var cond");
        exit(EXIT_FAILURE);
    }
    client->next = NULL;
    //printf("Ho comprato %d prodotti, sono il cliente %ld\n", client->nProducts, pthread_self());

    // in this case i use the function for assign
    // the cashier at client
    if( client->buyProducts != 0){
        pthread_mutex_lock(&(vcm->mtx));
        clock_gettime(CLOCK_MONOTONIC, &queueStartTime);
        insertInCashiersQueue(&client, vcm, cashiers);
        pthread_mutex_unlock(&(vcm->mtx));

        pthread_mutex_lock(&(client->lock));
        while( client->served == 0)
            pthread_cond_wait(&(client->myTurn), &(client->lock));

        clock_gettime(CLOCK_MONOTONIC, &queueEndTime);
        // chiedo il permesso di uscire al direttore
        diff = (queueEndTime.tv_sec - queueStartTime.tv_sec) * 1000;
        diff += ( queueEndTime.tv_nsec -queueStartTime.tv_nsec) / 1000000;
        //printf("        Tempo in coda %d\n", (int)diff);
        client->queueTime = (int)diff;
        pthread_mutex_unlock(&(client->lock));
    }else{
        pthread_mutex_lock(&(client->lock));
        //printf("Ho acquistato 0 prodotti, esco\n");
        client->queueTime = 0;
        pthread_mutex_unlock(&(client->lock));
    }


    pthread_mutex_lock(&(vcm->mtx));
    vcm->numClient--;
    clientExit(index);
    if(vcm->numClient < (C - E))
        pthread_cond_signal(&(vcm->NotFullClient));
    printf("Il cliente %d è uscito, clienti dentro il supermercato: %d\n ", index, vcm->numClient);
    pthread_mutex_unlock(&(vcm->mtx));

    //free(vcm);
    //free(cashiers);
    //free(var);
    free(client);

    // terminate
    //pthread_exit(NULL);
    return NULL;
}
