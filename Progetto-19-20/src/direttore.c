//#define _POSIX_C_SOURCE  200112L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include "direttore.h"
#include "cliente.h"
#include "cassiere.h"
#include "utility.h"

int *queueSize;
int threshold1 , threshold2;
int count = 0;
volatile sig_atomic_t notFinishManager = 1;
pthread_mutex_t mtx;
threadCassiere *cassieri;
var_client_manager *vcm;
struct vcmAndCashiers *var;
pthread_t *threadCashiers;
pthread_t *threadClients;

// move the client in the cashier's queue,
// in other cashiers randomly
void moveClient(int index){
    queue client;
    queue aux = cassieri[index].head;
    while( aux != NULL){
        client = aux;
        //printf("prodotti comprati: %d\n", client->buyProducts);
        client->next = NULL;
        /*client.buyProducts = aux->buyProducts;
        client.served = 0;
        client.supermarketTime = aux->supermarketTime;
        client.queueTime = aux->queueTime; // come si calcola?
        client.changedQueue = aux->changedQueue + 1;
        client.lock = aux->lock;
        client.myTurn = aux->myTurn;
        client.next = NULL;*/
        insertInCashiersQueue(&client, vcm, cassieri);

        aux = aux->next;
    }
    cassieri[index].head = NULL;
    cassieri[index].tail = NULL;
}

// close the cashier with bigger index, and
// move the clients in queue on other cashiers.
// if numbers of cashiers are > 1
void closeCashier(){
    int index, status;
    //printf("                sto cercando di chiudere una cassa\n");
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


    pthread_mutex_unlock(&(cassieri[index].lock));
    pthread_mutex_unlock(&(vcm->mtx));

    deleteNotifyTimer(&cassieri[index]);

    //printf("QUI\n");
    //pthread_detach(cassieri[index].reg);
    pthread_join(cassieri[index].reg, (void*)&status);
    /*if(status != 0){
        perror("Error pthread join cashier");
        exit(EXIT_FAILURE);
    }*/

    /*if( pthread_cancel(cassieri[index].sup) != 0){
        perror("Error cancel thread support ");
        exit(EXIT_FAILURE);
    }
    if( pthread_cancel(cassieri[index].reg) != 0){
        perror("Error cancel thread cashier");
        exit(EXIT_FAILURE);
    }*/
    printf("            Cassiere %d chiuso\n", index+1);
}

// open the cashier with smaller index, if
// numbers of cashiers are < K
void openCashier(){
    int index;
    //printf("            sto cercando di aprire un nuova cassa\n");
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
    //pthread_detach(threadCashiers[index]);
    printf("        Aperto nuovo cassiere: %d\n", index+1);
    //pthread_mutex_unlock(&(vcm->mtx));

}

// check the thresholds, and if the values are not respected
// open or close the cashiers
void checkCashiers(){
    int numRegOneClient = 0;
    //printf("           sono il direttore e sto controllando\n");

    for(int i=0; i<vcm->openCashiers; i++){
        if( queueSize[i] <= 1 )
            numRegOneClient++;
        if(S1 == numRegOneClient){
            //printf("ALMENO %d CASSE HANNO 1 CLIENTE IN CODA\n", S1);
            closeCashier();
            return;
        }
        if( queueSize[i] >= S2){
            //printf("LA CASSA %d, HA %d CLIENTI IN CODA\n", i+1, queueSize[i]);
            openCashier();
            return;
        }
    }
}

// this function is used by cashier to notify the manager
void updateCashiers(int qSize, int index){
    //pthread_mutex_lock(&(mtx));
    //printf("notify manager from reg %d\n", index);
    pthread_mutex_lock(&(vcm->mtx));
    count++;
    queueSize[index] = qSize;

    if( vcm->openCashiers == count && !vcm->finish){
        count = 0;
        //printf("CONTROLLO\n");
        pthread_mutex_unlock(&(vcm->mtx));
        checkCashiers();
        //pthread_mutex_unlock(&(mtx));
        //printf("La cassa %d ha in coda %d clienti\n",index+1, qSize);
        return;
    }


    pthread_mutex_unlock(&(vcm->mtx));
    //printf("La cassa %d ha in coda %d clienti\n",index+1, qSize);
    //pthread_mutex_unlock(&(mtx));
}

