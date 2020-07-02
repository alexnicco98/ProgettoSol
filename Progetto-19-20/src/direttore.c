#define _POSIX_C_SOURCE  200112L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "direttore.h"
#include "cliente.h"
#include "cassiere.h"
#include "utility.h"

int *queueSize;
int threshold1 , threshold2;
int count = 0;
volatile sig_atomic_t notFinishManager = 1;     // 1 if supermarket is open
volatile sig_atomic_t sigquitReceived;          // 1 if sigquit received
threadCassiere *cassieri;
var_struct *vcm;
struct var_cashier_client *var;
pthread_t *threadCashiers;                      // pthread_t array of cashiers
pthread_t *threadClients;                       // pthread_t array of clients
infoQueueClient iqClientHead;
infoQueueClient iqClientTail;
infoQueueCashier *iqCashier;

// move the client in the cashier's queue of other cashier
// randomly, index represent the cashier that is closing
void moveClient(int index){
    queue client;
    queue aux = cassieri[index].head;
    while( aux != NULL){
        client = aux;
        client->next = NULL;
        client->changedQueue += 1;
        insertInCashiersQueue(&client, index, vcm, cassieri);

        aux = aux->next;
    }
    cassieri[index].head = NULL;
    cassieri[index].tail = NULL;
    #if defined(DEBUG)
    printf("    [Direttore] i clienti del cassiere %d sono stati spostati\n", index+1);
    #endif
}

// close the cashier with bigger index, and move the clients
// in the queue of other cashiers. If numbers of cashiers are > 1
void closeCashier(){
    static int index, status;
    pthread_mutex_lock(&(vcm->mtx));
    if( vcm->openCashiers == 1){
        pthread_mutex_unlock(&(vcm->mtx));
        return;
    }
    vcm->openCashiers--;
    index = vcm->openCashiers;

    // block the cashiers that must close
    pthread_mutex_lock(&(cassieri[index].lock));
    moveClient(index);
    cassieri[index].notFinish = 0;
    pthread_mutex_unlock(&(vcm->mtx));
    pthread_mutex_unlock(&(cassieri[index].lock));

    pthread_cancel(cassieri[index].sup);

    pthread_mutex_lock(&(cassieri[index].lock));
    pthread_cond_signal(&(cassieri[index].notEmpty));
    pthread_mutex_unlock(&(cassieri[index].lock));
    pthread_join(cassieri[index].reg, (void*)&status);

    printf("    [Direttore] cassiere %d chiuso\n", index+1);



}

// open the cashier with smaller index, if
// numbers of cashiers are < K
void openCashier(){
    int index;
    pthread_mutex_lock(&(vcm->mtx));
    if(vcm->openCashiers == K){
        pthread_mutex_unlock(&(vcm->mtx));
        return;
    }
    vcm->openCashiers ++;
    index = vcm->openCashiers -1 ;

    cassiere_create(&cassieri[index], productTime, intervalTime, index);
    if( pthread_create(&threadCashiers[index], NULL, &cassiere_work,(void*) &(cassieri[index])) != 0){
        perror("Error create cashier ");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&(vcm->mtx));

    printf("    [Direttore] cassiere %d aperto\n", index+1);


}

// check the thresholds, and if the values are not respected
// open or close the cashiers
void checkCashiers(){
    static int numRegOneClient = 0, numCashiers;
    pthread_mutex_lock(&(vcm->mtx));
    numCashiers = vcm->openCashiers;
    pthread_mutex_unlock(&(vcm->mtx));

    for(int i=0; i<numCashiers; i++){
        if( queueSize[i] <= 1 )
            numRegOneClient++;
        if(S1 == numRegOneClient){
            closeCashier();
            return;
        }
        if( queueSize[i] >= S2){
            openCashier();
            return;
        }
    }
}

