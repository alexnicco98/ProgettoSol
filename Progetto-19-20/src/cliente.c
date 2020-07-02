#define _POSIX_C_SOURCE  200112L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include "cliente.h"
#include "cassiere.h"
#include "direttore.h"
#include "utility.h"

_Thread_local unsigned int seed;        // seed thread local for rand_r

// insert the client in cashier's tail of that index
void insertInCashiersQueue(queue *client, int index, var_struct *vcm, threadCassiere *cashiers){
    // generate a random number that corresponds at one cashier open
    int reg = rand_r(&seed)%( vcm->openCashiers);
    #if defined(DEBUG)
    printf("[Client %d] mi metto in coda nella cassa %d\n",index,  reg+1);
    #endif
    pthread_mutex_lock(&(cashiers[reg].lock));
    cashiers[reg].queueSize++;
    #if defined(DEBUG)
    printf("lunghezza coda: %d, cassa: %d\n", cashiers[reg].queueSize, reg+1);
    #endif
    insertClient(&(cashiers[reg].head), &(cashiers[reg].tail), client);
    pthread_cond_signal(&(cashiers[reg].notEmpty));
    pthread_mutex_unlock(&(cashiers[reg].lock));
}

// life cycle of client
void *client(void *arg) {
    struct var_cashier_client *var = arg;
    var_struct *vcm = var->vcm;
    threadCassiere *cashiers = var->regs;
    int tBuy, index = var->index, pBuy, cQueue, oldType;
    float diff, sTime, qTime;
    queue client = malloc(sizeof(nodoC));
    struct timespec t1;
    struct timespec queueStartTime;
    struct timespec queueEndTime;
    pthread_detach(pthread_self());
    if( pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldType) != 0){
        perror("Error setcanceltype");
        exit(EXIT_FAILURE);
    }
    seed = (unsigned int)pthread_self()*100000;

    // block signals
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGQUIT);
    if( pthread_sigmask(SIG_BLOCK, &set, NULL) != 0){
        perror("Error pthread sigmask");
        exit(EXIT_FAILURE);
    }

    client->served = 0;
    client->queueTime = -1;
    client->changedQueue = 0;
    client->next = NULL;
    client->serviceTime = 0;
    client->supermarketTime = 0;
    if( pthread_mutex_init(&(client->lock), NULL) != 0){
        perror("Error initialize lock");
        exit(EXIT_FAILURE);
    }
    if( pthread_cond_init(&(client->myTurn), NULL) != 0){
        perror("Error initialize var cond");
        exit(EXIT_FAILURE);
    }
    if( pthread_cond_init(&(client->modified), NULL) != 0){
        perror("Error initialize var cond");
        exit(EXIT_FAILURE);
    }

    srand(seed);
    // time for buy products
    client->buyTime = (float)(rand_r(&seed)%( (T + 1) - 10) + 10) / 1000;
    tBuy = client->buyTime * 1000000000;
    if(tBuy >= 1000000000 ){
        t1.tv_sec = tBuy / 1000000000;
        t1.tv_nsec = (tBuy % 1000000000);
    }
    else{
        t1.tv_sec = 0;
        t1.tv_nsec = tBuy;
    }

    // simulate time for buy products
    nanosleep(&t1, NULL);
    client->buyProducts = rand_r(&seed)%(P + 1);
    pBuy = client->buyProducts;

    // check client's buyProducts, if is 0 not insert in queue but
    // wait the manager for leave, otherwise insert in cashier's queue
    if( client->buyProducts != 0){
        pthread_mutex_lock(&(vcm->mtx));
        // take start time of queue client
        clock_gettime(CLOCK_MONOTONIC, &queueStartTime);
        insertInCashiersQueue(&client, index, vcm, cashiers);
        pthread_mutex_unlock(&(vcm->mtx));

        pthread_mutex_lock(&(client->lock));
        // Wait client turn
        while( client->served == 0)
            pthread_cond_wait(&(client->myTurn), &(client->lock));

        #if defined(DEBUG)
        printf("[Client %d] è il mio turno\n", index);
        #endif
        // take end time of queue client
        clock_gettime(CLOCK_MONOTONIC, &queueEndTime);

        diff = (queueEndTime.tv_sec - queueStartTime.tv_sec) ;
        if( (queueEndTime.tv_nsec - queueStartTime.tv_nsec) <0)
            diff += (float)( queueEndTime.tv_nsec -queueStartTime.tv_nsec)* (-1) / 1000000000;
        else
            diff += (float)(queueEndTime.tv_nsec -queueStartTime.tv_nsec) / 1000000000;
        client->queueTime = diff;
        // signal to cashier the changed
        pthread_cond_signal(&(client->modified));
        pthread_mutex_unlock(&(client->lock));

        pthread_mutex_lock(&(client->lock));
        // wait that cashier calculate value of client
        while( client->supermarketTime == 0)
            pthread_cond_wait(&(client->modified), &(client->lock));

    }else{
        pthread_mutex_lock(&(client->lock));
        #if defined(DEBUG)
        printf("[Cliente %d] ho acquistato 0 prodotti, chiedo il permesso di uscire\n", index);
        #endif
        client->queueTime = 0;
        client->serviceTime = 0;
        client->supermarketTime = client->buyTime;
    }

    pBuy = client->buyProducts;
    cQueue = client->changedQueue;
    sTime = client->supermarketTime;
    qTime = client->queueTime;
    pthread_mutex_unlock(&(client->lock));
    updateInfoClients(pthread_self(), pBuy, cQueue, sTime, qTime);
    fflush(stdout);
    // ask to manager to exit
    pthread_mutex_lock(&(vcm->mtx));

    vcm->numClient--;
    clientExit(index);
    printf("Il cliente %d è uscito, clienti dentro il supermercato: %d\n ", index, vcm->numClient);
    // the last client in the supermarket
    if(vcm->numClient == 0){
        #if defined(DEBUG)
        printf("SEGNALE ULTIMO CLIENT MANDATO\n");
        #endif
        pthread_cond_signal(&(vcm->NotFullClient));
        pthread_cond_signal(&(vcm->Empty));
        pthread_mutex_unlock(&(vcm->mtx));
        free(client);
        return NULL;
    }
    // signal to manager that the threashold has been reached
    if((vcm->numClient < (C - E)) && (vcm->finish == 0)){
        pthread_cond_signal(&(vcm->NotFullClient));
        pthread_mutex_unlock(&(vcm->mtx));
        free(client);
        return NULL;
    }
    pthread_mutex_unlock(&(vcm->mtx));
    free(client);
    return NULL;
}