// handler for SIGQUIT;
// terminate alla clients and cashiers immediatly
void handlerSigQuit(int sig){
    write(1, "catturato SIGQUIT\n", 20);
    pthread_mutex_lock(&(vcm->mtx));
    for(int i=0; i<C; i++){
        pthread_cancel(threadClients[i]);
    }
    for(int i=0; i<vcm->openCashiers; i++){
        pthread_cancel(threadCashiers[i]);
    }
    pthread_mutex_unlock(&(vcm->mtx));
    write(1, "supermarket close\n", 20);
    // effettuare tutte le varie free
    free(queueSize);
    free(cassieri);
    free(vcm);
    //free(var);
    free(threadClients);
    free(threadCashiers);
    _exit(EXIT_SUCCESS);
}

void handlerSigHup(int sig){
    write(1, "catturato SIGHUP\n", 20);
    notFinishManager = 0;
    pthread_mutex_lock(&(vcm->mtx));
    vcm->finish = 1;
    pthread_mutex_unlock(&(vcm->mtx));
    write(1, "supermarket is closing\n", 25);
    //_exit(EXIT_SUCCESS);
}

void clientExit(int index){
    threadClients[index] = 0;
    //printf("client %d exit, thread: %ld\n", index, threadClients[index]);
}

void waitClient(){
    pthread_mutex_lock(&(vcm->mtx));
    int count = vcm->numClient;
    printf("    Devo far uscire %d clients\n", count);

    while(count != 0){

    }

    pthread_mutex_unlock(&(vcm->mtx));
}