// this function is used by cashier to notify the manager
void updateCashiers(int qSize, int index){
    pthread_mutex_lock(&(vcm->mtx));
    count++;
    queueSize[index] = qSize;

    if( vcm->openCashiers == count && !vcm->finish ){
        count = 0;
        pthread_mutex_unlock(&(vcm->mtx));
        checkCashiers();
        return;
    }

    pthread_mutex_unlock(&(vcm->mtx));
}

// handler for SIGQUIT; Terminate all clients and cashiers immediatly
void handlerSigQuit(int sig){
    write(1, "catturato SIGQUIT\n", 20);
    notFinishManager = 0;
    vcm->sigquitReceived = 1;
    vcm->finish = 1;
}

// handler for SIGHUP; Wait the termination of clients inside the
// supermarket, after close the cashiers
void handlerSigHup(int sig){
    write(1, "catturato SIGHUP\n", 20);
    notFinishManager = 0;
    vcm->finish = 1;
}

// clean pthread_t of client with this index
void clientExit(int index){
    threadClients[index] = 0;
}

// wait client to terminate
void waitClient(){
    #if defined(DEBUG)
    printf("Entrato nella waitClient\n");
    #endif
    pthread_mutex_lock(&(vcm->mtx));
    #if defined(DEBUG)
    printf("    Devo far uscire %d clients\n", vcm->numClient);
    #endif

    if(vcm->numClient != 0){
        while(vcm->numClient != 0){
            pthread_cond_wait(&(vcm->Empty), &(vcm->mtx));
        }
    }else{
        #if defined(DEBUG)
        printf("    [Direttore] Non ho clienti nel supermercato esco\n");
        #endif
    }
    pthread_mutex_unlock(&(vcm->mtx));
}

void updateInfoClients(pthread_t idClient, int buyProducts, int changedQueue, float supermarketTime, float queueTime) {
    pthread_mutex_lock(&(vcm->mtx));
    infoQueueClient new = malloc(sizeof(nodoInfoClient));
    new->idClient = idClient;
    new->buyProducts = buyProducts;
    new->changedQueue = changedQueue;
    new->supermarketTime = supermarketTime;
    new->queueTime = queueTime;
    new->next = NULL;
    if(iqClientHead == NULL){
        iqClientHead = new;
        iqClientTail = new;
    }
    else{
        iqClientTail->next = new;
        iqClientTail = new;
    }
    pthread_mutex_unlock(&(vcm->mtx));
}

void updateInfoCashiers(int index, int processedProducts, int clientServed, float openTime, float averangeTimeService){
    pthread_mutex_lock(&(vcm->mtx));
    iqCashier[index]->processedProducts += processedProducts;
    iqCashier[index]->clientServed += clientServed;
    iqCashier[index]->numberClosures++;
    iqCashier[index]->openTime += openTime;
    iqCashier[index]->averangeTimeService += averangeTimeService;

    pthread_mutex_unlock(&(vcm->mtx));
}

// life cycle of manager
void *direttore(void *arg){
    struct var_manager *vcmM = arg;
    vcm = vcmM->vcm;
    iqCashier = vcmM->iqCashier;
    iqClientHead = vcmM->iqClientHead;
    iqClientTail = vcmM->iqClientTail;
    static int i;
    int status, thresholdC = C-E, add = 0, numCashiers;

    queueSize = malloc(sizeof(int)*K);
    for(i=0; i<K; i++)
        queueSize[i] = 0;
    cassieri = malloc(sizeof(threadCassiere)*K);
    var = malloc(sizeof(struct var_cashier_client)*C);

    struct sigaction sa;
    sigaction(SIGQUIT, NULL, &sa);
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handlerSigQuit;
    sigaction(SIGQUIT, &sa, NULL);

    sa.sa_handler = handlerSigHup;
    sigaction(SIGHUP, &sa, NULL);


    // at start open a number of cashiers define in the config file
    pthread_mutex_lock(&(vcm->mtx));
    printf("    [Direttore] cassieri aperti all'inizio: %d\n", vcm->openCashiers);
    pthread_mutex_unlock(&(vcm->mtx));

    // create first cashiers
    threadCashiers = malloc(sizeof(pthread_t)*K);
    threadClients = malloc(sizeof(pthread_t)*C);
    for( i=0; i<vcm->openCashiers; i++){
        cassiere_create(&cassieri[i], productTime, intervalTime, i);
        if( pthread_create(&threadCashiers[i], NULL, &cassiere_work,(void*) &(cassieri[i])) != 0){
            perror("    Error create register ");
            exit(EXIT_FAILURE);
        }
    }


    // create first C clients
    for( i=0; i<C; i++){
        var[i].vcm = vcm;
        var[i].regs = cassieri;
        var[i].index = i;
        if( pthread_create(&threadClients[i], NULL, &client, (void*) &var[i]) != 0){
            perror("    Error create client ");
            exit(EXIT_FAILURE);
        }
    }

    // cycle for insert new client in the supermarket when
    // C-E clients are exit
    while(notFinishManager){
        fflush(stdout);
        pthread_mutex_lock(&(vcm->mtx));

        // if numbers of client inside supermarket is less
        // than numbers of threshold then add new E client
        while( vcm->numClient > thresholdC)
            pthread_cond_wait(&(vcm->NotFullClient), &(vcm->mtx));

        if( !notFinishManager ){
            pthread_mutex_unlock(&(vcm->mtx));
            continue;
        }
        printf("    [Direttore] creo %d nuovi clienti\n", E);
        vcm->numClient += E;
        i = 0;
        while(add < E && i<C){
            if(threadClients[i] != 0){
                i++;
                continue;
            }
            var[i].vcm = vcm;
            var[i].regs = cassieri;
            var[i].index = i;
            add++;
            if( pthread_create(&threadClients[i], NULL, &client, (void*) &var[i]) != 0){
                perror("    Error create client ");
                exit(EXIT_FAILURE);
            }
            i++;
        }
        if(add != E){
            perror("    Error create new client");
            exit(EXIT_FAILURE);
        }
        add = 0;

        pthread_mutex_unlock(&(vcm->mtx));
    }

    // if i received a signal SIGQUIT i must close the clients
    // and the  cashiers as fast as possible
    if( vcm->sigquitReceived ){
        pthread_mutex_lock(&(vcm->mtx));
        for(i=0; i<C; i++){
            pthread_cancel(threadClients[i]);
        }
        for(i=0; i<vcm->openCashiers; i++){
            cassieri[i].notFinish = 0;
            pthread_cancel(cassieri[i].sup);
            pthread_cancel(threadCashiers[i]);
        }
        pthread_mutex_unlock(&(vcm->mtx));
        #if defined(DEBUG)
        printf("    [Direttore] Ho fatto uscire tutti i clienti e chiuso i cassieri\n");
        #endif
        return NULL;
    }

    // wait all clients
    waitClient();

    pthread_mutex_lock(&(vcm->mtx));
    printf("    [Direttore] Persone ancora nel supermercato: %d, cassieri aperti %d\n", vcm->numClient, vcm->openCashiers);
    numCashiers = vcm->openCashiers;
    // close all open cashiers
    pthread_mutex_unlock(&(vcm->mtx));
    for(i=0; i<numCashiers; i++){
        pthread_mutex_lock(&(cassieri[i].lock));
        cassieri[i].notFinish = 0;
        pthread_mutex_unlock(&(cassieri[i].lock));
        pthread_cancel(cassieri[i].sup);
        pthread_cond_signal(&(cassieri[i].notEmpty));
        pthread_join(threadCashiers[i], (void*)&status);

    }
    // update info cashiers
    for(i=0; i<K; i++){
        if(iqCashier[i]->clientServed != 0)
            iqCashier[i]->averangeTimeService = (float)iqCashier[i]->averangeTimeService / iqCashier[i]->clientServed;
    }
    #if defined(DEBUG)
    printf("    [Direttore] ho finito di chiudere i cassieri\n");
    #endif
    free(queueSize);
    free(cassieri);
    free(var);
    free(threadClients);
    free(threadCashiers);

    return NULL;
}