void *direttore(void *arg){
    vcm = arg;
    int i, status, thresholdC = C-E, add = 0, numExit = 0;
    //mtx = vcm->mtx; ??

    //pthread_detach(pthread_self());

    queueSize = malloc(sizeof(int)*K);
    for(i=0; i<K; i++)
        queueSize[i] = 0;
    cassieri = malloc(sizeof(threadCassiere)*K);
    var = malloc(sizeof(var_client_manager)*C);
    /*var.vcm = malloc(sizeof(var_client_manager));
    var.regs = malloc(sizeof(cassieri));
    var.vcm = vcm;
    var.regs = cassieri;*/

    struct sigaction sa;
    sigaction(SIGQUIT, NULL, &sa);
    sa.sa_handler = handlerSigQuit;
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = handlerSigHup;
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);

    // at start 1 register open
    vcm->openCashiers = 1;
    printf("Cassieri aperti: %d\n", vcm->openCashiers);

    // create first register
    threadCashiers = malloc(sizeof(pthread_t)*K);
    threadClients = malloc(sizeof(pthread_t)*C);
    cassiere_create(&cassieri[0], productTime, intervalTime, 0);
    if( pthread_create(&threadCashiers[0], NULL, &cassiere_work,(void*) &(cassieri[0])) != 0){
        perror("Error create register ");
        exit(EXIT_FAILURE);
    }
    //pthread_detach(threadCashiers[0]);

    printf("Il supermercato apre con %d casse aperte\n", vcm->openCashiers);
    // create first C clients
    for( i=0; i<C; i++){

        //var.vcm = malloc(sizeof(var_client_manager));
        //var.regs = malloc(sizeof(cassieri));
        var[i].vcm = vcm;
        var[i].regs = cassieri;
        var[i].index = i;
        //printf("indice client %d\n", i);
        if( pthread_create(&threadClients[i], NULL, &client, (void*) &var[i]) != 0){
            perror("Error create client ");
            exit(EXIT_FAILURE);
        }
        //pthread_detach(threadClients[i]);
    }


    /*while(notFinishManager){
        // aspetta
    }*/

    while(notFinishManager){
        // crea nuovi clienti e si mette in attesa di essere
        // informato dai cassieri
        pthread_mutex_lock(&(vcm->mtx));

        // if numbers of client inside supermarket is less
        // than numbers of threshold then add new E client
        while( vcm->numClient > thresholdC)
            pthread_cond_wait(&(vcm->NotFullClient), &(vcm->mtx));
        printf("Creo %d nuovi clienti\n", E);
        vcm->numClient += E;
        /*for( i=vcm->numClient-1; i<(vcm->numClient - 1 + E); i++){
            printf("indice assegnamento nuovo cliente %d\n", i);
            if( pthread_create(&threadClients[i], NULL, &client, (void*) var) != 0){
                perror("Error create client ");
                exit(EXIT_FAILURE);
            }
            //pthread_detach(threadClients[i]);
        }*/
        i = 0;
        while(add < E && i<C){
            if(threadClients[i] != 0){
                //printf("index: %d, not 0: %ld\n",i, threadClients[i]);
                i++;
                continue;
            }
            //var.vcm = malloc(sizeof(var_client_manager));
            //var.regs = malloc(sizeof(cassieri));
            var[i].vcm = vcm;
            var[i].regs = cassieri;
            var[i].index = i;
            printf("indice assegnamento nuovo cliente %d\n", i);
            add++;
            if( pthread_create(&threadClients[i], NULL, &client, (void*) &var[i]) != 0){
                perror("Error create client ");
                exit(EXIT_FAILURE);
            }

        }
        if(add != E){
            printf("add: %d\n", add);
            perror("Error create new client");
            exit(EXIT_FAILURE);
        }
        add = 0;

        pthread_mutex_unlock(&(vcm->mtx));
    }

    // devo far uscire tutti i clienti
    //waitClient();
    pthread_mutex_lock(&(vcm->mtx));
    for(i=0; i<vcm->openCashiers; i++){
        printClient(cassieri[i].head, cassieri[i].index);
    }
    pthread_mutex_unlock(&(vcm->mtx));

    for(i=0; i<C; i++){
        if(threadClients[i] == 0)
            continue;
        printf("    indice client join %d\n", i);
        numExit++;
        pthread_join(threadClients[i], (void*) &status);
        //pthread_detach(threadClients[i]);
    }
    printf("Client usciti %d\n", numExit);
    //sleep(1);
    pthread_mutex_lock(&(vcm->mtx));
    printf("Persone ancora nel supermercato: %d, cassieri aperti %d\n", vcm->numClient, vcm->openCashiers);
    for(i=0; i<vcm->openCashiers; i++){
        deleteNotifyTimer(&cassieri[i]);         // mi da problemi
        //printf("deleteNotifyTimer finito\n");

        pthread_join(threadCashiers[i], (void*)&status);

        //pthread_mutex_unlock(&(cassieri[i].lock));
        printf("cassiere finito\n");

        /*if( status != 0){
            perror("Error pthread join");
            exit(EXIT_FAILURE);
        }*/
        //pthread_cond_signal(&(cassieri[i].notEmpty));
        /*if( pthread_join(threadCashiers[i], (void*)&status) != 0){
            perror("Error pthread join");
            exit(EXIT_FAILURE);
        }*/
        /*pthread_mutex_lock(&(cassieri[i].lock));
        cassieri[i].notFinish = 0;
        pthread_mutex_unlock(&(cassieri[i].lock));
        pthread_join(cassieri[i].sup, (void*) &status);*/
        /*pthread_mutex_lock(&(cassieri[i].lock));
        pthread_detach(cassieri[i].sup);
        pthread_detach(threadCashiers[i]);
        pthread_mutex_unlock(&(cassieri[i].lock));*/

        //pthread_cancel(threadCashiers[i]);
    }
    pthread_mutex_unlock(&(vcm->mtx));
    printf("SONO IL DIRETTORE: STO TERMINANDO\n");
    //free(queueSize);
    //free(cassieri);
    //free(vcm);
    free(var);
    free(threadClients);
    free(threadCashiers);
    //exit(EXIT_SUCCESS);
    //pthread_detach(pthread_self());

    pthread_exit(NULL);
    //return NULL;
}
